// tokenRing_setup.c
// Name: Muhammad Ali
 
/*
 * The program simulates a Token Ring LAN by creating a thread
 * for each LAN node, that communicate via shared memory (now allocated
 * by malloc), instead of separate processes and shm. 
 * It keeps a count of packets sent and received for each node.
 */

 #include <stdio.h>
 #include <signal.h>
 #include <sys/time.h>
 #include <sys/ipc.h>    //sems
 #include <sys/sem.h>
 #include <sys/types.h>//sems
 #include <time.h>
 #include <stdlib.h>
 #include <unistd.h>
 #include <stdarg.h>
 #include <string.h>
 #include <errno.h>
 #include <pthread.h>   //pthreads
 
 #include "tokenRing.h"
 
 #define TOKEN_AVAILABLE     0x00 
 #define TOKEN_UNAVAILABLE   0x01  
 
//for sim
 struct thread_args {
     struct TokenRingData *control;
     int nodeNum;
 };
 
/* static thread handles & args for join */
static pthread_t threads[N_NODES];
static struct thread_args targs[N_NODES];
 
 void *threadTokenNode(void *args) {
     struct thread_args *t = (struct thread_args *)args;
     token_node(t->control, t->nodeNum);
     pthread_exit(NULL);
     return NULL;  
 }
 

 /*
  * The main program creates the shared memory region(malloc instead of shm) creates the semaphore set and:
  * This process generates packets at random and inserts them in
 * the to_send field of the nodes. When done it waits for each process
 * to be done receiving and then tells them to terminate and waits
 * for them to die and prints out the sent/received counts.
  */
 
 struct TokenRingData *
 setupSystem()
 {
     register int i;
     struct TokenRingData *control;
 
     control = (struct TokenRingData *) malloc(sizeof(struct TokenRingData));
     control->snd_state = TOKEN_FLAG;
 
     /*
      * Seed the random number generator.
      */
     srandom(time(0));
 
     /*
      * Instead of shmget + shmat, just allocate with malloc.
        * Create the shared memory region.
      */
     control->shared_ptr = (struct shared_data *) malloc(sizeof(struct shared_data));
     if (control->shared_ptr == NULL) {
         fprintf(stderr, "Failed to malloc shared_data.\n");
         goto FAIL;
     }
 

    //dont need shmid shmget  

 	/*
 	 * Now, create the semaphores, by creating the semaphore set.
 	 * Under Linux, semaphore sets are stored in an area of memory
 	 * handled much like a shared region. (A semaphore set is simply
 	 * a bunch of semaphores allocated to-gether.)
 	 */
     control->semid = semget(IPC_PRIVATE, NUM_SEM, 0600);
     if (control->semid < 0) {
         fprintf(stderr, "Can't create semaphore set\n");
         goto FAIL;
     }
 
    /*
 	 * and initialize them.
 	 * Semaphores are meaningless if they are not initialized.
 	 */
     for (i = 0; i < N_NODES; i++) {
         INITIALIZE_SEM(control, FILLED(i), 0);
         INITIALIZE_SEM(control, TO_SEND(i), 1);
         INITIALIZE_SEM(control, EMPTY(i), 1);
     }
     INITIALIZE_SEM(control, CRIT, 1);

 	/*
 	 * And initialize the shared data
 	 */
     for (i = 0; i < N_NODES; i++) {
         control->shared_ptr->node[i].data_xfer = 0;
         control->shared_ptr->node[i].to_send.token_flag = 0;
         control->shared_ptr->node[i].to_send.to = 0;
         control->shared_ptr->node[i].to_send.from = 0;
         control->shared_ptr->node[i].to_send.length = 0;
         control->shared_ptr->node[i].sent = 0;
         control->shared_ptr->node[i].received = 0;
         control->shared_ptr->node[i].terminate = 0;
         bzero(control->shared_ptr->node[i].to_send.data, MAX_DATA);
     }
 
     return control;
 
 FAIL:
     free(control);
     return NULL;
 }
 
 
 /*
  * Instead of forking child processes, we'll create threads.
  * The rest of the packet generation logic is unchanged.
  */
 int
 runSimulation(struct TokenRingData *control, int numberOfPackets)
 {
     int i, num, to;
     // arr of thread IDs and args are static above
 
     //create thread for each ring node
     for (i = 0; i < N_NODES; i++) {
         targs[i].control = control;
         targs[i].nodeNum = i;
         pthread_create(&threads[i], NULL, threadTokenNode, &targs[i]);
     }
 
    /*
 	 * Loop around generating packets at random.
 	 * (the parent)
 	 */
     for (i = 0; i < numberOfPackets; i++) {
         num = random() % N_NODES;
         WAIT_SEM(control, TO_SEND(num));
         WAIT_SEM(control, CRIT);
 
         if (control->shared_ptr->node[num].to_send.length > 0)
             panic("to_send filled\n");
 
         control->shared_ptr->node[num].to_send.token_flag = '0';
         do {
             to = random() % N_NODES;
         } while (to == num);
 
         control->shared_ptr->node[num].to_send.to = (char)to;
         control->shared_ptr->node[num].to_send.from = (char)num;
         control->shared_ptr->node[num].to_send.length = (random() % MAX_DATA) + 1;
 
         SIGNAL_SEM(control, CRIT);
     }
 
     return 1;
 }
 

 int
 cleanupSystem(struct TokenRingData *control)
 {
     int i;
     union semun zeroSemun;
     bzero(&zeroSemun, sizeof(zeroSemun));
 
    /*
 	 * Now wait for all nodes to finish sending and then tell them
 	 * to terminate.
 	 */
     for (i = 0; i < N_NODES; i++) {
         WAIT_SEM(control, TO_SEND(i));
     }
     WAIT_SEM(control, CRIT);
     for (i = 0; i < N_NODES; i++) {
         control->shared_ptr->node[i].terminate = 1;
     }
     SIGNAL_SEM(control, CRIT);
 
     
    /*
 	 * Wait for the node processes to terminate.
 	 */
     for (i = 0; i < N_NODES; i++) {
         send_byte(control, i, TOKEN_AVAILABLE);
     }
 
     /*
      * wait on each thread
      */
     for (i = 0; i < N_NODES; i++) {
         pthread_join(threads[i], NULL);
     }

     /*
      * print results
      */
     for (i = 0; i < N_NODES; i++) {
         printf("Node %d: sent=%d received=%d\n",
                i,
                control->shared_ptr->node[i].sent,
                control->shared_ptr->node[i].received);
     }
 
     free(control->shared_ptr);     //Free shared memory region  

     semctl(control->semid, 0, IPC_RMID, zeroSemun); //destroy sem set
 
     free(control); //free main control
 
     return 1;
 }
 
/*
 * Panic: Just print out the message and exit. 
 */
//??????
 void
 panic(const char *fmt, ...)
 {
     va_list vargs;
     va_start(vargs, fmt);
     (void)vfprintf(stdout, fmt, vargs);
     va_end(vargs);
     exit(5);
 }
