# Networked Blackjack (C, POSIX sockets)

## Overview
This project implements a turn-based multiplayer Blackjack game with a **server** (dealer) and **clients** (players). It uses **C (C11)**, **POSIX/Berkeley sockets** (TCP), and **pthreads** for concurrency. Communication uses a **4-byte length-prefixed ASCII framing** for robustness.

## Features
- Multiplayer (configurable up to 6 players)
- Server deals cards and enforces Blackjack rules (Ace = 1 or 11)
- Turn-based per-player actions with timeouts
- Robust framed protocol and per-client threads
- Text-based CLI clients (HIT / STAND / QUIT / CHAT)

## Files
- `server.c` — server implementation
- `client.c` — client implementation
- `common.h`, `protocol.h`, `deck.h` — shared headers (embedded content)
- `Makefile` — build targets
- `README.md`, `ARCHITECTURE.md`, `DIAGRAMS.txt`, `DEMO_SCRIPT.txt` — documentation

## Build
Requires a POSIX environment and `gcc`.

```sh
make
