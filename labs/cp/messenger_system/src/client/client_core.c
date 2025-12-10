#include "./client_core.h"
#include "../common/utils.h"
#include "./network_handler.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>

// ============================================
// Вспомогательные функции
// ============================================

static const char* state_to_string(ClientState state) {
    switch (state) {
        case CLIENT_STATE_DISCONNECTED: return "DISCONNECTED";
        case CLIENT_STATE_CONNECTING:   return "CONNECTING";
        case CLIENT_STATE_CONNECTED:    return "CONNECTED";
        case CLIENT_STATE_LOGGED_IN:    return "LOGGED_IN";
        case CLIENT_STATE_ERROR:        return "ERROR";
        case CLIENT_STATE_SHUTTING_DOWN: return "SHUTTING_DOWN";
        default: return "UNKNOWN";
    }
}

static void change_state(Client *client, ClientState new_state) {
    if (!client) return;
    
    pthread_mutex_lock(&client->state_mutex);
    ClientState old_state = atomic_load(&client->state);
    
    if (old_state != new_state) {
        atomic_store(&client->state, new_state);
        
        // Уведомляем ожидающих
        pthread_cond_broadcast(&client->state_cond);
        
        // Вызываем callback если есть
        if (client->on_status_change) {
            client->on_status_change(old_state, new_state, client->callback_data);
        }
        
        printf("[CLIENT] State changed: %s -> %s\n", 
               state_to_string(old_state), state_to_string(new_state));
    }
    pthread_mutex_unlock(&client->state_mutex);
}

static void queue_message(Client *client, const Message *msg) {
    if (!client || !msg) return;
    
    pthread_mutex_lock(&client->queue_mutex);
    
    // Увеличиваем очередь если нужно
    if (client->queue_size >= client->queue_capacity) {
        size_t new_capacity = client->queue_capacity * 2;
        if (new_capacity < 10) new_capacity = 10;
        
        Message *new_queue = realloc(client->message_queue, 
                                    new_capacity * sizeof(Message));
        if (!new_queue) {
            pthread_mutex_unlock(&client->queue_mutex);
            if (client->on_error) {
                client->on_error("Failed to allocate message queue", 
                               client->callback_data);
            }
            return;
        }
        
        client->message_queue = new_queue;
        client->queue_capacity = new_capacity;
    }
    
    // Добавляем сообщение
    memcpy(&client->message_queue[client->queue_size], msg, sizeof(Message));
    client->queue_size++;
    
    pthread_mutex_unlock(&client->queue_mutex);
}

// ============================================
// Основные функции
// ============================================

Client* client_create(const char *server_host, int req_port) {
    if (!server_host || req_port <= 0) {
        return NULL;
    }
    
    Client *client = (Client*)calloc(1, sizeof(Client));
    if (!client) {
        return NULL;
    }
    
    // Инициализация состояния
    atomic_init(&client->state, CLIENT_STATE_DISCONNECTED);
    atomic_init(&client->running, false);
    atomic_init(&client->threads_running, false);
    atomic_init(&client->messages_sent, 0);
    atomic_init(&client->messages_received, 0);
    atomic_init(&client->connection_attempts, 0);
    
    // Копируем параметры сервера
    safe_strcpy(client->server_host, server_host, sizeof(client->server_host));
    client->req_port = req_port;
    client->sub_port = req_port + 1;  // По умолчанию +1
    client->push_port = req_port + 2; // По умолчанию +2
    
    // Инициализация логина
    client->login[0] = '\0';
    client->status = USER_STATUS_OFFLINE;
    
    // Инициализация мьютексов и условий
    if (pthread_mutex_init(&client->state_mutex, NULL) != 0 ||
        pthread_mutex_init(&client->send_mutex, NULL) != 0 ||
        pthread_mutex_init(&client->queue_mutex, NULL) != 0 ||
        pthread_cond_init(&client->state_cond, NULL) != 0) {
        
        free(client);
        return NULL;
    }
    
    // Инициализация очереди сообщений
    client->queue_capacity = 10;
    client->queue_size = 0;
    client->message_queue = (Message*)malloc(client->queue_capacity * sizeof(Message));
    if (!client->message_queue) {
        pthread_mutex_destroy(&client->state_mutex);
        pthread_mutex_destroy(&client->send_mutex);
        pthread_mutex_destroy(&client->queue_mutex);
        pthread_cond_destroy(&client->state_cond);
        free(client);
        return NULL;
    }
    
    // Callbacks по умолчанию
    client->on_message = NULL;
    client->on_status_change = NULL;
    client->on_error = NULL;
    client->callback_data = NULL;
    
    // ZeroMQ контекст будет создан позже
    client->zmq_context = NULL;
    client->req_socket = NULL;
    client->sub_socket = NULL;
    client->push_socket = NULL;
    
    printf("[CLIENT] Created for server %s:%d\n", server_host, req_port);
    return client;
}

