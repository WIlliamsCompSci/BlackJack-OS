// client.c
#include "common.h"
#include "protocol.h"
#include <strings.h>  // for strcasecmp, strncasecmp

volatile int client_running = 1;
int sockfd = -1;
pthread_t reader_thread;

ssize_t write_all(int fd, const void *buf, size_t count) {
    const uint8_t *p = buf;
    size_t left = count;
    while (left > 0) {
        ssize_t n = write(fd, p, left);
        if (n <= 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        left -= (size_t)n; p += n;
    }
    return (ssize_t)count;
}

ssize_t read_all(int fd, void *buf, size_t count) {
    uint8_t *p = buf;
    size_t left = count;
    while (left > 0) {
        ssize_t n = read(fd, p, left);
        if (n < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        if (n == 0) return (ssize_t)(count - left);
        left -= (size_t)n; p += n;
    }
    return (ssize_t)count;
}

int send_msg(int fd, const char *msg) {
    uint32_t len = (uint32_t)strlen(msg);
    uint32_t nlen = htonl(len);
    if (write_all(fd, &nlen, sizeof(nlen)) < 0) return -1;
    if (write_all(fd, msg, len) < 0) return -1;
    return 0;
}

int recv_msg(int fd, char **out_buf) {
    uint32_t nlen;
    ssize_t r = read_all(fd, &nlen, sizeof(nlen));
    if (r <= 0) return -1;
    uint32_t len = ntohl(nlen);
    if (len > MAX_PAYLOAD) return -1;
    char *buf = malloc(len+1);
    if (!buf) return -1;
    if (read_all(fd, buf, len) != (ssize_t)len) { free(buf); return -1; }
    buf[len] = '\0';
    *out_buf = buf;
    return (int)len;
}

void *reader_func(void *arg) {
    (void)arg;
    while (client_running) {
        char *msg = NULL;
        int rc = recv_msg(sockfd, &msg);
        if (rc <= 0) {
            printf("Disconnected from server or read error\n");
            client_running = 0;
            break;
        }
        // print protocol tokens neatly
        if (strncmp(msg, MSG_WELCOME, strlen(MSG_WELCOME)) == 0) {
            printf("[SERVER] %s\n", msg);
        } else if (strncmp(msg, MSG_DEAL, strlen(MSG_DEAL)) == 0) {
            // DEAL <card1> <card2>
            printf("[DEAL] %s\n", msg + strlen(MSG_DEAL) + 1);
        } else if (strncmp(msg, MSG_YOUR_TURN, strlen(MSG_YOUR_TURN)) == 0) {
            printf("[SERVER] It's your turn.\n");
        } else if (strncmp(msg, MSG_REQUEST_ACTION, strlen(MSG_REQUEST_ACTION)) == 0) {
            printf("[SERVER] Requesting action. Type HIT or STAND then press Enter.\n");
        } else if (strncmp(msg, MSG_CARD, strlen(MSG_CARD)) == 0) {
            printf("[CARD] %s\n", msg + strlen(MSG_CARD) + 1);
        } else if (strncmp(msg, MSG_BUSTED, strlen(MSG_BUSTED)) == 0) {
            printf("[SERVER] You BUSTED!\n");
        } else if (strncmp(msg, MSG_RESULT, strlen(MSG_RESULT)) == 0) {
            printf("[RESULT] %s\n", msg + strlen(MSG_RESULT) + 1);
        } else if (strncmp(msg, MSG_BROADCAST, strlen(MSG_BROADCAST)) == 0) {
            // read next frame to get broadcast text (server sometimes sends separate frames)
            printf("[BROADCAST] "); // rest of message may come in another frame
            // if trailing text available in same frame, print
            // In our server implementation, we send BROADCAST then separate text; so try to read next message
            char *next = NULL;
            if (recv_msg(sockfd, &next) > 0) {
                printf("%s\n", next);
                free(next);
            } else {
                printf("\n");
            }
        } else if (strncmp(msg, MSG_ERROR, strlen(MSG_ERROR)) == 0) {
            printf("[ERROR] %s\n", msg + strlen(MSG_ERROR) + 1);
        } else {
            // unknown token: print raw
            printf("[SERVER] %s\n", msg);
        }
        free(msg);
    }
    return NULL;
}

void usage(const char *pname) {
    printf("Usage: %s <server_ip> <port> <player_name>\n", pname);
}

int main(int argc, char **argv) {
    if (argc < 4) {
        usage(argv[0]);
        return 1;
    }
    const char *server_ip = argv[1];
    int port = atoi(argv[2]);
    const char *player_name = argv[3];

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) { perror("socket"); return 1; }
    struct sockaddr_in srv;
    memset(&srv, 0, sizeof(srv));
    srv.sin_family = AF_INET;
    srv.sin_port = htons(port);
    if (inet_pton(AF_INET, server_ip, &srv.sin_addr) <= 0) { perror("inet_pton"); return 1; }
    if (connect(sockfd, (struct sockaddr*)&srv, sizeof(srv)) < 0) { perror("connect"); return 1; }

    // send JOIN
    char joinbuf[MAX_PAYLOAD];
    snprintf(joinbuf, sizeof(joinbuf), CMD_JOIN " %s", player_name);
    if (send_msg(sockfd, joinbuf) < 0) { perror("send"); return 1; }

    // spawn reader thread
    pthread_create(&reader_thread, NULL, reader_func, NULL);

    // main input loop
    char line[256];
    while (client_running) {
        if (!fgets(line, sizeof(line), stdin)) break;
        // trim newline
        char *nl = strchr(line, '\n');
        if (nl) *nl = '\0';
        // commands: HIT, STAND, QUIT, CHAT <message>
        if (strcasecmp(line, "HIT") == 0) {
            send_msg(sockfd, CMD_ACTION " HIT");
        } else if (strcasecmp(line, "STAND") == 0) {
            send_msg(sockfd, CMD_ACTION " STAND");
        } else if (strcasecmp(line, "QUIT") == 0) {
            send_msg(sockfd, CMD_QUIT);
            client_running = 0;
            break;
        } else if (strncasecmp(line, "CHAT ", 5) == 0) {
            char buf[MAX_PAYLOAD];
            snprintf(buf, sizeof(buf), CMD_CHAT " %s", line + 5);
            send_msg(sockfd, buf);
        } else {
            printf("Unknown command. Use HIT, STAND, QUIT, CHAT <message>\n");
        }
    }

    // cleanup
    close(sockfd);
    pthread_join(reader_thread, NULL);
    return 0;
}
