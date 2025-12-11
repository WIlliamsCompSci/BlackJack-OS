# Networked Blackjack (C, POSIX sockets)

## Overview
This project implements a turn-based multiplayer Blackjack game with a **server** (dealer) and multiple **clients** (players). It is written in **C (C11)** and uses **POSIX/Berkeley sockets** (TCP) for networking and **pthreads** for concurrency.  
The server and clients communicate using a **4-byte length-prefixed ASCII protocol**, ensuring reliable message framing and avoiding partial-read issues.

---

## Features
- Multiplayer support (configurable up to 6 players)  
- Server manages deck, shuffling, dealing, player turns, and dealer logic  
- Blackjack rules: Ace counts as 1 or 11 to maximize hand value ≤ 21  
- Turn timeouts and disconnect handling  
- Length-prefixed framed protocol for all messages  
- Per-client handler threads (pthreads) on the server  
- Text-based CLI clients supporting `HIT`, `STAND`, `QUIT`, and `CHAT`

---

## Files
- `server.c` — server implementation  
- `client.c` — client implementation  
- `common.h`, `protocol.h`, `deck.h` — shared headers (types, protocol tokens, deck helpers)  
- `Makefile` — build rules for the project  
- `README.md` — this file  

---

## Prerequisites (software & environment)
Ensure you have a POSIX-like environment (Linux, macOS, WSL) and these tools installed:

- `gcc` (supporting C11) — e.g., `gcc --version`  
- `make` — `make --version`  
- `bash` (for scripts or examples)  
- Optional but recommended: `valgrind` for memory checking (`valgrind --version`)

If you are on Windows, use WSL (Windows Subsystem for Linux) or a Linux VM for easiest compatibility.

---

## Build: Step-by-step (detailed)

This section explains exactly how to compile the project from scratch and explains what each step does.

## 1. Open a terminal in the project root
Open a terminal (bash, zsh, or similar) and `cd` into the folder that contains `Makefile`, `server.c`, and `client.c`.


cd /path/to/your/project


### 2. Clean any previous builds

This removes old binaries or object files:

make clean

## 3. Build the Project
Compile both the server and client using the Makefile:

make

This command:
compiles all .c files
links pthreads
produces two executables: server and client

When successful, your directory now contains:
server*
client*

## Running the Server
Choose any available port (example: 12345) and run:
./server 12345

Expected output (your version may vary):
Server listening on port 12345
Waiting for players...

Running More Clients (Optional)
Open additional terminals:
./client 127.0.0.1 12345 Bob
./client 127.0.0.1 12345 Charlie

## Playing the Game
Clients respond with commands:

- HIT — request another card
- STAND — end your turn
- CHAT <message> — send chat text to all players
- QUIT — disconnect from server

The server sends messages such as:
DEAL H10 SA
YOUR_TURN
REQUEST_ACTION
CARD D5
RESULT WIN 20 18

## Stopping Client(s)
Inside a client terminal type:
QUIT
Or simply close the terminal window.

## Stopping the Server
press:
Ctrl + C