int client_initialize(Client *client) {
    if (!client) return -1;
    
    // Создаем ZeroMQ контекст
    client->zmq_context = zmq_ctx_new();
    if (!client->zmq_context) {
        if (client->on_error) {
            client->on_error("Failed to create ZeroMQ context", client->callback_data);
        }
        return -1;
    }
    
    // Настраиваем контекст
    int max_sockets = 3; // REQ, SUB, PUSH
    if (zmq_ctx_set(client->zmq_context, ZMQ_MAX_SOCKETS, max_sockets) != 0) {
        zmq_ctx_destroy(client->zmq_context);
        client->zmq_context = NULL;
        return -1;
    }
    
    // Инициализируем сетевой слой
    if (network_init(client) != 0) {
        zmq_ctx_destroy(client->zmq_context);
        client->zmq_context = NULL;
        return -1;
    }
    
    change_state(client, CLIENT_STATE_CONNECTED);
    return 0;
}

int client_start(Client *client) {
    if (!client) return -1;
    
    if (atomic_load(&client->running)) {
        return 0; // Уже запущен
    }
    
    atomic_store(&client->running, true);
    
    // Запускаем поток приема сообщений
    if (pthread_create(&client->recv_thread, NULL, 
                      client_receive_thread_func, client) != 0) {
        atomic_store(&client->running, false);
        return -1;
    }
    
    // Запускаем heartbeat поток
    if (pthread_create(&client->heartbeat_thread, NULL,
                      client_heartbeat_thread_func, client) != 0) {
        atomic_store(&client->running, false);
        atomic_store(&client->threads_running, false);
        pthread_join(client->recv_thread, NULL);
        return -1;
    }
    
    atomic_store(&client->threads_running, true);
    printf("[CLIENT] Started\n");
    return 0;
}

int client_stop(Client *client) {
    if (!client) return -1;
    
    if (!atomic_load(&client->running)) {
        return 0; // Уже остановлен
    }
    
    printf("[CLIENT] Stopping...\n");
    change_state(client, CLIENT_STATE_SHUTTING_DOWN);
    atomic_store(&client->running, false);
    
    // Ждем завершения потоков
    if (atomic_load(&client->threads_running)) {
        pthread_join(client->recv_thread, NULL);
        pthread_join(client->heartbeat_thread, NULL);
        atomic_store(&client->threads_running, false);
    }
    
    printf("[CLIENT] Stopped\n");
    return 0;
}

void client_destroy(Client *client) {
    if (!client) return;
    
    // Останавливаем клиент если еще работает
    if (atomic_load(&client->running)) {
        client_stop(client);
    }
    
    // Закрываем ZeroMQ сокеты
    if (client->req_socket) zmq_close(client->req_socket);
    if (client->sub_socket) zmq_close(client->sub_socket);
    if (client->push_socket) zmq_close(client->push_socket);
    if (client->zmq_context) zmq_ctx_destroy(client->zmq_context);
    
    // Освобождаем память
    free(client->message_queue);
    
    // Уничтожаем мьютексы и условия
    pthread_mutex_destroy(&client->state_mutex);
    pthread_mutex_destroy(&client->send_mutex);
    pthread_mutex_destroy(&client->queue_mutex);
    pthread_cond_destroy(&client->state_cond);
    
    free(client);
    printf("[CLIENT] Destroyed\n");
}

