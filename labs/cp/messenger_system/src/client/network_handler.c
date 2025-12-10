#include "network_handler.h"
#include "../common/utils.h"
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

// ============================================
// Вспомогательные функции
// ============================================

static int create_socket(void *context, int type, const char *address) {
    void *socket = zmq_socket(context, type);
    if (!socket) {
        fprintf(stderr, "[NETWORK] Failed to create socket: %s\n", zmq_strerror(errno));
        return -1;
    }
    
    // Настраиваем таймауты
    int timeout = 1000; // 1 секунда
    if (type == ZMQ_REQ || type == ZMQ_SUB) {
        zmq_setsockopt(socket, ZMQ_RCVTIMEO, &timeout, sizeof(timeout));
    }
    if (type == ZMQ_REQ || type == ZMQ_PUSH) {
        zmq_setsockopt(socket, ZMQ_SNDTIMEO, &timeout, sizeof(timeout));
    }
    
    // Подключаемся/привязываем
    if (type == ZMQ_SUB) {
        // SUB сокет подключается
        if (zmq_connect(socket, address) != 0) {
            fprintf(stderr, "[NETWORK] Failed to connect %s: %s\n", address, zmq_strerror(errno));
            zmq_close(socket);
            return -1;
        }
    } else {
        // REQ и PUSH сокеты подключаются
        if (zmq_connect(socket, address) != 0) {
            fprintf(stderr, "[NETWORK] Failed to connect %s: %s\n", address, zmq_strerror(errno));
            zmq_close(socket);
            return -1;
        }
    }
    
    return 0;
}

static char* build_address(const char *host, int port) {
    static char address[256];
    snprintf(address, sizeof(address), "tcp://%s:%d", host, port);
    return address;
}

// ============================================
// Основные функции
// ============================================

int network_init(Client *client) {
    if (!client || !client->zmq_context) {
        return -1;
    }
    
    printf("[NETWORK] Initializing sockets...\n");
    
    // Создаем адреса
    char req_address[256], push_address[256];
    snprintf(req_address, sizeof(req_address), "tcp://%s:%d", 
             client->server_host, client->req_port);
    snprintf(push_address, sizeof(push_address), "tcp://%s:%d", 
             client->server_host, client->push_port);
    
    printf("  REQ:  %s\n", req_address);
    printf("  PUSH: %s\n", push_address);
    printf("  SUB:  tcp://%s:%d (will be created after login)\n", 
           client->server_host, client->sub_port);
    
    // Создаем REQ сокет (для команд)
    client->req_socket = zmq_socket(client->zmq_context, ZMQ_REQ);
    if (!client->req_socket) {
        fprintf(stderr, "[NETWORK] Failed to create REQ socket: %s\n", zmq_strerror(errno));
        return -1;
    }
    
    // Настраиваем таймаут для REQ
    int timeout = 5000; // 5 секунд
    zmq_setsockopt(client->req_socket, ZMQ_RCVTIMEO, &timeout, sizeof(timeout));
    zmq_setsockopt(client->req_socket, ZMQ_SNDTIMEO, &timeout, sizeof(timeout));
    
    // Подключаем REQ
    if (zmq_connect(client->req_socket, req_address) != 0) {
        fprintf(stderr, "[NETWORK] Failed to connect REQ: %s\n", zmq_strerror(errno));
        zmq_close(client->req_socket);
        client->req_socket = NULL;
        return -1;
    }
    printf("[NETWORK] REQ socket connected\n");
    
    // Создаем PUSH сокет (для отправки сообщений)
    client->push_socket = zmq_socket(client->zmq_context, ZMQ_PUSH);
    if (!client->push_socket) {
        fprintf(stderr, "[NETWORK] Failed to create PUSH socket: %s\n", zmq_strerror(errno));
        zmq_close(client->req_socket);
        client->req_socket = NULL;
        return -1;
    }
    
    // Подключаем PUSH
    if (zmq_connect(client->push_socket, push_address) != 0) {
        fprintf(stderr, "[NETWORK] Failed to connect PUSH: %s\n", zmq_strerror(errno));
        zmq_close(client->req_socket);
        zmq_close(client->push_socket);
        client->req_socket = NULL;
        client->push_socket = NULL;
        return -1;
    }
    printf("[NETWORK] PUSH socket connected\n");
    
    // SUB сокет будет создан после логина
    client->sub_socket = NULL;
    
    // printf("[NETWORK] Initialization complete\n");
    printf("[NETWORK] REQ and PUSH sockets initialized, SUB socket will be created after login\n");
    return 0;
}

