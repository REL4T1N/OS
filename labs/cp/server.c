#include <zmq.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#define MAX_USERS 32
#define MAX_DELAYED 128
#define ZMQ_ID_LEN 256
#define USERNAME_SIZE 256
#define MAX_MSG_LEN 1024
#define TIME_LEN 16


typedef struct {
    char name[USERNAME_SIZE];
    int online;
    char zmq_id[ZMQ_ID_LEN];
    size_t zmq_id_size;
} user_t;

typedef struct {
    struct timespec deliver_at;
    char sender[USERNAME_SIZE];
    char receiver[USERNAME_SIZE];
    char payload[MAX_MSG_LEN];
} delayed_msg_t;

void current_time(char *buf, size_t size) {
    time_t t = time(NULL);
    struct tm *tm = localtime(&t);

    snprintf(buf, size,
             "[%02d:%02d:%02d]",
             tm->tm_hour,
             tm->tm_min,
             tm->tm_sec);
}

void format_time(time_t t, char *buf, size_t size) {
    struct tm *tm = localtime(&t);
    snprintf(buf, size,
             "%02d:%02d:%02d",
             tm->tm_hour,
             tm->tm_min,
             tm->tm_sec);
}

timespec_ge(const struct timespec *a, const struct timespec *b) {
    return (a->tv_sec > b->tv_sec) ||
           (a->tv_sec == b->tv_sec &&
            a->tv_nsec >= b->tv_nsec);
}