// ============================================
// Подключение/отключение
// ============================================

int client_connect(Client *client) {
    if (!client) return -1;
    
    // Если уже подключен
    ClientState current_state = atomic_load(&client->state);
    if (current_state >= CLIENT_STATE_CONNECTED) {
        return 0;
    }
    
    change_state(client, CLIENT_STATE_CONNECTING);
    
    // Инициализируем
    if (client_initialize(client) != 0) {
        change_state(client, CLIENT_STATE_ERROR);
        return -1;
    }
    
    change_state(client, CLIENT_STATE_CONNECTED);
    return 0;
}

int client_disconnect(Client *client) {
    if (!client) return -1;
    
    change_state(client, CLIENT_STATE_DISCONNECTED);
    
    // TODO: Реализация отключения в network_handler
    
    return 0;
}

// ============================================
// Аутентификация
// ============================================

int client_login(Client *client, const char *login) {
    if (!client || !login) return -1;
    
    // Проверяем логин
    if (!is_valid_login(login)) {
        if (client->on_error) {
            client->on_error("Invalid login format", client->callback_data);
        }
        return ERROR_INVALID_LOGIN;
    }
    
    // Проверяем состояние
    if (!client_is_connected(client)) {
        if (client->on_error) {
            client->on_error("Not connected to server", client->callback_data);
        }
        return ERROR_NOT_AUTHORIZED;
    }
    printf("[CLIENT DEBUG] Sending login request for: %s\n", login);

    // Отправляем запрос регистрации/логина
    Message msg;
    memset(&msg, 0, sizeof(Message));
    msg.type = MSG_TYPE_REGISTER; // или MSG_TYPE_LOGIN если разделяем
    msg.message_id = generate_message_id();
    msg.timestamp = get_current_timestamp();
    safe_strcpy(msg.sender, login, MAX_LOGIN_LENGTH);

    printf("[CLIENT DEBUG] Message ID: %u, timestamp: %u\n", 
           msg.message_id, msg.timestamp);
    
    ServerResponse resp;
    int send_result = network_send_request(client, &msg, &resp);
    printf("[CLIENT DEBUG] network_send_request returned: %d\n", send_result);
    // if (network_send_request(client, &msg, &resp) != 0) {
    if (send_result != 0) {
        if (client->on_error) {
            client->on_error("Failed to send login request", client->callback_data);
        }
        return ERROR_INTERNAL_SERVER;
    }
    
    // Проверяем ответ сервера
    if (resp.err_code != ERROR_SUCCESS) {
        if (client->on_error) {
            client->on_error(resp.info, client->callback_data);
        }
        return resp.err_code;
    }
    
    // Устанавливаем логин и статус
    safe_strcpy(client->login, login, MAX_LOGIN_LENGTH);
    client->status = USER_STATUS_ONLINE;
    change_state(client, CLIENT_STATE_LOGGED_IN);
    
    // ТЕПЕРЬ инициализируем SUB сокет для приема сообщений
    printf("[CLIENT] Initializing SUB socket for receiving messages...\n");

    // Закрываем старый SUB сокет, если он существует
    if (client->sub_socket) {
        zmq_close(client->sub_socket);
        client->sub_socket = NULL;
    }

    char sub_address[256];
    snprintf(sub_address, sizeof(sub_address), "tcp://%s:%d", 
            client->server_host, client->sub_port);

    // Создаем SUB сокет
    client->sub_socket = zmq_socket(client->zmq_context, ZMQ_SUB);
    if (!client->sub_socket) {
        printf("[CLIENT ERROR] Failed to create SUB socket: %s\n", zmq_strerror(errno));
        if (client->on_error) {
            client->on_error("Failed to create subscription socket", client->callback_data);
        }
        return ERROR_INTERNAL_SERVER;
    }

    // Устанавливаем HWM (High Water Mark) чтобы избежать переполнения
    int hwm = 1000; // Максимальное количество сообщений в очереди
    zmq_setsockopt(client->sub_socket, ZMQ_RCVHWM, &hwm, sizeof(hwm));

    // Подключаем SUB сокет
    if (zmq_connect(client->sub_socket, sub_address) != 0) {
        printf("[CLIENT ERROR] Failed to connect SUB socket: %s\n", zmq_strerror(errno));
        zmq_close(client->sub_socket);
        client->sub_socket = NULL;
        if (client->on_error) {
            client->on_error("Failed to connect to message stream", client->callback_data);
        }
        return ERROR_INTERNAL_SERVER;
    }

    // Подписываемся на все сообщения
    const char *filter = ""; // Пустая строка = все сообщения
    int rc = zmq_setsockopt(client->sub_socket, ZMQ_SUBSCRIBE, filter, strlen(filter));
    if (rc != 0) {
        printf("[CLIENT WARN] Failed to subscribe: %s\n", zmq_strerror(errno));
    } else {
        printf("[CLIENT] Subscribed to all messages\n");
    }

    // Устанавливаем таймаут для SUB сокета
    int timeout = 1000; // 1 секунда для теста
    zmq_setsockopt(client->sub_socket, ZMQ_RCVTIMEO, &timeout, sizeof(timeout));

    // Тестируем подписку
    printf("[CLIENT] Testing subscription...\n");
    char test_buffer[1024];
    memset(test_buffer, 0, sizeof(test_buffer));

    // Пробуем получить сообщение с коротким таймаутом
    rc = zmq_recv(client->sub_socket, test_buffer, sizeof(test_buffer) - 1, 0);
    if (rc >= 0) {
        test_buffer[rc] = '\0';
        printf("[CLIENT] Received test message: %s\n", test_buffer);
    } else if (errno == EAGAIN) {
        printf("[CLIENT] No messages yet, subscription OK\n");
    } else {
        printf("[CLIENT WARN] Subscription test warning: %s\n", zmq_strerror(errno));
    }

    // Устанавливаем бесконечный таймаут для работы потока
    int no_timeout = -1;
    zmq_setsockopt(client->sub_socket, ZMQ_RCVTIMEO, &no_timeout, sizeof(no_timeout));

    printf("[CLIENT] SUB socket connected to %s\n", sub_address);
    printf("[CLIENT] Logged in as: %s\n", login);
    
    // Уведомляем поток приема сообщений, что SUB сокет готов
    if (client->threads_running) {
        printf("[CLIENT] Notifying receive thread that SUB socket is ready\n");
    }
    
    return ERROR_SUCCESS;
}

