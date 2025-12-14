// server.c
#include "common.h"
#include "protocol.h"
#include "deck.h"
#include <stdarg.h>
#include <sys/time.h>

#define BACKLOG 10
#define RESHUFFLE_THRESHOLD 15

typedef enum {
    PLAYER_STATE_EMPTY = 0,
    PLAYER_STATE_CONNECTED,
    PLAYER_STATE_IN_GAME,
} PlayerState;

typedef enum {
    PLAYER_ACTION_NONE = 0,
    PLAYER_ACTION_HIT,
    PLAYER_ACTION_STAND
} PlayerAction;

typedef struct {
    int sockfd;
    pthread_t thread;
    int id; // 1-based
    char name[MAX_NAME_LEN];
    PlayerState state;

    // per-round
    Card hand[12];
    int hand_size;
    int is_busted;
    int has_stood;

    // action synchronization
    pthread_mutex_t action_lock;
    pthread_cond_t action_cond;
    PlayerAction pending_action;
    int awaiting_action; // coordinator sets to 1 to indicate awaiting

    int alive; // 1 = connection open and responsive, 0 = disconnected
} Player;

typedef struct {
    Player players[MAX_PLAYERS];
    pthread_mutex_t lock;
    int connected_count;
    Card deck[52];
    int deck_top;
    unsigned rand_seed;
} GameState;

GameState G;
int listen_fd = -1;
int server_running = 1;

static void handle_sigint(int sig) {
    (void)sig;
    server_running = 0;
    if (listen_fd >= 0) close(listen_fd);
}

// ------------------ helper implementations ------------------

