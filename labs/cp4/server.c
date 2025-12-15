#include <zmq.h>
#include <stdio.h>
#include <string.h>

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

        if (strncmp(zmq_msg_data(&msg), "JOIN", 4) == 0) {
            if (!client_exists(clients, client_count, &id)) {
                memcpy(clients[client_count].id,
                       zmq_msg_data(&id),
                       zmq_msg_size(&id));
                clients[client_count].size = zmq_msg_size(&id);
                client_count++;

                printf("Client joined: %.*s\n",
                       (int)zmq_msg_size(&id),
                       (char *)zmq_msg_data(&id));
            }
        } else {
            printf("From %.*s: %.*s\n",
                   (int)zmq_msg_size(&id),
                   (char *)zmq_msg_data(&id),
                   (int)zmq_msg_size(&msg),
                   (char *)zmq_msg_data(&msg));

            for (int i = 0; i < client_count; i++) {
                if (clients[i].size == zmq_msg_size(&id) &&
                    memcmp(clients[i].id, zmq_msg_data(&id), clients[i].size) == 0) {
                    continue;
                }

                zmq_send(router, clients[i].id, clients[i].size, ZMQ_SNDMORE);
                zmq_send(router,
                         zmq_msg_data(&msg),
                         zmq_msg_size(&msg),
                         0);
            }
        }

        zmq_msg_close(&id);
        zmq_msg_close(&msg);
    }

    zmq_close(router);
    zmq_ctx_destroy(ctx);
}