int client_logout(Client *client) {
    if (!client) return -1;
    
    if (!client_is_logged_in(client)) {
        return 0; // Уже не залогинен
    }
    
    // TODO: Отправка запроса logout на сервер
    
    client->login[0] = '\0';
    client->status = USER_STATUS_OFFLINE;
    change_state(client, CLIENT_STATE_CONNECTED);
    
    printf("[CLIENT] Logged out\n");
    return ERROR_SUCCESS;
}

// ============================================
// Действия
// ============================================

int client_send_message(Client *client, const char *receiver, const char *text) {
    if (!client || !receiver || !text) return ERROR_INVALID_MESSAGE;
    
    if (!client_is_logged_in(client)) {
        if (client->on_error) {
            client->on_error("Not logged in", client->callback_data);
        }
        return ERROR_NOT_AUTHORIZED;
    }
    
    if (!is_valid_login(receiver)) {
        return ERROR_INVALID_LOGIN;
    }
    
    // TODO: Отправка через network_handler
    printf("[CLIENT] Would send message to %s: %s\n", receiver, text);
    
    atomic_fetch_add(&client->messages_sent, 1);
    return ERROR_SUCCESS;
}

// ============================================
// Callbacks
// ============================================

void client_set_message_callback(Client *client, MessageCallback callback, void *user_data) {
    if (!client) return;
    client->on_message = callback;
    client->callback_data = user_data;
}

