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
- `Makefile` — build rules for the project (Linux/macOS/WSL)  
- `build.ps1` — PowerShell build script for Windows  
- `README.md` — this file  

---

## Prerequisites (software & environment)
Ensure you have a POSIX-like environment (Linux, macOS, WSL) and these tools installed:

- `gcc` (supporting C11) — e.g., `gcc --version`  
- `make` — `make --version` (or use `build.ps1` on Windows PowerShell)  
- `bash` (for scripts or examples)  
- Optional but recommended: `valgrind` for memory checking (`valgrind --version`)

**Windows users:** You can use:
- **PowerShell:** Use the provided `build.ps1` script (requires `gcc` installed, e.g., via MinGW)
- **WSL:** Windows Subsystem for Linux for full POSIX compatibility
- **Linux VM:** For easiest compatibility

---

## HOW TO ACTUALLY RUN THE GAME
LET ME SHOW YOU HOW TO RUN THE GAME AND HOW TO PLAY THE GAME

## Step 1: Open a Terminal
A terminal is like a chat box for the computer.

## Step 2: Go to the folder where the files are
Type this:
cd your-folder-name-here
(Replace the folder name with the one where your project is.)
When you’re in the right place, you’ll see these files:
server.c  client.c  deck.h  protocol.h  common.h  Makefile

## Step 3: Build (compile) the game
Building = telling the computer to "make the game ready to play."

**On Linux/macOS/WSL:**
```
make clean
make
```

**On Windows PowerShell:**
```
.\build.ps1 clean
.\build.ps1
```

If it works, new files will appear:
server
client
These are the actual game programs.

## Step 4: Start the Dealer (the server)
Now you pretend to be the dealer.
Open Terminal #1 and type:
./server 12345
This starts the game dealer on port 12345 (just a number everyone connects to).
If it worked, the computer will say something like:
Server listening on port 12345
Waiting for players...
GREAT! The dealer is ready.

## Step 5: Start a Player (you)
Open Terminal #2 (a new window).
Type:
./client 127.0.0.1 12345 Alice
This means:
You are Alice
You are connecting to your own computer (127.0.0.1)
Using port 12345 where the dealer is waiting
If it works, Alice joins the game!

## Step 6: Start a Player for Your Friend
Your friend does the same thing on Terminal #3:
./client 127.0.0.1 12345 Bob
Now it’s:
Alice (you)
Bob (your friend)
Dealer (server)
Everyone is connected!

## Step 7: Play the Game 
Each player types one of these:

HIT	-- Give me another card
STAND -- I'm done, next player
CHAT hello -- Send messages to other players
QUIT -- Leave the game

The game automatically tells you:
When it’s your turn
What cards you get
If you bust
Who wins
You just type the words when the game asks.

## Step 8: Stopping the Game
To stop the player:
Type:
QUIT
or close the window.
To stop the dealer:
Go to Terminal #1 and press:
Ctrl + C


