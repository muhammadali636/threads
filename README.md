# TokenRing Modelled with Threads  

To run the simulation:

```bash
make
./tokensim <number of packets>
```

If you want to see the debug statements, uncomment `-DDEBUG` in the `Makefile` and run `make` again.

---

## INTRO

A **Token Ring** is a type of LAN where several computers (or nodes) are connected in a circle and communicate by passing around a small data packet (pkt) called a **token**. Only the node holding the token is allowed to transmit data. This avoids collisions and ensures each node eventually gets a turn to communicate.

Here, we simulate a token ring using **threads**. Each node runs as a thread within the same process. We use **semaphores** to coordinate access to the shared data, which is allocated using `malloc`. All threads share this memory space without the need for `shmget`/`shmat`.

---

## PROGRAM DESIGN

### 1. Setting up TokenRing (`tokenRing_setup.c`)

- The random number generator is seeded to allow for random packet generation.
- A block of shared memory is allocated using `malloc`. This block contains:
  - Per-node structures like `to_send`, `sent`, `received`, and `terminate`.
- A set of **semaphores** is created using `semget`. Each node has:
  - `EMPTY(n)` — buffer is empty
  - `FILLED(n)` — buffer is filled
  - `TO_SEND(n)` — ready to send
- One global semaphore `CRIT` is used for critical sections.
- All node buffers and counters (`sent`, `received`, `to_send`, etc.) are initialized to zero.

---

### 2. Simulating (`tokenRing_simulate.c`)

- The main thread creates `N_NODES` threads using `pthread_create`, each running the `token_node` function.
- **Node 0** starts the simulation by sending out the available token (`0x00`).
- Each node:
  - Waits for a byte from the previous node using `rcv_byte`
  - Processes the byte using a **state machine**
  - Forwards it using `send_byte`

#### State Machine Overview:

- `TOKEN_FLAG`: Check if we can send a packet or forward the token.
- `TO`: Process destination address.
- `FROM`: Process source address.
- `LEN`: Read packet length.
- `DATA`: Process or forward the data payload.
- `DONE`: Reset state after finishing a packet.

- If a node has data to send and the token is available:
  - It enters the critical section (`CRIT` semaphore)
  - Constructs a packet (`to_send`) and transmits it byte-by-byte
  - Sends the token again after the packet is done

- The **main thread** randomly assigns packets to nodes by filling their `to_send` fields. After all packets are assigned, it:
  - Sets the `terminate` flag on each node
  - Sends a final token to ensure each thread gets a chance to exit

---

### 3. Termination & Cleanup

- Each thread checks the `terminate` flag both **before and after** waiting on a semaphore to avoid deadlocks.
- When a thread detects its termination flag, it breaks out of the main loop and exits via `pthread_exit`.
- The main thread:
  - Sends a final token to each node
  - Calls `pthread_join()` on all threads
  - Frees shared memory using `free()`
  - Removes the semaphore set using `semctl(IPC_RMID)`

- Finally, the program prints stats like:

```text
Node 0: sent=3 received=4
Node 1: sent=1 received=3
...
```

---

## Conclusion

Using **threads** and **semaphores** allows us to simulate a Token Ring with minimal overhead, since all threads run in a single memory space.

- Only the node with the token can send data, avoiding collisions.
- All communication occurs through a shared memory buffer and synchronized semaphores.
- Deadlocks were encountered during development, especially around semaphores and token passing. These were resolved by carefully ordering semaphore signals.
- A major challenge was ensuring **graceful termination**. Threads could block if another exited early. This was resolved by sending final tokens and checking `terminate` flags aggressively.

If you run:

```bash
./tokensim 10
```

multiple times, the program completes **consistently and successfully**, printing accurate final statistics for each node.

---
