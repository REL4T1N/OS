#include <zmq.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <pthread.h>
#include <stdbool.h>
#include <unistd.h>

#define MAX_MSG_LEN 1024
#define USERNAME_SIZE 128
#define COMMAND_SIZE 64

typedef struct {
    void *context;
    void *dealer_socket;
    void *sub_socket;
    char username[USERNAME_SIZE];
    bool running;
    pthread_t recv_thread;
} ClientState;

ClientState client_state;

void *receive_thread(void *arg) {
    while (client_state.running) {
        zmq_pollitem_t items[] = {
            {client_state.dealer_socket, 0, ZMQ_POLLIN, 0},
            {client_state.sub_socket, 0, ZMQ_POLLIN, 0}
        };

        int rc = zmq_poll(items, 2, 100);

        if (rc > 0) {
            if (items[0].revents & ZMQ_POLLIN) {
                char msg[MAX_MSG_LEN];
                zmq_recv(client_state.dealer_socket, msg, MAX_MSG_LEN, 0);

                if (strncmp(msg, "REGISTERED ", 11) == 0) {
                    printf("%s, вы успешно подключились к серверу\n", client_state.username);
                } else {
                    printf("%s\n", msg);
                }
            }

            if (items[1].revents & ZMQ_POLLIN) {
                char msg[MAX_MSG_LEN];
                zmq_recv(client_state.sub_socket, msg, MAX_MSG_LEN, 0);
                printf("%s\n", msg);
            }
        }
    }
    return NULL;
}

void parse_and_send_message(const char *input) {
    char command[COMMAND_SIZE], recipient[USERNAME_SIZE], msg[MAX_MSG_LEN];

    if (strncmp(input, "/msg ", 5) == 0) {
        if (sscanf(input + 5, "@all %[^\n]", msg) == 1) {
            char full_msg[MAX_MSG_LEN * 2];
            snprintf(full_msg, sizeof(full_msg), "MESSAGE %s all %s", client_state.username, msg);
            zmq_send(client_state.dealer_socket, full_msg, strlen(full_msg), 0);
        } else if (sscanf(input + 5, "@%s %[^\n]", recipient, msg) == 2) {
            char full_msg[MAX_MSG_LEN * 2];
            snprintf(full_msg, sizeof(full_msg), "MESSAGE %s %s %s", client_state.username, recipient, msg);
            zmq_send(client_state.dealer_socket, full_msg, strlen(full_msg), 0);
        } else {    // сообщение себе
            sscanf(input + 5, "%[^\n]", msg);
            char full_msg[MAX_MSG_LEN * 2];
            snprintf(full_msg, sizeof(full_msg), "MESSAGE %s %s %s", client_state.username, client_state.username, msg);
            zmq_send(client_state.dealer_socket, full_msg, strlen(full_msg), 0);
        }
    } else if (strncmp(input, "/delayed_msg ", 13) == 0) {
        int delay;
        char delay_input[MAX_MSG_LEN];
        strcpy(delay_input, input + 13);

        if (sscanf(delay_input, "%d @all %[^\n]", &delay, msg) == 2) {
            char full_msg[MAX_MSG_LEN * 2];
            snprintf(full_msg, sizeof(full_msg), "DELAYED %d %s all %s", delay, client_state.username, msg);
            zmq_send(client_state.dealer_socket, full_msg, strlen(full_msg), 0);            
        } else if (sscanf(delay_input, "%d @%s %[^\n]", &delay, recipient, msg) == 3) {
            char full_msg[MAX_MSG_LEN * 2];
            snprintf(full_msg, sizeof(full_msg), "DELAYED %d %s %s %s", delay, client_state.username, recipient, msg);
            zmq_send(client_state.dealer_socket, full_msg, strlen(full_msg), 0);
        } else if (sscanf(delay_input, "%d %[^\n]", &delay, msg) == 2) {
            char full_msg[MAX_MSG_LEN * 2];
            snprintf(full_msg, sizeof(full_msg), "DELAYED %d %s %s %s", delay, client_state.username, client_state.username, msg);
            zmq_send(client_state.dealer_socket, full_msg, strlen(full_msg), 0);
        }
    } else if (strcmp(input, "/exit") == 0) {
        char full_msg[MAX_MSG_LEN];
        snprintf(full_msg, sizeof(full_msg), "EXIT %s", client_state.username);
        zmq_send(client_state.dealer_socket, full_msg, strlen(full_msg), 0);
        client_state.running = false;
    } else {    // сообщение без команд отправляется самому себе
        char full_msg[MAX_MSG_LEN * 2];
        snprintf(full_msg, sizeof(full_msg), "MESSAGE %s %s %s", client_state.username, client_state.username, input);
        zmq_send(client_state.dealer_socket, full_msg, strlen(full_msg), 0);
    }
}

int main(int argC, char *argV[]) {
    if (argC != 2) {
        printf("Использование: %s <имя пользователя>\n", argV[0]);
        return 1;
    }

    if (strcmp(argV[1], "all") == 0) {
        printf("Имя 'all' зарезервировано системой\n");
        return 1;
    }

    client_state.context = zmq_ctx_new();
    client_state.dealer_socket = zmq_socket(client_state.context, ZMQ_DEALER);
    client_state.sub_socket = zmq_socket(client_state.context, ZMQ_SUB);

    strcpy(client_state.username, argV[1]);
    client_state.running = true;

    zmq_connect(client_state.dealer_socket, "tcp://localhost:6666");
    zmq_connect(client_state.sub_socket, "tcp://localhost:2121");
    zmq_setsockopt(client_state.sub_socket, ZMQ_SUBSCRIBE, "", 0);  // подписка на обновления

    char reg_msg[MAX_MSG_LEN];
    snprintf(reg_msg, sizeof(reg_msg), "REGISTER %s", client_state.username);
    zmq_send(client_state.dealer_socket, reg_msg, strlen(reg_msg), 0);

    // поток для приёма сообщений
    pthread_create(&client_state.recv_thread, NULL, receive_thread, NULL);

    char input[MAX_MSG_LEN];
    while (client_state.running) {
        if (fgets(input, sizeof(input), stdin) != NULL) {
            input[strcspn(input, "\n")] = 0;    // удалить \n
            if (strlen(input) > 0) {
                parse_and_send_message(input);
            }
        }
    }

    pthread_join(client_state.recv_thread, NULL);
    zmq_close(client_state.dealer_socket);
    zmq_close(client_state.sub_socket);
    zmq_ctx_destroy(client_state.context);
    // printf("Клиент отключён\n");
    return 0;
}