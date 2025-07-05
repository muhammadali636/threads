CIS3110 Assignment 3: TokenRing Now with Threads and Processes
Muhammad Ali (1115336)

run 'make' and then './tokensim <number of packets>' to run the simulation. If you want to see the Debug statements, uncomment #DDEBUG in makefile and make again.

INTRO:
A Token Ring is a type of LAN where several computers (or nodes) are connected in a circle and communicate by passing around a small data packet (pkt) called a token. Only node holding token is allowed to transmit data, which helps avoid collisions and ensures that each node eventually gets a turn to communicate.

In this assignment, we simulate a token ring using threads instead of forking separate processes. We still use semaphores to coordinate access to the shared data, but now each node runs as a thread within the same process. The large shared region is allocated via malloc, which all threads share.

PROGRAM DESIGN:
1. Setting up TokenRing (TokenRing_setup.c):
First we seed random number generator so that data pkts are generated randomly.
Next we allocate a block of shared memory via malloc (instead of using shmget/shmat). This block contains data structures for all nodes (as defined in tokenRing.h).
Then set of semaphores is made. Each node has semaphores that just shows whether its buffer is empty or filled, and whether it is ready to send data. 
In the end, for each node’s data, the buffer and counters are initialized to 0 including token flag, sent and received counters, and terminating flags.

2. Simulating (TokenRing_simulate.c):
Instead of forking child processes, we make thread for each node. Parent thread makes N_NODES threads using pthread_create. Each thread runs token_node, which does the node’s work in a loop.
Node 0 begins by sending out available token (0x00).
Each node waits for a byte from previous node using semaphores (rcv_byte), processes that byte via a small state machine, and forwards it to next node using send_byte.
If a node has data to send, it waits for available token and then sends a pkts. Pkts includes a token flag, destination, source, length.
After sending its own data, node restores token to available (0x00) and continues passing it around the ring.
At the same time, a main thread randomly fills each node’s to_send pkts field. When it has queued all required pkts, it sets terminate flag on each node  and sends out a final token so that all nodes can exit their loops.

3. Termination & Cleanup:
After main thread finishes generating pkts, it signals all the nodes to terminate by setting a termination flag and sending a final token to each.
Each node sees this termination flag and later exits loop. Then the main thread calls pthread_join on all node threads, ensuring they have all completed.
Finally, program prints out sent/received counts for each node and cleans up by destroying the semaphores and freeing shared memory block allocated by malloc.

Conclusion:
Using threads (instead of processes) and semaphores allows us to simulate a token ring with minimal overhead—everything is already in one memory space.
The system avoid data collisions by letting only the node with the token send its data.
I came through some issues with deadlock in testing but fixed them by carefully sending the right semaphore signals. 
Another challenge was ensuring each thread exits properly when signaled; sometimes, 
one node’s early exit could block others. I fixed this by sending extra signals (like passing the token) 
so that any waiting threads break out of their loops, increment their counters if needed, and then exit. 
If you run ./tokensim 10 multiple times, it now completes successfully, consistently, and prints the final stats for each node.







