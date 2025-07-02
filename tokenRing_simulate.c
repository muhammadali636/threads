//tokenRing_simulate.c
//Name: Muhammad Ali

/*
 * The program simulates a Token Ring LAN by forking off a process
 * for each LAN node, that communicate via shared memory, instead
 * of network cables. To keep the implementation simple, it jiggles
 * out bytes instead of bits.
 *
 * It keeps a count of packets sent and received for each node.
 */
#include <stdio.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

#include "tokenRing.h"
#define TOKEN_AVAILABLE 0x00 
#define TOKEN_UNAVAILABLE 0x01  


/*
 * This function is the body of a child process emulating a node.
 */
void
token_node(control, num)
	struct TokenRingData *control;
	int num;
{

    int rcv_state = TOKEN_FLAG, not_done=1, sending = 0, len, receivedIndexPosition = 0, received = 0; 
    unsigned char byte;
    

    /*
	 * If this is node #0, start the ball rolling by creating the
	 * token.
	 */
    if (!num){send_byte(control, num, TOKEN_AVAILABLE);} //SEND IF AVAILABLE, if num is 0. 

    /*
	 * Loop around processing data, until done. WE break out once termination is flagged
	 */
    while (not_done) {
        //check term flag before wainting on any semaphor
        if (control ->  shared_ptr-> node[num].terminate == 1) {

#ifdef DEBUG
            fprintf(stderr, "TERMINATING NODE (First Check): %d\n", num);
#endif
            //signal all them semaphores to mak sre term (RUN TOKENSIM 10000 times make it consistent). Note: Workign after assignment due? Server issues? might be redundant.
            SIGNAL_SEM(control, CRIT);
            for(int i = 0; i < N_NODES; i++){
                SIGNAL_SEM(control, EMPTY(i));
                SIGNAL_SEM(control, FILLED(i));
                SIGNAL_SEM(control, TO_SEND(i));
            }
            break;
        }


        //If sending is ON --> the node enters a critical section to send packet (SAFE).
        if (sending==1) 
        {
            WAIT_SEM(control, CRIT);
            send_pkt(control, num);
            SIGNAL_SEM(control, CRIT);

            //if the token is setreset the sending flag and update the rcv state before getting a byte
            if (control->snd_state == TOKEN_FLAG){  
                rcv_state = TOKEN_FLAG;
                sending = 0;
            }
        }
        //otherwise rcv a bite.
        byte = rcv_byte(control, num);

        //check termination flag after receiving byte
        if (control-> shared_ptr -> node[num].terminate == 1) {

#ifdef DEBUG
            fprintf(stderr, "terminating the node (second): %d\n", num);
#endif
            break;
        }
        
        /*
		 * Handle the byte, based upon current state.
		 */
        //STATE MACHINA
        switch (rcv_state) {
            case TOKEN_FLAG:
                //if sending is active then change state to TO

                if (sending != 0) {rcv_state = TO;} 
                else if (byte != TOKEN_AVAILABLE) {
                    //not token assume data pkt start
                    send_byte(control, num, byte);
                    rcv_state = TO;
                } 
                else if (control-> shared_ptr->node[num].to_send.length > 0) {
                    //if a pkt is waiting, switch state.
                    rcv_state = TO;
                    sending = 1;
                } 
                 //else forwrd token 
                else {send_byte(control, num, TOKEN_AVAILABLE);}
                break;

            case TO:
                //in TO state if not sending --> FORWARD the byte and update count 
                if (!sending){ //sending == 0.
                    if (byte == num) {
                        control ->shared_ptr->node[num].received++;
                    }
                    send_byte(control, num, byte); //forward byte
                }
                rcv_state = FROM;  //change state to FROM
                break;

            case FROM:
                //in FROM state check if byte matches node number to flag reception
                if (byte == num) {received = 1;}
                if (!sending){send_byte(control, num, byte); }//orward the byte
                rcv_state = LEN; //change state to LEN
                break;

            case LEN:
                //in LEN state if not sending then forward the byte and set length
                if (!sending){
                    send_byte(control, num, byte);
                    len = byte;
                    receivedIndexPosition = 0;
                }
                rcv_state = DATA; //change state to DATA
                break;

            case DATA:
                //in DATA state if not sending then forward the byte
                if (!sending) {send_byte(control, num, byte);}
                receivedIndexPosition++;
                //if all data bites received then update count and forward token if needed THEN reset state
                if (receivedIndexPosition != len) {break;}
                if (received == 1){
                    control -> shared_ptr->node[num].received++;
                    send_byte(control, num, TOKEN_AVAILABLE);
                }
                sending = 0;
                receivedIndexPosition = 0;
                len = 0;
                rcv_state = TOKEN_FLAG;
                break;
        }
    }
}