void network_cleanup(Client *client) {
    if (!client) return;
    
    printf("[NETWORK] Cleaning up...\n");
    
    if (client->req_socket) {
        zmq_close(client->req_socket);
        client->req_socket = NULL;
    }
    
    if (client->sub_socket) {
        zmq_close(client->sub_socket);
        client->sub_socket = NULL;
    }
    
    if (client->push_socket) {
        zmq_close(client->push_socket);
        client->push_socket = NULL;
    }
    
    printf("[NETWORK] Cleanup complete\n");
}

int network_send_request(Client *client, Message *msg, ServerResponse *resp) {
    if (!client || !msg || !resp) {
        return -1;
    }
    
    if (!client->req_socket) {
        fprintf(stderr, "[NETWORK] REQ socket not initialized\n");
        return -1;
    }
    
    // Устанавливаем ID и timestamp если не установлены
    if (msg->message_id == 0) {
        msg->message_id = generate_message_id();
    }
    if (msg->timestamp == 0) {
        msg->timestamp = get_current_timestamp();
    }
    
    // Отправляем сообщение
    int rc = zmq_send(client->req_socket, msg, sizeof(Message), 0);
    if (rc == -1) {
        fprintf(stderr, "[NETWORK] Failed to send request: %s\n", zmq_strerror(errno));
        return -1;
    }
    
    printf("[NETWORK] Sent request type %d, id %u\n", msg->type, msg->message_id);
    
    // Получаем ответ
    memset(resp, 0, sizeof(ServerResponse));
    rc = zmq_recv(client->req_socket, resp, sizeof(ServerResponse), 0);
    if (rc == -1) {
        if (errno == EAGAIN) {
            fprintf(stderr, "[NETWORK] Request timeout\n");
        } else {
            fprintf(stderr, "[NETWORK] Failed to receive response: %s\n", zmq_strerror(errno));
        }
        return -1;
    }
    
    printf("[NETWORK] Received response: error=%d, info=%s\n", 
           resp->err_code, resp->info);
    
    return 0;
}

int network_send_message(Client *client, Message *msg) {
    if (!client || !msg) {
        return -1;
    }
    
    if (!client->push_socket) {
        fprintf(stderr, "[NETWORK] PUSH socket not initialized\n");
        return -1;
    }
    
    // Устанавливаем ID и timestamp если не установлены
    if (msg->message_id == 0) {
        msg->message_id = generate_message_id();
    }
    if (msg->timestamp == 0) {
        msg->timestamp = get_current_timestamp();
    }
    
    // Отправляем через PUSH сокет
    int rc = zmq_send(client->push_socket, msg, sizeof(Message), 0);
    if (rc == -1) {
        fprintf(stderr, "[NETWORK] Failed to send message: %s\n", zmq_strerror(errno));
        return -1;
    }
    
    printf("[NETWORK] Sent message from %s to %s: %s\n", 
           msg->sender, msg->receiver, msg->text);
    
    return 0;
}

int network_send_heartbeat(Client *client) {
    if (!client || !client_is_logged_in(client)) {
        return -1;
    }
    
    Message heartbeat;
    memset(&heartbeat, 0, sizeof(Message));
    heartbeat.type = MSG_TYPE_SET_STATUS;
    heartbeat.message_id = generate_message_id();
    heartbeat.timestamp = get_current_timestamp();
    safe_strcpy(heartbeat.sender, client->login, MAX_LOGIN_LENGTH);
    safe_strcpy(heartbeat.receiver, "*", MAX_LOGIN_LENGTH);
    safe_strcpy(heartbeat.text, "heartbeat", MAX_MESSAGE_LENGTH);
    
    // Используем PUSH для heartbeat
    return network_send_message(client, &heartbeat);
}

