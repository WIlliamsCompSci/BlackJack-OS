CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -pthread -g
LDFLAGS =
SRCS = server.c
CLIENT_SRCS = client.c
TARGETS = server client

all: server client

server: server.c
	$(CC) $(CFLAGS) -o server server.c

client: client.c
	$(CC) $(CFLAGS) -o client client.c

clean:
	-rm -f server client *.o

.PHONY: all clean
