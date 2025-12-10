#ifndef SERVER_CORE_H
#define SERVER_CORE_H

#include "../common/protocol.h"
#include "../common/utils.h"
#include "./user_manager.h"      
#include "./message_store.h"     
#include <zmq.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>            
#include <time.h>             

// ============================================
// Конфигурация сервера
// ============================================

#define SERVER_DEFAULT_PORT 5555
#define SERVER_PUB_PORT 5556
#define SERVER_PULL_PORT 5557
#define MAX_CLIENTS 1000
#define SERVER_TIMEOUT_MS 1000
#define INACTIVE_TIMEOUT 300

// ============================================
// Структура сервера
// ============================================

typedef struct {
    void *zmq_context;  // ZeroMQ контекст
    void *rep_socket;   // REQ/REP - сокеты для регистрации и команд
    void *pub_socket;   // PUB/SUB - сокеты для рассылки сообщений
    void *pull_socket;  // PUSH/PULL - для приема сообщений от клиентов

    atomic_int running;
    atomic_int shutdown_requested;

    int port;
    char *bind_address;

    // статистика
    atomic_ullong messages_processed;
    atomic_ullong clients_connected;
    time_t start_time;

    pthread_mutex_t stats_mutex;

    FILE *log_file;
    struct UserManager *user_manager;
    struct MessageStore *message_store;
} Server;

// ============================================
// Основные функции сервера
// ============================================

Server *create_server(const char *bind_address, int port);
int run_server(Server *server);
void stop_server(Server *server);
int server_is_running(Server *server);
void destroy_server(Server *server);

// ============================================
// Вспомогательные функции
// ============================================

void server_init_signals(void);
void server_signal_handler(int sig);
int server_init_sockets(Server *server);
void server_close_sockets(Server *server);
void server_process_message(Server *server, Message *msg, void *response_socket);  // обработка сообщения сервером
int server_handle_broadcast(Server *server, Message *msg);
int server_send_response(Server *server, void *socket, MessageType original_type, uint32_t original_id, ErrorCode err_code, const char *info);
// Троеточие позволяет функции принимать переменное число аргументов
void server_log(Server *server, const char *level, const char *format, ...);

// для обработки сообщений
int server_handle_register(Server *server, Message *msg, void *client_socket);
int server_handle_login(Server *server, Message *msg, void *client_socket);
int server_handle_logout(Server *server, Message *msg);
int server_handle_text_message(Server *server, Message *msg);
int server_handle_get_users(Server *server, Message *msg, void *client_socket);
int server_handle_set_status(Server *server, Message *msg);

// Работа с offline-сообщениями
int server_deliver_offline_messages(Server *server, const char *login);

// Макросы для логирования
#define SERVER_LOG_ERROR(server, format, ...) \
    server_log(server, "ERROR", format, ##__VA_ARGS__)
#define SERVER_LOG_INFO(server, format, ...) \
    server_log(server, "INFO", format, ##__VA_ARGS__)
#define SERVER_LOG_DEBUG(server, format, ...) \
    server_log(server, "DEBUG", format, ##__VA_ARGS__)
#define SERVER_LOG_WARN(server, format, ...) \
    server_log(server, "WARN", format, ##__VA_ARGS__)

#endif