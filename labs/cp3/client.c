#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <zmq.h>
#include <unistd.h>
#include <pthread.h>
#include <stdbool.h>

volatile bool running = true;

void *receive_messages(void *socket_ptr) {
    void *socket = socket_ptr;
    
    while (running) {
        zmq_msg_t identity_msg;
        zmq_msg_init(&identity_msg);
        
        if (zmq_msg_recv(&identity_msg, socket, 0) < 0) {
            if (running) {
                perror("Ошибка при получении идентификатора");
            }
            zmq_msg_close(&identity_msg);
            usleep(100000); // 100ms
            continue;
        }
        
        zmq_msg_t empty_msg;
        zmq_msg_init(&empty_msg);
        zmq_msg_recv(&empty_msg, socket, 0);
        zmq_msg_close(&empty_msg);
        
        zmq_msg_t message_msg;
        zmq_msg_init(&message_msg);
        
        if (zmq_msg_recv(&message_msg, socket, 0) < 0) {
            zmq_msg_close(&identity_msg);
            zmq_msg_close(&message_msg);
            continue;
        }
        
        char *message = (char*)zmq_msg_data(&message_msg);
        size_t message_len = zmq_msg_size(&message_msg);
        
        // Выводим сообщение
        printf("\n>> %.*s\n", (int)message_len, message);
        printf("Введите сообщение (@имя текст): ");
        fflush(stdout);
        
        zmq_msg_close(&identity_msg);
        zmq_msg_close(&message_msg);
    }
    
    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Использование: %s <имя_клиента>\n", argv[0]);
        return 1;
    }
    
    char *client_name = argv[1];
    printf("Клиент '%s' запущен\n", client_name);
    printf("Подключение к серверу...\n");
    
    void *context = zmq_ctx_new();
    void *socket = zmq_socket(context, ZMQ_DEALER);
    
    // Подключаемся к серверу
    if (zmq_connect(socket, "tcp://localhost:5555") != 0) {
        perror("Ошибка подключения к серверу");
        zmq_close(socket);
        zmq_ctx_destroy(context);
        return 1;
    }
    
    printf("Подключено к серверу\n");
    
    // Отправляем имя для регистрации
    zmq_msg_t reg_msg;
    zmq_msg_init_size(&reg_msg, strlen(client_name));
    memcpy(zmq_msg_data(&reg_msg), client_name, strlen(client_name));
    
    if (zmq_msg_send(&reg_msg, socket, 0) < 0) {
        perror("Ошибка при регистрации");
        zmq_msg_close(&reg_msg);
        zmq_close(socket);
        zmq_ctx_destroy(context);
        return 1;
    }
    
    zmq_msg_close(&reg_msg);
    printf("Зарегистрирован на сервере как '%s'\n", client_name);
    printf("Для отправки сообщения используйте формат: @имя текст\n");
    printf("Пример: @bob привет!\n");
    printf("Для выхода нажмите Ctrl+C\n\n");
    
    // Запускаем поток для приема сообщений
    pthread_t thread;
    if (pthread_create(&thread, NULL, receive_messages, socket) != 0) {
        perror("Ошибка создания потока");
        zmq_close(socket);
        zmq_ctx_destroy(context);
        return 1;
    }
    
    // Основной цикл для отправки сообщений
    char input[256];
    while (running) {
        printf("Введите сообщение (@имя текст): ");
        fflush(stdout);
        
        if (fgets(input, sizeof(input), stdin)) {
            // Проверяем выход
            if (strcmp(input, "exit\n") == 0 || strcmp(input, "quit\n") == 0) {
                running = false;
                break;
            }
            
            // Убираем символ новой строки
            input[strcspn(input, "\n")] = 0;
            
            if (strlen(input) > 0) {
                zmq_msg_t msg;
                zmq_msg_init_size(&msg, strlen(input));
                memcpy(zmq_msg_data(&msg), input, strlen(input));
                
                if (zmq_msg_send(&msg, socket, 0) < 0) {
                    perror("Ошибка отправки сообщения");
                } else {
                    printf("Сообщение отправлено: %s\n", input);
                }
                
                zmq_msg_close(&msg);
            }
        }
    }
    
    printf("Завершение работы...\n");
    running = false;
    
    // Даем потоку время завершиться
    usleep(100000);
    
    pthread_cancel(thread);
    pthread_join(thread, NULL);
    
    zmq_close(socket);
    zmq_ctx_destroy(context);
    
    printf("Клиент завершил работу\n");
    return 0;
}