#include <zmq.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <pthread.h>
#include <signal.h>
#include <unistd.h>
#include <stdbool.h>

#include <stdarg.h>

#define MAX_USERS 100
#define MAX_MSG_LEN 1024
#define USERNAME_SIZE 128
#define TIME_STRSIZE 9

typedef struct {
    char username[USERNAME_SIZE];
    void *dealer_socket;            // что за поле
    time_t connect_time;
    bool active;
} User;

typedef struct {
    char sender[USERNAME_SIZE];
    char recipient[USERNAME_SIZE];
    char message[MAX_MSG_LEN];
    time_t send_time;
    time_t delivery_time;
    bool is_delayed;
    time_t delay_until;
} Message;

typedef struct {
    User users[MAX_USERS];
    int user_count;
    void *context;                  // котекст сообщения
    void *router_socket;            // что бы это не значило
    void *pub_socket;               // сокет для получения сообщений на сервер и отправи их дальше
    pthread_mutex_t mutex;
    bool running;
} ServerState;

ServerState server_state;

void log_message(const char *format, ...) {
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);   // какая структурка, лучше изучить что это
    char time_str[TIME_STRSIZE];
    strftime(time_str, sizeof(time_str), "%H:%M:%S", tm_info);      // типо распарсил текущее время на чч:мм:сс

    printf("[%s]", time_str);

    // эта что за набор функций? из какой они библиотеки и для чего используются
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    printf("\n");
    fflush(stdout);
}

void send_to_user(const char *username, const char *msg) {
    pthread_mutex_lock(&server_state.mutex);

    for (int i = 0; i < server_state.user_count; ++i) {
        if (server_state.users[i].active && strcmp(server_state.users[i].username, username) == 0) {
            zmq_send(server_state.users[i].dealer_socket, msg, strlen(msg), 0);
            break;
        }
    }

    pthread_mutex_unlock(&server_state.mutex);
}

void broadcast_message(const char *msg, const char *excude_user) {
    pthread_mutex_lock(&server_state.mutex);

    for (int i = 0; i < server_state.user_count; ++i) {
        if (server_state.users[i].active && (excude_user == NULL || strcmp(server_state.users[i].username, excude_user) != 0)) {
            zmq_send(server_state.users[i].dealer_socket, msg, strlen(msg), 0);
        }
    }
    
    pthread_mutex_unlock(&server_state.mutex);
}

void *delayed_messages_pthread(void *arg) {
    Message delayed_msgs[100];
    int msg_count = 0;

    while (server_state.running) {
        time_t now = time(NULL);
        for (int i = 0; i < msg_count; ++i) {
            if (delayed_msgs[i].is_delayed && delayed_msgs[i].delay_until <= now) {     // проверка, что сообщение относится к отложенным и пришло время отправки
                char formatted_msg[MAX_MSG_LEN * 2];        // зачем х2 размер?
                struct tm *tm_info = localtime(&delayed_msgs[i].send_time);
                char send_time_str[TIME_STRSIZE];
                strftime(send_time_str, sizeof(send_time_str), "%H:%M:%S", tm_info);

                tm_info = localtime(&now);
                char deliver_time_str[TIME_STRSIZE];
                strftime(deliver_time_str, sizeof(deliver_time_str), "%H:%M:%S", tm_info);

                if (strcmp(delayed_msgs[i].recipient, "all") == 0) {
                    snprintf(formatted_msg, sizeof(formatted_msg), "[%s] %s -> all: %s (отправлено в %s)", deliver_time_str, delayed_msgs[i].sender, delayed_msgs[i].message, send_time_str);

                    broadcast_message(formatted_msg, delayed_msgs[i].sender);
                    log_message("Пользователь @%s получил отложенное сообщение от пользователя @%s. Отправлено в %s, доставлено в %s", delayed_msgs[i].recipient, delayed_msgs[i].sender, send_time_str, deliver_time_str);
                } else {
                    snprintf(formatted_msg, sizeof(formatted_msg), "[%s->%s] %s -> %s: %s", send_time_str, deliver_time_str, delayed_msgs[i].sender, delayed_msgs[i].recipient, delayed_msgs[i].message);

                    send_to_user(delayed_msgs[i].recipient, formatted_msg);
                    log_message("Пользователь @%s получил отложенное сообщение от пользователя @%s. Отправлено в %s, доставлено в %s", delayed_msgs[i].recipient, delayed_msgs[i].sender, send_time_str, deliver_time_str);
                }
                for (int j = i; j < msg_count - 1; ++j) {
                    delayed_msgs[j] = delayed_msgs[j + 1];
                }

                msg_count--;
                i--;
            }

        }
        sleep(1);       // проверка каждую секунду
    }
    return NULL;
}

void handle_signal(int sig) {
    log_message("Получен сигнал завершения. Остановка сервера...");
    server_state.running = false;
}