void client_set_status_callback(Client *client, StatusCallback callback, void *user_data) {
    if (!client) return;
    client->on_status_change = callback;
    client->callback_data = user_data;
}

void client_set_error_callback(Client *client, ErrorCallback callback, void *user_data) {
    if (!client) return;
    client->on_error = callback;
    client->callback_data = user_data;
}

// ============================================
// Утилиты
// ============================================

ClientState client_get_state(Client *client) {
    return client ? atomic_load(&client->state) : CLIENT_STATE_ERROR;
}

const char* client_get_login(Client *client) {
    return client ? client->login : NULL;
}

bool client_is_connected(Client *client) {
    return client && (atomic_load(&client->state) >= CLIENT_STATE_CONNECTED);
}

bool client_is_logged_in(Client *client) {
    return client && (atomic_load(&client->state) == CLIENT_STATE_LOGGED_IN);
}

int client_get_message_count(Client *client) {
    return client ? atomic_load(&client->messages_received) : 0;
}

// ============================================
// Внутренние функции
// ============================================

void* client_get_zmq_context(Client *client) {
    return client ? client->zmq_context : NULL;
}

void* client_get_req_socket(Client *client) {
    return client ? client->req_socket : NULL;
}

void* client_get_sub_socket(Client *client) {
    return client ? client->sub_socket : NULL;
}

void* client_get_push_socket(Client *client) {
    return client ? client->push_socket : NULL;
}

// ============================================
// Обработка сообщений
// ============================================

int client_handle_incoming_message(Client *client, const Message *msg) {
    if (!client || !msg) return -1;
    
    atomic_fetch_add(&client->messages_received, 1);
    
    // Вызываем callback если есть
    if (client->on_message) {
        client->on_message(msg, client->callback_data);
    }
    
    return 0;
}

int client_handle_server_response(Client *client, const ServerResponse *resp) {
    if (!client || !resp) return -1;
    
    printf("[CLIENT] Server response: type=%d, error=%d, info=%s\n",
           resp->original_type, resp->err_code, resp->info);
    
    // TODO: Обработка специфичных ответов
    return 0;
}

// ============================================
// Потоки
// ============================================

void* client_receive_thread_func(void *arg) {
    Client *client = (Client*)arg;
    if (!client) return NULL;
    
    printf("[RECV THREAD] Started\n");
    
    // Ждем пока создастся SUB сокет
    while (atomic_load(&client->running) && !client->sub_socket) {
        sleep(0.1); // 100ms
    }
    
    if (!client->sub_socket) {
        printf("[RECV THREAD] No SUB socket, exiting\n");
        return NULL;
    }
    
    printf("[RECV THREAD] SUB socket ready, starting to listen\n");
    
while (atomic_load(&client->running)) {
        Message incoming;
        memset(&incoming, 0, sizeof(Message));
        
        int rc = network_receive_message(client, &incoming, 1000); // 1 секунда таймаут
        
        if (rc == 0) {
            printf("[RECV THREAD] Message received from %s: %s\n", 
                   incoming.sender, incoming.text);
            client_handle_incoming_message(client, &incoming);
        } else if (rc == -1) {
            // Критическая ошибка, возможно нужно переподключиться
            sleep(1); // Ждем секунду перед повторной попыткой
        }
        // rc == 1 это "нет данных", просто продолжаем
        
        sleep(0.01); // 10ms пауза чтобы не грузить CPU
    }
    
    printf("[RECV THREAD] Stopped\n");
    return NULL;
}

// ============================================
// Дополнительные функции (которые использует UI)
// ============================================

int client_send_broadcast(Client *client, const char *text) {
    if (!client || !text) return ERROR_INVALID_MESSAGE;
    
    if (!client_is_logged_in(client)) {
        if (client->on_error) {
            client->on_error("Not logged in", client->callback_data);
        }
        return ERROR_NOT_AUTHORIZED;
    }
    
    // Создаем broadcast сообщение
    Message msg;
    memset(&msg, 0, sizeof(Message));
    msg.type = MSG_TYPE_BROADCAST;
    msg.message_id = generate_message_id();
    msg.timestamp = get_current_timestamp();
    safe_strcpy(msg.sender, client->login, MAX_LOGIN_LENGTH);
    safe_strcpy(msg.receiver, "*", MAX_LOGIN_LENGTH);  // * означает "всем"
    safe_strcpy(msg.text, text, MAX_MESSAGE_LENGTH);
    
    // Отправляем через PUSH сокет
    return network_send_message(client, &msg);
}

