Modelling a Token Ring using Semaphores
Muhammad Ali 

run 'make' and then './tokensim <number of packets>' to run the simulation. If you want to see the Debug statements, uncomment #DDEBUG in makefile and make again.

INTRO:
A Token Ring is a type of LAN where several computers are connected in a circle and communicate. 
A small data packet called a token is passed around the ring; however, nodes must wait for the token before it can send its data. 
Only nodes that have a token are allowed to transmit data and when a node receives the token, it sends its packet and then passes the token along. 

PROGRAM DESIGN:
1. Setting up the TokenRing (TokenRing_setup.c)
First we have to setup the token ring.
The parent process prepares everything needed for the simulation. 
The program first seeds the random number generator so that data packets are generated randomly and then the program creates a shared memory area that all nodes use to communicate. 
The parent process also creates semaphores that control access to the shared memory. 
For each node, semaphores are set up to show when its buffer is empty, filled, or ready to send data. 
Each node’s data is initialized by clearing its buffer, setting the token flag to zero, and resetting the counters for packets sent and received.

2. Simulating (TokenRing_simulate.c)
Now for the simulation.
The parent process forks a child process for each node. 
Node 0 starts by sending out the available token (0x00). 
Each node then enters a loop where it waits for a byte from the previous node, processes it using a state machine, and forwards it to the next node. 
When a node wants to send data, it waits for available token and once it gets the token, it marks node as unavailable (0x01) and sends its data packet. 
The packet contains a token flag, destination, source, length, and a load of up to 250 bytes (see tokenRing.h).
After sending its packet, the node resets the token back to available (0x00).
Once done, the parent process signals all the nodes to terminate by setting a termination flag and sending a final token to each node. 
The parent then waits for all child processes to die, which after the parent prints the receive/send numbers of packets for each node and cleans up the shared memory and semaphores.

Conclusion:
Overall, this system is great because it helps avoid data collisions and makes sure every node gets a turn to communicate. 
I was stuck initially but I eventually fixed it. One was deadlock, which I fixed by using the right signals. 
Another was termination of all children (stuck) and randomness of the program working (for example if i run ./tokensim 10 twice, it worked once and fails the other time, getting stuck on children termination phase).
What was happening was: some nodes might terminate earlier than others so the rest of nodes that didn't terminate yet might be stuck. 
I fixed this by sending a burst of signals to all the semaphores before exiting, which released any waiting nodes so they could finish their work and exit. 
