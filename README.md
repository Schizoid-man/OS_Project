# Race Condition Demonstration Project

This project demonstrates race conditions in a multi-threaded server environment and how to fix them using synchronization primitives.

## Project Overview

- **Client**: A simple program that connects to the server, sends a word, and receives the reversed word back.
- **Server (Two Versions)**:
  - `server_race.c`: Demonstrates race conditions with shared resources (buffer) without synchronization
  - `server_fixed.c`: Fixes the race condition using a semaphore

## Components

- `client.c` - Client implementation that connects to a server
- `run_clients.c` - Launches 100 client processes to stress-test the servers
- `server_race.c` - Server with race condition
- `server_fixed.c` - Fixed server with proper synchronization
- `test_race_condition.sh` - Script to test both servers

## How to Use

1. Run the test script to compile and test both servers:

```bash
./test_race_condition.sh
```

This will:
- Compile all C files
- Test the race condition server with 100 clients
- Test the fixed server with 100 clients
- Analyze logs for race conditions

2. Check the generated log files for detailed information:
- `clients.log` - Client operations log
- `server_race.log` - Race condition server log
- `server_fixed.log` - Fixed server log

## Race Condition Explanation

The race condition occurs in `server_race.c` because:
1. Multiple threads share the same buffer (`shared_buffer`)
2. There's no synchronization between threads accessing this buffer
3. Under high load with 100 concurrent clients, race conditions can naturally occur

In contrast, `server_fixed.c`:
1. Uses a semaphore to ensure only one thread can access the critical section at a time
2. Uses local buffers for each thread instead of a shared buffer

## Expected Results

When running the test script:
- The race condition server may produce incorrect results (different words getting mixed up)
- The fixed server should produce correct results (each client gets the correct reversed word)

The script analyzes logs to detect if race conditions occurred during the run by checking if there are duplicate responses, which would indicate that multiple client requests were processed incorrectly. 
