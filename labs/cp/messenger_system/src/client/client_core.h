#ifndef CLIENT_CORE_H
#define CLIENT_CORE_H

#include "../common/protocol.h"
#include <zmq.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>

// ============================================
// Конфигурация
// ============================================

#define CLIENT_RECV_TIMEOUT_MS 1000
#define CLIENT_HEARTBEAT_INTERVAL 30
#define CLIENT_RECONNECT_DELAY 3000

// ============================================
// Состояния клиента
// ============================================

typedef enum {
    CLIENT_STATE_DISCONNECTED = 0,
    CLIENT_STATE_CONNECTING,
    CLIENT_STATE_CONNECTED,
    CLIENT_STATE_LOGGED_IN,
    CLIENT_STATE_ERROR,
    CLIENT_STATE_SHUTTING_DOWN
} ClientState;

// ============================================
// Callback-типы для обработки событий
// ============================================

typedef void (*MessageCallback)(const Message *msg, void *user_data);
typedef void (*StatusCallback)(ClientState old_state, ClientState new_state, void *user_data);
typedef void (*ErrorCallback)(const char *error_msg, void *user_data);

// ============================================
// Структура клиента
// ============================================

typedef struct {
    // === ZeroMQ ===
    void *zmq_context;
    void *req_socket;    // REQ для команд (порт 5555)
    void *sub_socket;    // SUB для получения (порт 5556)  
    void *push_socket;   // PUSH для отправки (порт 5557)
    
    // === Состояние ===
    atomic_int state;
    atomic_bool running;
    char login[MAX_LOGIN_LENGTH];
    UserStatus status;
    
    // === Серверные адреса ===
    char server_host[256];
    int req_port;
    int sub_port;
    int push_port;
    
    // === Потоки ===
    pthread_t recv_thread;
    pthread_t heartbeat_thread;
    atomic_bool threads_running;
    
    // === Callbacks ===
    MessageCallback on_message;
    StatusCallback on_status_change;
    ErrorCallback on_error;
    void *callback_data;
    
    // === Статистика ===
    atomic_uint messages_sent;
    atomic_uint messages_received;
    atomic_uint connection_attempts;
    
    // === Синхронизация ===
    pthread_mutex_t state_mutex;
    pthread_cond_t state_cond;
    pthread_mutex_t send_mutex;  // Для сериализации отправки
    
    // === Буферы ===
    Message *message_queue;
    size_t queue_size;
    size_t queue_capacity;
    pthread_mutex_t queue_mutex;
    
} Client;

// ============================================
// Основной API клиента
// ============================================

// Жизненный цикл
Client* client_create(const char *server_host, int req_port);
int client_initialize(Client *client);
int client_start(Client *client);
int client_stop(Client *client);
void client_destroy(Client *client);

// Подключение/отключение
int client_connect(Client *client);
int client_disconnect(Client *client);
int client_reconnect(Client *client);

// Аутентификация
int client_login(Client *client, const char *login);
int client_logout(Client *client);
int client_register(Client *client, const char *login);

// Действия
int client_send_message(Client *client, const char *receiver, const char *text);
int client_send_broadcast(Client *client, const char *text);
int client_request_users(Client *client);
int client_set_status(Client *client, UserStatus status);

// Callbacks
void client_set_message_callback(Client *client, MessageCallback callback, void *user_data);
void client_set_status_callback(Client *client, StatusCallback callback, void *user_data);
void client_set_error_callback(Client *client, ErrorCallback callback, void *user_data);

// Утилиты
ClientState client_get_state(Client *client);
const char* client_get_login(Client *client);
bool client_is_connected(Client *client);
bool client_is_logged_in(Client *client);
int client_get_message_count(Client *client);

// Внутренние функции (для использования в network_handler)
void* client_get_zmq_context(Client *client);
void* client_get_req_socket(Client *client);
void* client_get_sub_socket(Client *client);
void* client_get_push_socket(Client *client);

// Обработка входящих сообщений (вызывается из network_handler)
int client_handle_incoming_message(Client *client, const Message *msg);
int client_handle_server_response(Client *client, const ServerResponse *resp);

// Потоки
void* client_receive_thread_func(void *arg);
void* client_heartbeat_thread_func(void *arg);

#endif // CLIENT_CORE_H