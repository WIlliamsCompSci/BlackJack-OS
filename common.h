// common.h
#ifndef COMMON_H
#define COMMON_H

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <inttypes.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <fcntl.h>

#define MAX_NAME_LEN 32
#define MAX_PAYLOAD 1024
#define MAX_PLAYERS 6
#define DEFAULT_PORT 12345
#define ACTION_TIMEOUT_SEC 30  // seconds to wait for player's action

// helper prototypes:
ssize_t write_all(int fd, const void *buf, size_t count);
ssize_t read_all(int fd, void *buf, size_t count);
int send_msg(int fd, const char *msg);
int recv_msg(int fd, char **out_buf); // allocates *out_buf, caller must free

#endif // COMMON_H
