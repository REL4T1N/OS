#include <zmq.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#define MAX_CLIENTS 10

typedef struct {
    char id[256];
    size_t size;
} client_t;

int client_exists(client_t *clients, int count, zmq_msg_t *id)
{
    for (int i = 0; i < count; i++) {
        if (clients[i].size == zmq_msg_size(id) &&
            memcmp(clients[i].id, zmq_msg_data(id), clients[i].size) == 0) {
            return 1;
        }
    }
    return 0;
}

int find_client(client_t *clients, int count, const char *name)
{
    for (int i = 0; i < count; i++) {
        if (clients[i].size == strlen(name) &&
            memcmp(clients[i].id, name, clients[i].size) == 0) {
            return i;
        }
    }
    return -1;
}

void current_time(char *buf, size_t size)
{
    time_t t = time(NULL);
    struct tm *tm = localtime(&t);

    snprintf(buf,
             size,
             "[%02d:%02d:%02d]",
             tm->tm_hour,
             tm->tm_min,
             tm->tm_sec);
}

int main(void)
{
    void *ctx = zmq_ctx_new();
    void *router = zmq_socket(ctx, ZMQ_ROUTER);

    zmq_bind(router, "tcp://*:5555");

    client_t clients[MAX_CLIENTS];
    int client_count = 0;

    while (1) {
        zmq_msg_t id;
        zmq_msg_t msg;

        zmq_msg_init(&id);
        zmq_msg_init(&msg);

        zmq_msg_recv(&id, router, 0);
        zmq_msg_recv(&msg, router, 0);

        char sender[256];
        memcpy(sender, zmq_msg_data(&id), zmq_msg_size(&id));
        sender[zmq_msg_size(&id)] = '\0';

        char text[256];
        memcpy(text, zmq_msg_data(&msg), zmq_msg_size(&msg));
        text[zmq_msg_size(&msg)] = '\0';

        char ts[16];

        if (strncmp(zmq_msg_data(&msg), "JOIN", 4) == 0) {
            if (!client_exists(clients, client_count, &id)) {
                memcpy(clients[client_count].id,
                       zmq_msg_data(&id),
                       zmq_msg_size(&id));
                clients[client_count].size = zmq_msg_size(&id);
                client_count++;

                // char ts[16];
                current_time(ts, sizeof(ts));

                printf("%s client joined: %s\n", ts, sender);
            }
        } else {
            // char sender[256];
            // char text[256];
            // memcpy(sender, zmq_msg_data(&id), zmq_msg_size(&id));
            // sender[zmq_msg_size(&id)] = '\0';
            // memcpy(text, zmq_msg_data(&msg), zmq_msg_size(&msg));
            // text[zmq_msg_size(&msg)] = '\0';

            if (text[0] != '@') {
                zmq_msg_close(&id);
                zmq_msg_close(&msg);
                continue;
            }

            char *space = strchr(text, ' ');
            if (!space) {
                zmq_msg_close(&id);
                zmq_msg_close(&msg);
                continue;
            }

            *space = '\0';
            char *target = text + 1;
            char *payload = space + 1;

            current_time(ts, sizeof(ts));

            if (strcmp(target, "all") == 0) {
                printf("%s %s -> all: %s\n", ts, sender, payload);

                char out_other[512];
                char out_me[512];

                snprintf(out_other,
                         sizeof(out_other),
                         "%s %s -> all: %s",
                         ts,
                         sender,
                         payload);

                snprintf(out_me,
                         sizeof(out_me),
                         "%s Me: %s",
                         ts,
                         payload);

                for (int i = 0; i < client_count; i++) {
                    zmq_send(router,
                             clients[i].id,
                             clients[i].size,
                             ZMQ_SNDMORE);

                    if (clients[i].size == zmq_msg_size(&id) &&
                        memcmp(clients[i].id,
                               zmq_msg_data(&id),
                               clients[i].size) == 0) {
                        zmq_send(router,
                                 out_me,
                                 strlen(out_me),
                                 0);
                    } else {
                        zmq_send(router,
                                 out_other,
                                 strlen(out_other),
                                 0);
                    }
                }
            } else {
                printf("%s %s -> %s: %s\n", ts, sender, target, payload);

                // printf("%s -> %s: %s\n", sender, target, payload);
                // char ts[16];
                // current_time(ts, sizeof(ts));

                // printf("%s %s -> %s: %s\n",
                //     ts,
                //     sender,
                //     target,
                //     payload);
                int idx = find_client(clients, client_count, target);
                if (idx >= 0) {
                    char out[512];

                    snprintf(out,
                             sizeof(out),
                             "%s %s -> %s: %s",
                             ts,
                             sender,
                             target,
                             payload);

                    zmq_send(router,
                             clients[idx].id,
                             clients[idx].size,
                             ZMQ_SNDMORE);
                    zmq_send(router,
                             out,
                             strlen(out),
                             0);
                }
            }
        }


        zmq_msg_close(&id);
        zmq_msg_close(&msg);
    }

    zmq_close(router);
    zmq_ctx_destroy(ctx);
}