/*
 * This function sends a data packet followed by the token, one byte each
 * time it is called.
 */
void
send_pkt(control, num)
    struct TokenRingData *control;
    int num;
{
    static int sndpos = 0, sndlen;
    
    struct data_pkt *packet = &(control-> shared_ptr->node[num].to_send);

    switch (control ->snd_state) {
        //if state is token flag then send and change state to TO
        case TOKEN_FLAG:
            send_byte(control, num,TOKEN_UNAVAILABLE);
            control -> snd_state = TO;
            break;

        //if state is TO then send destination and change state to FROM
        case TO:{
            unsigned char destination = packet -> to;
            send_byte(control, num, destination);
            control-> snd_state = FROM;
            break;
        }

        //if state is FROM then send source and change state to LEN
        case FROM: {
            unsigned char source = packet->from;
            send_byte(control, num, source);
            control -> snd_state = LEN;
            break;
        }


        //if state is LEN then get packet length and send it and change state to DATA
        case LEN:
            {
                sndlen = packet->length;
                unsigned char length_byte = (unsigned char)sndlen;
                send_byte(control, num, length_byte);
                control -> snd_state = DATA;
            }
            break;

        //if state is DATA then send a data byte and increment send position
        case DATA: {
            unsigned char current_byte = packet -> data[sndpos];
            sndpos++;
            send_byte(control, num, current_byte);
            //if all data bytes sent then change state to DONE
            if (sndpos == sndlen){ control -> snd_state = DONE; }
        }
        break;

        case DONE: {
            int sentCount = control->shared_ptr->node[num].sent;  //cur count
            sentCount++;     //increment count
            control->shared_ptr->node[num].sent = sentCount; //upd  sent counter
            control->snd_state = TOKEN_FLAG;
            packet->length = 0; //clear pkt length and send token after finishing sending
            send_byte(control, num, TOKEN_AVAILABLE); //signal semaphore for next sending
            SIGNAL_SEM(control, TO_SEND(num));
            sndpos = 0; //reset send post for next send
            break;
        }
    }
}

/*
 * Send a byte to the next node on the ring.
 */
void
send_byte(control, num, byte)
	struct TokenRingData *control;
	int num;
	unsigned byte;
{
    //NEW:
    int next; //next node 
    next = (num + 1) % N_NODES; 
    WAIT_SEM(control, EMPTY(next)); //wait until next node buffer empty
    control -> shared_ptr -> node[next].data_xfer = byte;
    SIGNAL_SEM(control, FILLED(next)); //signal next node bfufer full.
}

/*
 * Receive a byte for this node.
 */
unsigned char
rcv_byte(control, num)
	struct TokenRingData *control;
	int num;
{
    unsigned char byte_to_return;
    //NEW:
    WAIT_SEM(control, FILLED(num)); //wait until THIS NODES buffer is FULL.
    byte_to_return = control -> shared_ptr -> node[num].data_xfer;
    SIGNAL_SEM(control, EMPTY(num)); //SIGNAL Buf EMPTY
    return byte_to_return;
} 