int network_receive_message(Client *client, Message *msg, int timeout_ms) {
    if (!client || !msg || !client->sub_socket) {
        return -1;
    }
    
    printf("[NETWORK DEBUG] === Attempting zmq_recv on SUB socket ===\n");
    
    // Сохраняем текущий timeout
    int original_timeout;
    size_t size = sizeof(original_timeout);
    int rc = zmq_getsockopt(client->sub_socket, ZMQ_RCVTIMEO, &original_timeout, &size);
    if (rc != 0) {
        printf("[NETWORK DEBUG] Failed to get socket timeout: %s\n", zmq_strerror(errno));
    }
    
    // Устанавливаем короткий таймаут для неблокирующего чтения
    int new_timeout = 100; // 100ms
    rc = zmq_setsockopt(client->sub_socket, ZMQ_RCVTIMEO, &new_timeout, sizeof(new_timeout));
    if (rc != 0) {
        printf("[NETWORK DEBUG] Failed to set timeout: %s\n", zmq_strerror(errno));
    }
    
    // Пробуем получить сообщение
    char buffer[2048]; // Увеличим буфер
    memset(buffer, 0, sizeof(buffer));
    
    rc = zmq_recv(client->sub_socket, buffer, sizeof(buffer) - 1, 0);
    
    // Восстанавливаем оригинальный timeout
    zmq_setsockopt(client->sub_socket, ZMQ_RCVTIMEO, &original_timeout, sizeof(original_timeout));
    
    if (rc == -1) {
        if (errno == EAGAIN) {
            printf("[NETWORK DEBUG] No data available (timeout)\n");
            return 1;
        }
        fprintf(stderr, "[NETWORK DEBUG] zmq_recv failed: %s (errno=%d)\n", 
                zmq_strerror(errno), errno);
        return -1;
    }
    
    buffer[rc] = '\0';
    printf("[NETWORK DEBUG] SUCCESS! Received %d bytes\n", rc);
    printf("[NETWORK DEBUG] Raw data: '%s'\n", buffer);
    
    // Парсим сообщение
    memset(msg, 0, sizeof(Message));
    msg->message_id = generate_message_id();
    msg->timestamp = time(NULL);
    
    // Формат: "SYS:system:*:Welcome test_user to the chat!:1765394719"
    // Или: "MSG:sender:receiver:text:timestamp"
    
    // Создаем копию буфера для токенизации
    char buffer_copy[2048];
    strcpy(buffer_copy, buffer);
    
    char *parts[5] = {NULL};
    char *token = strtok(buffer_copy, ":");
    int part_index = 0;
    
    while (token && part_index < 5) {
        parts[part_index++] = token;
        token = strtok(NULL, ":");
    }
    
    if (part_index >= 5) {
        // У нас есть все части
        if (strcmp(parts[0], "SYS") == 0) {
            msg->type = MSG_TYPE_SYSTEM_MESSAGE;
            msg->flags = FLAG_SYSTEM;
        } else if (strcmp(parts[0], "MSG") == 0) {
            msg->type = MSG_TYPE_CHAT_MESSAGE;
        } else if (strcmp(parts[0], "OFFLINE") == 0) {
            msg->type = MSG_TYPE_CHAT_MESSAGE;
            msg->flags = FLAG_OFFLINE_STORE;
        }
        
        safe_strcpy(msg->sender, parts[1], MAX_LOGIN_LENGTH);
        safe_strcpy(msg->receiver, parts[2], MAX_LOGIN_LENGTH);
        safe_strcpy(msg->text, parts[3], MAX_MESSAGE_LENGTH);
        if (parts[4]) {
            msg->timestamp = atol(parts[4]);
        }
    } else {
        // Простой текст
        safe_strcpy(msg->text, buffer, MAX_MESSAGE_LENGTH);
    }
    
    printf("[NETWORK DEBUG] Parsed message: %s -> %s: %s\n", 
           msg->sender, msg->receiver, msg->text);
    
    return 0;
}

