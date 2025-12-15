// simple_client.c
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <zmq.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>

#define MAX_MSG_SIZE 256

// Структура для потока приема
typedef struct {
    char client_id[20];
} thread_data_t;

// Функция для приема сообщений
void* receive_messages(void *arg) {
    thread_data_t *data = (thread_data_t*)arg;
    
    void *context = zmq_ctx_new();
    void *subscriber = zmq_socket(context, ZMQ_SUB);
    
    // Подключаемся к серверу для получения сообщений
    if (zmq_connect(subscriber, "tcp://localhost:5555") != 0) {
        printf("Ошибка подключения получателя: %s\n", zmq_strerror(errno));
        return NULL;
    }
    
    // Подписываемся на все сообщения
    zmq_setsockopt(subscriber, ZMQ_SUBSCRIBE, "", 0);
    
    printf("Получатель для %s запущен\n", data->client_id);
    
    char buffer[MAX_MSG_SIZE];
    while (1) {
        memset(buffer, 0, sizeof(buffer));
        int size = zmq_recv(subscriber, buffer, sizeof(buffer) - 1, 0);
        
        if (size > 0) {
            buffer[size] = '\0';
            
            // Проверяем, не наше ли это сообщение
            char search_str[50];
            snprintf(search_str, sizeof(search_str), "[%s]:", data->client_id);
            
            if (strstr(buffer, search_str) == NULL) {
                // Это не наше сообщение, показываем его
                printf("\n>>> %s\n", buffer);
                printf("%s > ", data->client_id);
                fflush(stdout);
            }
        }
        usleep(10000);
    }
    
    zmq_close(subscriber);
    zmq_ctx_destroy(context);
    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Использование: %s <client_id>\n", argv[0]);
        printf("Пример: %s bob\n", argv[0]);
        return 1;
    }
    
    // Создаем контекст для отправителя
    void *context = zmq_ctx_new();
    void *publisher = zmq_socket(context, ZMQ_PUB);
    
    // Подключаемся к серверу для отправки сообщений
    if (zmq_connect(publisher, "tcp://localhost:5556") != 0) {
        printf("Ошибка подключения отправителя: %s\n", zmq_strerror(errno));
        return 1;
    }
    
    // Даем время на установление соединения (важно для PUB)
    printf("Подключение...\n");
    sleep(1);
    
    // Создаем поток для приема сообщений
    pthread_t thread;
    thread_data_t thread_data;
    strncpy(thread_data.client_id, argv[1], sizeof(thread_data.client_id) - 1);
    
    if (pthread_create(&thread, NULL, receive_messages, &thread_data) != 0) {
        printf("Ошибка создания потока\n");
        return 1;
    }
    
    printf("Клиент %s запущен. Вводите сообщения (Ctrl+C для выхода):\n", argv[1]);
    
    char message[MAX_MSG_SIZE];
    while (1) {
        printf("%s > ", argv[1]);
        fflush(stdout);
        
        if (fgets(message, sizeof(message), stdin) != NULL) {
            // Убираем символ новой строки
            message[strcspn(message, "\n")] = 0;
            
            if (strlen(message) > 0) {
                // Формируем полное сообщение с идентификатором
                char full_message[MAX_MSG_SIZE + 50];
                snprintf(full_message, sizeof(full_message), "[%s]: %s", argv[1], message);
                
                printf("Отправляю: %s\n", full_message);
                
                // Отправляем сообщение
                if (zmq_send(publisher, full_message, strlen(full_message), 0) == -1) {
                    printf("Ошибка отправки: %s\n", zmq_strerror(errno));
                } else {
                    printf("Сообщение отправлено\n");
                }
            }
        }
    }
    
    zmq_close(publisher);
    zmq_ctx_destroy(context);
    return 0;
}