int client_request_users(Client *client) {
    if (!client) return ERROR_INVALID_MESSAGE;
    
    if (!client_is_logged_in(client)) {
        if (client->on_error) {
            client->on_error("Not logged in", client->callback_data);
        }
        return ERROR_NOT_AUTHORIZED;
    }
    
    // Создаем запрос списка пользователей
    Message msg;
    memset(&msg, 0, sizeof(Message));
    msg.type = MSG_TYPE_GET_USERS;
    msg.message_id = generate_message_id();
    msg.timestamp = get_current_timestamp();
    safe_strcpy(msg.sender, client->login, MAX_LOGIN_LENGTH);
    
    // Отправляем через REQ сокет и ждем ответ
    ServerResponse resp;
    if (network_send_request(client, &msg, &resp) != 0) {
        if (client->on_error) {
            client->on_error("Failed to request user list", client->callback_data);
        }
        return ERROR_INTERNAL_SERVER;
    }
    
    // Выводим список пользователей через callback
    if (client->on_message) {
        // Создаем сообщение с результатом
        Message result_msg;
        memset(&result_msg, 0, sizeof(Message));
        result_msg.type = MSG_TYPE_SYSTEM_MESSAGE;
        result_msg.message_id = generate_message_id();
        result_msg.timestamp = get_current_timestamp();
        safe_strcpy(result_msg.sender, "system", MAX_LOGIN_LENGTH);
        safe_strcpy(result_msg.text, resp.info, MAX_MESSAGE_LENGTH);
        result_msg.flags = FLAG_SYSTEM;
        
        client->on_message(&result_msg, client->callback_data);
    }
    
    return resp.err_code;
}

int client_set_status(Client *client, UserStatus status) {
    if (!client) return ERROR_INVALID_MESSAGE;
    
    if (!client_is_logged_in(client)) {
        if (client->on_error) {
            client->on_error("Not logged in", client->callback_data);
        }
        return ERROR_NOT_AUTHORIZED;
    }
    
    // Сохраняем статус локально
    client->status = status;
    
    // Создаем сообщение смены статуса
    Message msg;
    memset(&msg, 0, sizeof(Message));
    msg.type = MSG_TYPE_SET_STATUS;
    msg.message_id = generate_message_id();
    msg.timestamp = get_current_timestamp();
    safe_strcpy(msg.sender, client->login, MAX_LOGIN_LENGTH);
    
    // Преобразуем статус в строку
    const char *status_str = status_to_string(status);
    safe_strcpy(msg.text, status_str, MAX_MESSAGE_LENGTH);
    
    // Отправляем через PUSH сокет
    return network_send_message(client, &msg);
}

int client_register(Client *client, const char *login) {
    // Для простоты регистрация = логин
    return client_login(client, login);
}

int client_reconnect(Client *client) {
    if (!client) return -1;
    
    printf("[CLIENT] Attempting to reconnect...\n");
    
    // Отключаемся
    client_disconnect(client);
    
    // Ждем немного
    sleep(1);
    
    // Пытаемся подключиться снова
    return client_connect(client);
}

void* client_heartbeat_thread_func(void *arg) {
    Client *client = (Client*)arg;
    if (!client) return NULL;
    
    printf("[HEARTBEAT THREAD] Started\n");
    
    while (atomic_load(&client->running)) {
        // Отправляем heartbeat если подключены
        if (client_is_connected(client)) {
            // TODO: Отправка heartbeat через network_handler
        }
        
        // Ждем интервал
        sleep(CLIENT_HEARTBEAT_INTERVAL);
    }
    
    printf("[HEARTBEAT THREAD] Stopped\n");
    return NULL;
}