int main() {
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    server_state.context = zmq_ctx_new();
    server_state.router_socket = zmq_socket(server_state.context, ZMQ_ROUTER);      // узнать как работает router
    server_state.pub_socket = zmq_socket(server_state.context, ZMQ_PUB);            // узнать как работает pub/sub
    server_state.user_count = 0;
    server_state.running = true;
    pthread_mutex_init(&server_state.mutex, NULL);

    zmq_bind(server_state.router_socket, "tcp://*:6666");
    zmq_bind(server_state.pub_socket, "tcp://*:2121");

    log_message("Сервер запущен");

    pthread_t delayed_pthread;
    pthread_create(&delayed_pthread, NULL, delayed_messages_pthread, NULL);

    while (server_state.running) {
        zmq_pollitem_t items[] = {
            {server_state.router_socket, 0, ZMQ_POLLIN, 0}      // узнать что я тут сделал
        };

        int rc = zmq_poll(items, 1, 100);       // полинг каждые 100 мс
        if (rc > 0 && items[0].revents & ZMQ_POLLIN) {
            char identity[256];     // нужна константа
            char msg[MAX_MSG_LEN];

            zmq_recv(server_state.router_socket, identity, 256, 0);        // что за функция, что за identity, что за 256 и 0
            zmq_recv(server_state.router_socket, msg, MAX_MSG_LEN, 0); // аналогичные вопросы

            if (strncmp(msg, "REGISTER ", 9) == 0) {        // 9 = sizeof("REGISTER ")
                char username[USERNAME_SIZE];
                sscanf(msg + 9, "%s", username);
                pthread_mutex_lock(&server_state.mutex);

                int user_index = -1;
                for (int i = 0; i < server_state.user_count; ++i) {
                    if (strcmp(server_state.users[i].username, username) == 0) {
                        user_index = i;     // пользователь найден
                        break;
                    }
                }

                if (user_index == -1) {     // такого пользователя нет, создаём
                    user_index = server_state.user_count;
                    server_state.user_count++;
                }

                strcpy(server_state.users[user_index].username, username);
                server_state.users[user_index].dealer_socket = zmq_socket(server_state.context, ZMQ_DEALER);        // узнать что такое ZMQ_DEALER
                zmq_connect(server_state.users[user_index].dealer_socket, "inproc://users");        // узнать к чему я тут подключился
                server_state.users[user_index].connect_time = time(NULL);
                server_state.users[user_index].active = true;

                pthread_mutex_unlock(&server_state.mutex);

                log_message("К серверу подключён пользователь @%s. Активных пользователей %d", server_state.users[user_index].username, server_state.user_count);

                // подтверждение клиенту
                char response[MAX_MSG_LEN];     // нужна константа для размера ответов
                snprintf(response, sizeof(response), "REGISTERED %s", username);
                zmq_send(server_state.router_socket, identity, strlen(identity), ZMQ_SNDMORE);      // узнать что за ZNQ_SNDMORE
                zmq_send(server_state.router_socket, response, strlen(response), 0);
            } else if (strncmp(msg, "MESSAGE ", 8) == 0) {      // 8 = sizeof("MESSAGE ")
                char sender[USERNAME_SIZE], recipient[USERNAME_SIZE], msg_content[MAX_MSG_LEN];
                sscanf(msg + 8, "%s %s %[^\n]", sender, recipient, msg_content);

                time_t now = time(NULL);
                struct tm *tm_info = localtime(&now);
                char time_str[TIME_STRSIZE];
                strftime(time_str, sizeof(time_str), "%H:%M:%S", tm_info);

                if (strcmp(recipient, "all") == 0) {    // сообщение всем
                    char formatted_msg[MAX_MSG_LEN * 2];
                    snprintf(formatted_msg, sizeof(formatted_msg), "[%s] %s -> all: %s", time_str, sender, msg_content);

                    broadcast_message(formatted_msg, sender);
                    log_message("Пользователь @%s отправил сообщение всем", sender);
                } else {        // личное сообщение
                    char formatted_msg[MAX_MSG_LEN * 2];
                    snprintf(formatted_msg, sizeof(formatted_msg), "[%s] %s -> %s: %s", time_str, sender, recipient, msg_content);

                    send_to_user(recipient, formatted_msg);
                    log_message("Пользователь @%s отправил прямое сообщение пользователю %s", sender, recipient);
                    log_message("Пользователь @%s получил прямое сообщение от пользователя %s", recipient, sender);
                }
            } else if (strncmp(msg, "DELAYED ", 8) == 0) {
                // /delayed_msg @all <сообщение> будет отправлено пользователю all а не всем - исправить
                // отложенное сообщение
                char sender[USERNAME_SIZE], recipient[USERNAME_SIZE], msg_content[MAX_MSG_LEN];
                int delay_seconds;
                sscanf(msg + 8, "%d %s %s %[^\n]", &delay_seconds, sender, recipient, msg_content);
                
                time_t now = time(NULL);
                time_t delivery_time = now + delay_seconds;
                struct tm *tm_info = localtime(&delivery_time);
                char delivery_time_str[TIME_STRSIZE];
                strftime(delivery_time_str, sizeof(delivery_time_str), "%H:%M:%S", tm_info);
                
                log_message("Пользователь @%s отправил отложенное сообщение пользователю %@s. Ожидаемое время отправки: %s", sender, recipient, delivery_time_str);
            } else if (strncmp(msg, "EXIT ", 5) == 0) {
                char username[USERNAME_SIZE];
                sscanf(msg + 5, "%s", username);

                pthread_mutex_lock(&server_state.mutex);

                for (int i = 0; i < server_state.user_count; ++i) {
                    if (strcmp(server_state.users[i].username, username) == 0) {
                        server_state.users[i].active = false;
                        zmq_close(server_state.users[i].dealer_socket);
                        break;
                    }
                }

                pthread_mutex_unlock(&server_state.mutex);
                log_message("Пользователь @%s отключился от сервера", server_state.username);
            }
        }
    }

    pthread_join(delayed_pthread, NULL);
    pthread_mutex_destroy(&server_state.mutex);
    zmq_close(server_state.router_socket);
    zmq_close(server_state.pub_socket);
    zmq_ctx_destroy(server_state.context);

    log_message("Сервер остановлен");
    return 0;
}