int network_receive_broadcast(Client *client, char *buffer, size_t buffer_size) {
    if (!client || !buffer || buffer_size == 0) {
        return -1;
    }
    
    if (!client->sub_socket) {
        return -1;
    }
    
    // Для broadcast сообщений (строковый формат)
    memset(buffer, 0, buffer_size);
    int rc = zmq_recv(client->sub_socket, buffer, buffer_size - 1, 0);
    
    if (rc == -1) {
        if (errno == EAGAIN) {
            return 1; // Таймаут
        }
        return -1;
    }
    
    buffer[rc] = '\0'; // Гарантируем нуль-терминатор
    return 0;
}

int network_subscribe(Client *client, const char *filter) {
    if (!client || !filter || !client->sub_socket) {
        return -1;
    }
    
    int rc = zmq_setsockopt(client->sub_socket, ZMQ_SUBSCRIBE, filter, strlen(filter));
    if (rc != 0) {
        fprintf(stderr, "[NETWORK] Failed to subscribe to '%s': %s\n", 
                filter, zmq_strerror(errno));
        return -1;
    }
    
    printf("[NETWORK] Subscribed to: %s\n", filter);
    return 0;
}

int network_unsubscribe(Client *client, const char *filter) {
    if (!client || !filter || !client->sub_socket) {
        return -1;
    }
    
    int rc = zmq_setsockopt(client->sub_socket, ZMQ_UNSUBSCRIBE, filter, strlen(filter));
    if (rc != 0) {
        fprintf(stderr, "[NETWORK] Failed to unsubscribe from '%s': %s\n", 
                filter, zmq_strerror(errno));
        return -1;
    }
    
    printf("[NETWORK] Unsubscribed from: %s\n", filter);
    return 0;
}

const char* network_get_last_error(void) {
    return zmq_strerror(errno);
}

int network_check_connection(Client *client) {
    if (!client) {
        return 0;
    }
    
    // Проверяем REQ сокет отправкой тестового сообщения
    Message test_msg;
    memset(&test_msg, 0, sizeof(Message));
    test_msg.type = MSG_TYPE_GET_USERS;
    test_msg.message_id = generate_message_id();
    test_msg.timestamp = get_current_timestamp();
    safe_strcpy(test_msg.sender, "test", MAX_LOGIN_LENGTH);
    
    ServerResponse resp;
    int rc = network_send_request(client, &test_msg, &resp);
    
    if (rc == 0) {
        return 1; // Соединение работает
    }
    
    // Если не удалось, пытаемся переподключиться
    printf("[NETWORK] Connection lost, attempting to reconnect...\n");
    
    network_cleanup(client);
    sleep(1);
    
    if (network_init(client) == 0) {
        printf("[NETWORK] Reconnected successfully\n");
        return 1;
    }
    
    printf("[NETWORK] Reconnection failed\n");
    return 0;
}

// тестовая функция
int network_test_subscription(Client *client) {
    if (!client || !client->sub_socket) {
        printf("[NETWORK ERROR] No SUB socket for testing\n");
        return -1;
    }
    
    printf("[NETWORK] Testing subscription...\n");
    
    // Сохраняем текущий таймаут
    int original_timeout;
    size_t size = sizeof(original_timeout);
    zmq_getsockopt(client->sub_socket, ZMQ_RCVTIMEO, &original_timeout, &size);
    
    // Устанавливаем короткий таймаут для теста
    int test_timeout = 100; // 100ms
    zmq_setsockopt(client->sub_socket, ZMQ_RCVTIMEO, &test_timeout, sizeof(test_timeout));
    
    // Пробуем получить сообщение
    char buffer[1024];
    memset(buffer, 0, sizeof(buffer));
    
    int rc = zmq_recv(client->sub_socket, buffer, sizeof(buffer) - 1, 0);
    
    // Восстанавливаем оригинальный таймаут
    zmq_setsockopt(client->sub_socket, ZMQ_RCVTIMEO, &original_timeout, sizeof(original_timeout));
    
    if (rc == -1) {
        if (errno == EAGAIN) {
            printf("[NETWORK] No messages yet, subscription seems OK\n");
            return 0;
        } else {
            printf("[NETWORK ERROR] Subscription test failed: %s (errno=%d)\n", 
                   zmq_strerror(errno), errno);
            return -1;
        }
    } else {
        buffer[rc] = '\0';
        printf("[NETWORK] Received test message: %s\n", buffer);
        return 0;
    }
}