ssize_t write_all(int fd, const void *buf, size_t count) {
    const uint8_t *p = buf;
    size_t left = count;
    while (left > 0) {
        ssize_t n = write(fd, p, left);
        if (n <= 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        left -= (size_t)n;
        p += n;
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
        if (n == 0) return (ssize_t)(count - left); // EOF
        left -= (size_t)n;
        p += n;
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
    if (len == 0) {
        *out_buf = strdup("");
        return 0;
    }
    if (len > MAX_PAYLOAD) {
        // too big; drain and return error
        char tmp[256];
        size_t to_read = len;
        while (to_read > 0) {
            ssize_t rr = read(fd, tmp, sizeof(tmp) < to_read ? sizeof(tmp) : to_read);
            if (rr <= 0) break;
            to_read -= rr;
        }
        return -1;
    }
    char *buf = malloc(len + 1);
    if (!buf) return -1;
    if (read_all(fd, buf, len) != (ssize_t)len) {
        free(buf);
        return -1;
    }
    buf[len] = '\0';
    *out_buf = buf;
    return (int)len;
}

// ------------------ deck.c content (deck utilities) ------------------

void init_deck(Card deck[52]) {
    for (int i = 0; i < 52; ++i) deck[i] = (Card)i;
}

void shuffle_deck(Card deck[52], unsigned *seedp) {
    for (int i = 51; i > 0; --i) {
        int j = rand_r(seedp) % (i + 1);
        Card tmp = deck[i]; deck[i] = deck[j]; deck[j] = tmp;
    }
}

Card deal_card(Card deck[52], int *top_index) {
    if (*top_index >= 52) {
        // caller should reshuffle before
        return 0xFF;
    }
    return deck[(*top_index)++];
}

void card_to_str(Card c, char *out) {
    if (c > 51) { strcpy(out, "??"); return; }
    const char *suits = "SHDC"; // spade, heart, diamond, club
    int suit = c / 13;
    int rank = c % 13; // 0 = Ace, 9 = 10, 10=J,11=Q,12=K
    char rbuf[4];
    if (rank == 0) strcpy(rbuf, "A");
    else if (rank >= 1 && rank <= 8) sprintf(rbuf, "%d", rank + 1);
    else if (rank == 9) strcpy(rbuf, "10");
    else if (rank == 10) strcpy(rbuf, "J");
    else if (rank == 11) strcpy(rbuf, "Q");
    else strcpy(rbuf, "K");
    sprintf(out, "%c%s", suits[suit], rbuf);
}

int hand_value(const Card *hand, int n) {
    int total = 0, aces = 0;
    for (int i = 0; i < n; ++i) {
        int r = hand[i] % 13;
        if (r == 0) { aces++; total += 11; }
        else if (r >= 10) total += 10;
        else total += (r + 1);
    }
    while (total > 21 && aces > 0) {
        total -= 10;
        aces--;
    }
    return total;
}

// ------------------ server utilities ------------------

void init_game_state(GameState *g) {
    pthread_mutex_init(&g->lock, NULL);
    g->connected_count = 0;
    g->rand_seed = (unsigned)time(NULL) ^ (unsigned)getpid();
    init_deck(g->deck);
    shuffle_deck(g->deck, &g->rand_seed);
    g->deck_top = 0;
    for (int i = 0; i < MAX_PLAYERS; ++i) {
        g->players[i].sockfd = -1;
        g->players[i].id = i+1;
        g->players[i].state = PLAYER_STATE_EMPTY;
        g->players[i].hand_size = 0;
        g->players[i].is_busted = 0;
        g->players[i].has_stood = 0;
        g->players[i].pending_action = PLAYER_ACTION_NONE;
        g->players[i].awaiting_action = 0;
        g->players[i].alive = 0;
        pthread_mutex_init(&g->players[i].action_lock, NULL);
        pthread_cond_init(&g->players[i].action_cond, NULL);
    }
}

int find_free_slot(GameState *g) {
    for (int i = 0; i < MAX_PLAYERS; ++i) {
        if (g->players[i].state == PLAYER_STATE_EMPTY) return i;
    }
    return -1;
}

void broadcast_msg(const char *fmt, ...) {
    char buf[MAX_PAYLOAD];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);

    pthread_mutex_lock(&G.lock);
    for (int i = 0; i < MAX_PLAYERS; ++i) {
        if (G.players[i].state != PLAYER_STATE_EMPTY && G.players[i].alive) {
            send_msg(G.players[i].sockfd, MSG_BROADCAST " "); // not strictly needed
            send_msg(G.players[i].sockfd, buf);
        }
    }
    pthread_mutex_unlock(&G.lock);
}

// Per-client reader thread: reads messages (JOIN handled before spawning), then loops reading frames
void *client_reader_thread(void *arg) {
    Player *p = (Player*)arg;
    int fd = p->sockfd;
    while (p->alive) {
        char *msg = NULL;
        int rr = recv_msg(fd, &msg);
        if (rr <= 0) {
            // disconnected
            free(msg);
            pthread_mutex_lock(&G.lock);
            p->alive = 0;
            p->state = PLAYER_STATE_EMPTY;
            G.connected_count--;
            pthread_mutex_unlock(&G.lock);
            // notify any waiting coordinator
            pthread_mutex_lock(&p->action_lock);
            p->pending_action = PLAYER_ACTION_STAND; // treat as stand when missing
            p->awaiting_action = 0;
            pthread_cond_signal(&p->action_cond);
            pthread_mutex_unlock(&p->action_lock);
            close(fd);
            printf("Player %d disconnected\n", p->id);
            break;
        }
        // parse message
        // messages: ACTION HIT, ACTION STAND, QUIT, CHAT ...
        if (strncmp(msg, CMD_ACTION, strlen(CMD_ACTION)) == 0) {
            char *arg = msg + strlen(CMD_ACTION);
            while (*arg == ' ') arg++;
            if (strncmp(arg, "HIT", 3) == 0) {
                pthread_mutex_lock(&p->action_lock);
                p->pending_action = PLAYER_ACTION_HIT;
                if (p->awaiting_action) {
                    p->awaiting_action = 0;
                    pthread_cond_signal(&p->action_cond);
                }
                pthread_mutex_unlock(&p->action_lock);
            } else if (strncmp(arg, "STAND", 5) == 0) {
                pthread_mutex_lock(&p->action_lock);
                p->pending_action = PLAYER_ACTION_STAND;
                if (p->awaiting_action) {
                    p->awaiting_action = 0;
                    pthread_cond_signal(&p->action_cond);
                }
                pthread_mutex_unlock(&p->action_lock);
            }
        } else if (strncmp(msg, CMD_QUIT, strlen(CMD_QUIT)) == 0) {
            free(msg);
            pthread_mutex_lock(&G.lock);
            p->alive = 0;
            p->state = PLAYER_STATE_EMPTY;
            G.connected_count--;
            pthread_mutex_unlock(&G.lock);
            close(fd);
            printf("Player %d quit\n", p->id);
            break;
        } else if (strncmp(msg, CMD_CHAT, strlen(CMD_CHAT)) == 0) {
            // optional chat handling
            // broadcast message to others
            char *payload = msg + strlen(CMD_CHAT);
            while (*payload == ' ') payload++;
            char bcast[MAX_PAYLOAD];
            snprintf(bcast, sizeof(bcast), "%s: %s", p->name, payload);
            // use simple broadcast: send MSG_BROADCAST then text
            pthread_mutex_lock(&G.lock);
            for (int i = 0; i < MAX_PLAYERS; ++i) {
                if (G.players[i].alive && G.players[i].sockfd != p->sockfd) {
                    send_msg(G.players[i].sockfd, MSG_BROADCAST);
                    send_msg(G.players[i].sockfd, bcast);
                }
            }
            pthread_mutex_unlock(&G.lock);
        }
        free(msg);
    }
    return NULL;
}

// Coordinator utilities: send player's hand, etc.
void send_hand_to_player(Player *p) {
    char buf[MAX_PAYLOAD];
    char c1[4], c2[4];
    if (p->hand_size >= 1) card_to_str(p->hand[0], c1); else strcpy(c1,"??");
    if (p->hand_size >= 2) card_to_str(p->hand[1], c2); else strcpy(c2,"??");
    snprintf(buf, sizeof(buf), MSG_DEAL " %s %s", c1, c2);
    send_msg(p->sockfd, buf);
}

// send a single card with MSG_CARD
void send_card_to_player(Player *p, Card c) {
    char s[4];
    card_to_str(c, s);
    char buf[MAX_PAYLOAD];
    snprintf(buf, sizeof(buf), MSG_CARD " %s", s);
    send_msg(p->sockfd, buf);
}

// send generic text to player (prefixed as BROADCAST for simplicity)
void send_text_to_player(Player *p, const char *text) {
    send_msg(p->sockfd, MSG_BROADCAST);
    send_msg(p->sockfd, text);
}

void reset_player_round(Player *p) {
    p->hand_size = 0;
    p->is_busted = 0;
    p->has_stood = 0;
    p->pending_action = PLAYER_ACTION_NONE;
    p->awaiting_action = 0;
}

void game_loop(void);

// Wrapper for pthread
void *game_loop_wrapper(void *arg) {
    (void)arg;
    game_loop();
    return NULL;
}

// main coordinator loop
void game_loop() {
    while (server_running) {
        // Wait for at least 1 connected player
        pthread_mutex_lock(&G.lock);
        while (G.connected_count == 0 && server_running) {
            pthread_mutex_unlock(&G.lock);
            sleep(1);
            pthread_mutex_lock(&G.lock);
        }
        pthread_mutex_unlock(&G.lock);
        if (!server_running) break;

        // Start a round
        printf("Starting a new round\n");

        // Reinitialize deck if necessary
        pthread_mutex_lock(&G.lock);
        if (G.deck_top > 52 - RESHUFFLE_THRESHOLD) {
            init_deck(G.deck);
            shuffle_deck(G.deck, &G.rand_seed);
            G.deck_top = 0;
            printf("Deck reshuffled\n");
        }

        // Reset players and deal initial two cards
        for (int i = 0; i < MAX_PLAYERS; ++i) {
            Player *p = &G.players[i];
            if (p->alive && p->state == PLAYER_STATE_IN_GAME) {
                reset_player_round(p);
                // deal two cards each
                p->hand[p->hand_size++] = deal_card(G.deck, &G.deck_top);
                p->hand[p->hand_size++] = deal_card(G.deck, &G.deck_top);
            }
        }

        // Dealer hand in coordinator (not a player)
        Card dealer_hand[12]; int dealer_size = 0;
        dealer_hand[dealer_size++] = deal_card(G.deck, &G.deck_top);
        dealer_hand[dealer_size++] = deal_card(G.deck, &G.deck_top);

        // Send initial DEAL messages
        pthread_mutex_lock(&G.lock);
        for (int i = 0; i < MAX_PLAYERS; ++i) {
            Player *p = &G.players[i];
            if (p->alive && p->state == PLAYER_STATE_IN_GAME) {
                send_hand_to_player(p);
            }
        }
        pthread_mutex_unlock(&G.lock);

        // Per-player turns
        for (int i = 0; i < MAX_PLAYERS; ++i) {
            Player *p = &G.players[i];
            if (!(p->alive && p->state == PLAYER_STATE_IN_GAME)) continue;
            // if busted or stood skip (fresh round none are)
            while (!p->is_busted && !p->has_stood) {
                // send YOUR_TURN & REQUEST_ACTION
                send_msg(p->sockfd, MSG_YOUR_TURN);
                send_msg(p->sockfd, MSG_REQUEST_ACTION);

                // wait for player's action (timed)
                struct timespec ts;
                clock_gettime(CLOCK_REALTIME, &ts);
                ts.tv_sec += ACTION_TIMEOUT_SEC;

                pthread_mutex_lock(&p->action_lock);
                p->awaiting_action = 1;
                while (p->awaiting_action && p->pending_action == PLAYER_ACTION_NONE && p->alive) {
                    int rc = pthread_cond_timedwait(&p->action_cond, &p->action_lock, &ts);
                    if (rc == ETIMEDOUT) {
                        // treat timeout as STAND
                        p->pending_action = PLAYER_ACTION_STAND;
                        p->awaiting_action = 0;
                        break;
                    }
                }
                PlayerAction act = p->pending_action;
                // reset pending for next time
                p->pending_action = PLAYER_ACTION_NONE;
                p->awaiting_action = 0;
                pthread_mutex_unlock(&p->action_lock);

                if (!p->alive) break;

                if (act == PLAYER_ACTION_HIT) {
                    Card c = deal_card(G.deck, &G.deck_top);
                    p->hand[p->hand_size++] = c;
                    send_card_to_player(p, c);
                    int hv = hand_value(p->hand, p->hand_size);
                    if (hv > 21) {
                        p->is_busted = 1;
                        send_msg(p->sockfd, MSG_BUSTED);
                        break;
                    } else {
                        // continue loop (player may hit again)
                        continue;
                    }
                } else { // stand
                    p->has_stood = 1;
                    break;
                }
            } // end while per player
        } // next player

        // Dealer plays: reveal hole and hit until >=17
        int dealer_val = hand_value(dealer_hand, dealer_size);
        // (optional): send dealer hole reveal to all
        char hole0[4], hole1[4];
        card_to_str(dealer_hand[0], hole0);
        card_to_str(dealer_hand[1], hole1);
        char reveal_msg[MAX_PAYLOAD];
        snprintf(reveal_msg, sizeof(reveal_msg), "Dealer shows %s %s", hole0, hole1);
        pthread_mutex_lock(&G.lock);
        for (int i = 0; i < MAX_PLAYERS; ++i) {
            Player *p = &G.players[i];
            if (p->alive && p->state == PLAYER_STATE_IN_GAME) {
                send_text_to_player(p, reveal_msg);
            }
        }
        pthread_mutex_unlock(&G.lock);

        // Dealer rules: hit while < 17 (treat Ace appropriately via hand_value)
        while (dealer_val < 17) {
            Card c = deal_card(G.deck, &G.deck_top);
            dealer_hand[dealer_size++] = c;
            // notify players of dealer card
            char s[4]; card_to_str(c, s);
            char dbuf[MAX_PAYLOAD];
            snprintf(dbuf, sizeof(dbuf), "Dealer hits %s", s);
            pthread_mutex_lock(&G.lock);
            for (int i = 0; i < MAX_PLAYERS; ++i) {
                Player *p = &G.players[i];
                if (p->alive && p->state == PLAYER_STATE_IN_GAME) {
                    send_text_to_player(p, dbuf);
                }
            }
            pthread_mutex_unlock(&G.lock);
            dealer_val = hand_value(dealer_hand, dealer_size);
        }

        // Evaluate results and send RESULT to each player
        pthread_mutex_lock(&G.lock);
        for (int i = 0; i < MAX_PLAYERS; ++i) {
            Player *p = &G.players[i];
            if (!(p->alive && p->state == PLAYER_STATE_IN_GAME)) continue;
            int pval = hand_value(p->hand, p->hand_size);
            char result_msg[MAX_PAYLOAD];
            if (p->is_busted) {
                snprintf(result_msg, sizeof(result_msg), MSG_RESULT " LOSE %d %d", pval, dealer_val);
            } else if (dealer_val > 21) {
                snprintf(result_msg, sizeof(result_msg), MSG_RESULT " WIN %d %d", pval, dealer_val);
            } else {
                if (pval > dealer_val) snprintf(result_msg, sizeof(result_msg), MSG_RESULT " WIN %d %d", pval, dealer_val);
                else if (pval < dealer_val) snprintf(result_msg, sizeof(result_msg), MSG_RESULT " LOSE %d %d", pval, dealer_val);
                else snprintf(result_msg, sizeof(result_msg), MSG_RESULT " PUSH %d %d", pval, dealer_val);
            }
            send_msg(p->sockfd, result_msg);
        }
        pthread_mutex_unlock(&G.lock);

        // small pause between rounds
        sleep(2);
    }
}

// Accept loop: accepts connections, expects JOIN <name> immediately, then spawns client_reader_thread
void accept_loop(int port) {
    struct sockaddr_in addr;
    listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) { perror("socket"); exit(1); }
    int opt = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(listen_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind"); exit(1);
    }
    if (listen(listen_fd, BACKLOG) < 0) { perror("listen"); exit(1); }
    printf("Server listening on port %d\n", port);

    while (server_running) {
        struct sockaddr_storage ss;
        socklen_t slen = sizeof(ss);
        int client_fd = accept(listen_fd, (struct sockaddr*)&ss, &slen);
        if (client_fd < 0) {
            if (errno == EINTR) continue;
            perror("accept"); break;
        }
        // set a small recv timeout to detect dead connections quicker (optional)
        struct timeval tv = { .tv_sec = 0, .tv_usec = 0 }; // default blocking; change if needed
        setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));

        // immediately expect a JOIN message (within recv_msg)
        char *msg = NULL;
        int rr = recv_msg(client_fd, &msg);
        if (rr <= 0) {
            close(client_fd);
            free(msg);
            continue;
        }
        if (strncmp(msg, CMD_JOIN, strlen(CMD_JOIN)) != 0) {
            // send error and close
            send_msg(client_fd, MSG_ERROR);
            send_msg(client_fd, "Expected JOIN");
            close(client_fd);
            free(msg);
            continue;
        }
        // parse name
        char *name = msg + strlen(CMD_JOIN);
        while (*name == ' ') name++;
        char pname[MAX_NAME_LEN];
        strncpy(pname, name, MAX_NAME_LEN-1);
        pname[MAX_NAME_LEN-1] = '\0';
        free(msg);

        // assign a slot
        pthread_mutex_lock(&G.lock);
        int slot = find_free_slot(&G);
        if (slot < 0) {
            pthread_mutex_unlock(&G.lock);
            send_msg(client_fd, MSG_ERROR);
            send_msg(client_fd, "Server full");
            close(client_fd);
            continue;
        }
        Player *p = &G.players[slot];
        p->sockfd = client_fd;
        p->state = PLAYER_STATE_CONNECTED;
        p->alive = 1;
        strncpy(p->name, pname, MAX_NAME_LEN-1);
        p->name[MAX_NAME_LEN-1] = '\0';
        p->hand_size = 0;
        p->has_stood = 0;
        p->is_busted = 0;
        p->pending_action = PLAYER_ACTION_NONE;
        p->awaiting_action = 0;
        send_msg(client_fd, MSG_WELCOME);
        char welc[MAX_PAYLOAD];
        snprintf(welc, sizeof(welc), "%s %d", p->name, p->id);
        send_msg(client_fd, welc);

        // mark as in-game for rounds
        p->state = PLAYER_STATE_IN_GAME;
        G.connected_count++;
        pthread_mutex_unlock(&G.lock);

        // spawn reader thread
        pthread_create(&p->thread, NULL, client_reader_thread, p);
        printf("Player %d connected: %s\n", p->id, p->name);
    }
}

// main
int main(int argc, char **argv) {
    int port = DEFAULT_PORT;
    if (argc >= 2) port = atoi(argv[1]);
    signal(SIGINT, handle_sigint);
    init_game_state(&G);
    
    // Start game loop in a separate thread
    pthread_t game_thread;
    pthread_create(&game_thread, NULL, game_loop_wrapper, NULL);
    
    accept_loop(port);
    // if server_running becomes 0, drop to cleanup and exit
    
    // Wait for game thread to finish
    pthread_join(game_thread, NULL);
    
    printf("Server shutting down\n");
    close(listen_fd);
    return 0;
}