int find_user(user_t *users, int count, const char *name) {
    for (int i = 0; i < count; i++) {
        if (strcmp(users[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}

int main() {
    void *ctx = zmq_ctx_new();
    void *router = zmq_socket(ctx, ZMQ_ROUTER);

    zmq_bind(router, "tcp://*:5555");

    user_t users[MAX_USERS];
    delayed_msg_t delayed[MAX_DELAYED];
    int user_count = 0, delayed_count = 0;

    while (1) {
        zmq_pollitem_t items[] = {
            {router, 0, ZMQ_POLLIN, 0}
        };
        zmq_poll(items, 1, 100);

        if (items[0].revents & ZMQ_POLLIN) {
            zmq_msg_t id;
            zmq_msg_t msg;

            zmq_msg_init(&id);
            zmq_msg_init(&msg);

            zmq_msg_recv(&id, router, 0);
            zmq_msg_recv(&msg, router, 0);

            char sender[USERNAME_SIZE];
            char text[MAX_MSG_LEN];
            memcpy(sender, zmq_msg_data(&id), zmq_msg_size(&id));
            memcpy(text, zmq_msg_data(&msg), zmq_msg_size(&msg));
            sender[zmq_msg_size(&id)] = '\0';
            text[zmq_msg_size(&msg)] = '\0';

            char ts[TIME_LEN];

            if (strcmp(text, "JOIN") == 0) {
                int idx = find_user(users, user_count, sender);
                current_time(ts, sizeof(ts));
                
                if (idx >= 0 && users[idx].online) {
                    zmq_msg_close(&id);
                    zmq_msg_close(&msg);
                    continue;
                }

                if (idx < 0) {
                    idx = user_count++;
                    strcpy(users[idx].name, sender);
                }
                
                users[idx].online = 1;
                memcpy(users[idx].zmq_id, zmq_msg_data(&id), zmq_msg_size(&id));
                users[idx].zmq_id_size = zmq_msg_size(&id);

                printf("%s client joined: %s\n", ts, sender);
            } else if (strcmp(text, "/exit") == 0) {
                int idx =  find_user(users, user_count, sender);
                if (idx >= 0) {
                    users[idx].online = 0;
                }
                printf("%s client disconnected: %s\n", ts, sender);
            } else if (text[0] == '/') {
                char *space1 = strchr(text, ' ');
                if (!space1) {
                    zmq_msg_close(&id);
                    zmq_msg_close(&msg);
                    continue;
                }

                *space1 = '\0';
                char *cmd = text;
                char *rest = space1 + 1;

                char *space2 = strchr(rest, ' ');
                if (!space2) {
                    zmq_msg_close(&id);
                    zmq_msg_close(&msg); 
                    continue;
                }

                *space2 = '\0';

                if (rest[0] != '@') {
                    zmq_msg_close(&id);
                    zmq_msg_close(&msg);
                    continue;
                }

                char *target = rest + 1;
                char *payload = space2 + 1;

                if (payload[0] == '\0' || target[0] == '\0') {
                    zmq_msg_close(&id);
                    zmq_msg_close(&msg);
                    continue;
                }

                if (strcmp(target, "all") == 0) {
                    if (strcmp(cmd, "/m") != 0) {
                        zmq_msg_close(&id);
                        zmq_msg_close(&msg);
                        continue;
                    }
                }

                current_time(ts, sizeof(ts));

                if (strcmp(cmd, "/m") == 0) {
                    if (strcmp(target, "all") == 0) {
                        printf("%s %s -> all: %s\n", ts, sender, payload);
                        char out_other[MAX_MSG_LEN];
                        char out_me[MAX_MSG_LEN];

                        snprintf(out_other, sizeof(out_other), "%s %s -> all: %s", ts, sender, payload);
                        snprintf(out_me, sizeof(out_me), "%s Me: %s", ts, payload);
                        for (int i = 0; i < user_count; i++) {
                            if (!users[i].online) {
                                continue;
                            }

                            zmq_send(router, users[i].zmq_id, users[i].zmq_id_size, ZMQ_SNDMORE);
                            if (strcmp(users[i].name, sender) == 0) {
                                zmq_send(router, out_me, strlen(out_me), 0);
                            } else {
                                zmq_send(router, out_other, strlen(out_other), 0);
                            }
                        }
                    } else {
                        printf("%s %s -> %s: %s\n", ts, sender, target, payload);

                        int r_idx = find_user(users, user_count, target);
                        if (r_idx >= 0 && users[r_idx].online) {
                            char out[MAX_MSG_LEN];
                            if (strcmp(target, sender) == 0) {
                                snprintf(out, sizeof(out), "%s Me: %s", ts, payload);
                            } else {
                                snprintf(out, sizeof(out), "%s %s -> %s: %s", ts, sender, target, payload);
                            }

                            zmq_send(router, users[r_idx].zmq_id, users[r_idx].zmq_id_size, ZMQ_SNDMORE);
                            zmq_send(router, out, strlen(out), 0);
                        }
                    }
                } else if (strncmp(cmd, "/dm_", 4) == 0) {
                    int delay = atoi(cmd + 4);
                    if (delay <= 0 || delayed_count >= MAX_DELAYED) {
                        zmq_msg_close(&id);
                        zmq_msg_close(&msg);
                        continue;
                    }

                    struct timespec now_mono;
                    clock_gettime(CLOCK_MONOTONIC, &now_mono);

                    struct timespec deliver_at;
                    deliver_at.tv_sec  = now_mono.tv_sec + delay;
                    deliver_at.tv_nsec = now_mono.tv_nsec;

                    time_t eta_wall = time(NULL) + delay;
                    char eta[TIME_LEN];
                    format_time(eta_wall, eta, sizeof(eta));

                    printf("%s (DELAYED) %s -> %s: %s (ETA %s)\n", ts, sender, target, payload, eta);

                    delayed_msg_t *d = &delayed[delayed_count++];
                    d->deliver_at = deliver_at;
                    strcpy(d->sender, sender);
                    strcpy(d->receiver, target);
                    strcpy(d->payload, payload);
                }
            }
            zmq_msg_close(&id);
            zmq_msg_close(&msg);
            continue;
        }

        struct timespec now_mono;
        clock_gettime(CLOCK_MONOTONIC, &now_mono);
        for (int i = 0; i < delayed_count; ) {
            if (timespec_ge(&now_mono, &delayed[i].deliver_at)) {
                char ts[TIME_LEN];
                current_time(ts, sizeof(ts));

                int r_idx = find_user(users, user_count, delayed[i].receiver);
                
                if (r_idx >= 0 && users[r_idx].online) {
                    printf("%s (DELAYED DELIVERED) %s -> %s: %s\n", ts, delayed[i].sender, delayed[i].receiver, delayed[i].payload);
                    char out[MAX_MSG_LEN];

                    snprintf(out, sizeof(out), "%s (delayed) %s -> %s: %s", ts, delayed[i].sender, delayed[i].receiver, delayed[i].payload);
                    zmq_send(router, users[r_idx].zmq_id, users[r_idx].zmq_id_size, ZMQ_SNDMORE);
                    zmq_send(router, out, strlen(out), 0);                
                } else {
                    printf("%s (DELAYED DROPPED) %s -> %s: %s (receiver offline)\n", ts, delayed[i].sender, delayed[i].receiver, delayed[i].payload);
                }
                delayed[i] = delayed[--delayed_count];
            } else {
                i++;
            }
        }
    }
    zmq_close(router);
    zmq_ctx_destroy(ctx);
}