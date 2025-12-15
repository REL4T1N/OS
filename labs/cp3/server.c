#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <zmq.h>
#include <unistd.h>

typedef struct {
    char name[32];
    unsigned char identity[256];
    size_t identity_size;
} Client;

Client clients[10];
int client_count = 0;

int find_client_index_by_name(const char *name) {
    for (int i = 0; i < client_count; i++) {
        if (strcmp(clients[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}

int find_client_index_by_identity(unsigned char *identity, size_t size) {
    for (int i = 0; i < client_count; i++) {
        if (clients[i].identity_size == size && 
            memcmp(clients[i].identity, identity, size) == 0) {
            return i;
        }
    }
    return -1;
}

int register_client(unsigned char *identity, size_t identity_size, const char *name) {
    int idx = find_client_index_by_identity(identity, identity_size);
    
    if (idx >= 0) {
        // Клиент уже существует - обновляем имя
        strncpy(clients[idx].name, name, 31);
        clients[idx].name[31] = '\0';
        printf("[СЕРВЕР] Клиент обновил имя на: %s\n", name);
        return 1;
    }
    
    if (client_count >= 10) {
        printf("[СЕРВЕР] Достигнут лимит клиентов\n");
        return 0;
    }
    
    // Добавляем нового клиента
    memcpy(clients[client_count].identity, identity, identity_size);
    clients[client_count].identity_size = identity_size;
    strncpy(clients[client_count].name, name, 31);
    clients[client_count].name[31] = '\0';
    client_count++;
    
    printf("[СЕРВЕР] Зарегистрирован клиент: %s\n", name);
    return 1;
}

int main() {
    printf("Запуск сервера...\n");
    
    void *context = zmq_ctx_new();
    void *socket = zmq_socket(context, ZMQ_ROUTER);
    zmq_bind(socket, "tcp://*:5555");
    
    printf("Сервер слушает на tcp://*:5555\n");
    printf("Ожидание подключений...\n");
    
    while (1) {
        // Получаем идентификатор клиента
        zmq_msg_t identity_msg;
        zmq_msg_init(&identity_msg);
        if (zmq_msg_recv(&identity_msg, socket, 0) < 0) {
            perror("Ошибка при получении идентификатора");
            continue;
        }
        
        unsigned char *identity = (unsigned char*)zmq_msg_data(&identity_msg);
        size_t identity_size = zmq_msg_size(&identity_msg);
        
        // Получаем пустой разделитель
        zmq_msg_t empty_msg;
        zmq_msg_init(&empty_msg);
        zmq_msg_recv(&empty_msg, socket, 0);
        zmq_msg_close(&empty_msg);
        
        // Получаем сообщение от клиента
        zmq_msg_t message_msg;
        zmq_msg_init(&message_msg);
        if (zmq_msg_recv(&message_msg, socket, 0) < 0) {
            perror("Ошибка при получении сообщения");
            zmq_msg_close(&identity_msg);
            continue;
        }
        
        char *message = (char*)zmq_msg_data(&message_msg);
        size_t message_len = zmq_msg_size(&message_msg);
        
        // Создаем нуль-терминированную строку
        char message_str[256];
        size_t copy_size = message_len < 255 ? message_len : 255;
        memcpy(message_str, message, copy_size);
        message_str[copy_size] = '\0';
        
        printf("\n[СЕРВЕР] Получено сообщение: '%s'\n", message_str);
        printf("  От клиента с ID размером: %zu байт\n", identity_size);
        
        // Проверяем, регистрация это или сообщение
        if (strncmp(message_str, "@", 1) == 0) {
            // Сообщение для другого клиента
            char *space = strchr(message_str, ' ');
            if (space) {
                *space = '\0';
                char *target_name = message_str + 1; // Пропускаем '@'
                char *text = space + 1;
                
                printf("[СЕРВЕР] Пересылаю сообщение клиенту '%s': %s\n", 
                       target_name, text);
                
                // Находим целевого клиента
                int target_idx = find_client_index_by_name(target_name);
                if (target_idx >= 0) {
                    // Отправляем сообщение целевому клиенту
                    zmq_msg_t target_identity_msg;
                    zmq_msg_init_size(&target_identity_msg, clients[target_idx].identity_size);
                    memcpy(zmq_msg_data(&target_identity_msg), 
                           clients[target_idx].identity, 
                           clients[target_idx].identity_size);
                    
                    zmq_msg_send(&target_identity_msg, socket, ZMQ_SNDMORE);
                    
                    // Пустой разделитель
                    zmq_msg_t empty2_msg;
                    zmq_msg_init_size(&empty2_msg, 0);
                    zmq_msg_send(&empty2_msg, socket, ZMQ_SNDMORE);
                    
                    // Текст сообщения
                    zmq_msg_t reply_msg;
                    zmq_msg_init_size(&reply_msg, strlen(text));
                    memcpy(zmq_msg_data(&reply_msg), text, strlen(text));
                    zmq_msg_send(&reply_msg, socket, 0);
                    
                    zmq_msg_close(&target_identity_msg);
                    zmq_msg_close(&empty2_msg);
                    zmq_msg_close(&reply_msg);
                    
                    printf("[СЕРВЕР] Сообщение отправлено клиенту '%s'\n", target_name);
                } else {
                    printf("[СЕРВЕР] ОШИБКА: Клиент '%s' не найден\n", target_name);
                    
                    // Отправляем сообщение об ошибке отправителю
                    zmq_msg_send(&identity_msg, socket, ZMQ_SNDMORE);
                    
                    zmq_msg_t empty_err_msg;
                    zmq_msg_init_size(&empty_err_msg, 0);
                    zmq_msg_send(&empty_err_msg, socket, ZMQ_SNDMORE);
                    
                    char error_msg[100];
                    snprintf(error_msg, sizeof(error_msg), 
                             "Ошибка: клиент '%s' не найден", target_name);
                    zmq_msg_t err_msg;
                    zmq_msg_init_size(&err_msg, strlen(error_msg));
                    memcpy(zmq_msg_data(&err_msg), error_msg, strlen(error_msg));
                    zmq_msg_send(&err_msg, socket, 0);
                    
                    zmq_msg_close(&empty_err_msg);
                    zmq_msg_close(&err_msg);
                }
            } else {
                printf("[СЕРВЕР] ОШИБКА: Неправильный формат сообщения\n");
            }
        } else {
            // Регистрация клиента
            register_client(identity, identity_size, message_str);
        }
        
        // Очищаем сообщения
        zmq_msg_close(&identity_msg);
        zmq_msg_close(&message_msg);
    }
    
    zmq_close(socket);
    zmq_ctx_destroy(context);
    return 0;
}