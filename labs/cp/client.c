#include <zmq.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define MAX_MSG_LEN 1024

int main(int argc, char **argv) {
    if (argc != 2) {
        printf("Usage: %s <name>\n", argv[0]);
        return 1;
    }

    void *ctx = zmq_ctx_new();
    void *dealer = zmq_socket(ctx, ZMQ_DEALER);

    zmq_setsockopt(dealer, ZMQ_IDENTITY, argv[1], strlen(argv[1]));
    zmq_connect(dealer, "tcp://localhost:5555");
    zmq_send(dealer, "JOIN", 4, 0);

    zmq_pollitem_t items[] = {
        { dealer, 0, ZMQ_POLLIN, 0 },
        { NULL,   STDIN_FILENO, ZMQ_POLLIN, 0 }
    };

    char buffer[MAX_MSG_LEN];
    while (1) {
        zmq_poll(items, 2, -1);

        if (items[0].revents & ZMQ_POLLIN) {
            int size = zmq_recv(dealer, buffer, sizeof(buffer) - 1, 0);
            buffer[size] = '\0';

            printf("%s\n", buffer);
        }

        if (items[1].revents & ZMQ_POLLIN) {
            if (!fgets(buffer, sizeof(buffer), stdin)) {
                break;
            }

            buffer[strcspn(buffer, "\n")] = '\0';
            if (buffer[0] == '\0') {
                continue;
            }
            zmq_send(dealer, buffer, strlen(buffer), 0);
            if (strcmp(buffer, "/exit") == 0) {
                break;
            }
        }
    }
    zmq_close(dealer);
    zmq_ctx_destroy(ctx);
    return 0;
}
