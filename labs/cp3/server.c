// simple_server.c
#include <stdio.h>
#include <string.h>
#include <zmq.h>
#include <unistd.h>

int main() {
    void *context = zmq_ctx_new();
    void *publisher = zmq_socket(context, ZMQ_PUB);
    void *subscriber = zmq_socket(context, ZMQ_SUB);
    
    // Биндим PUB сокет для отправки сообщений клиентам
    int rc = zmq_bind(publisher, "tcp://*:5555");
    if (rc != 0) {
        printf("Ошибка bind publisher: %s\n", zmq_strerror(errno));
        return 1;
    }
    
    // Биндим SUB сокет для получения сообщений от клиентов
    rc = zmq_bind(subscriber, "tcp://*:5556");
    if (rc != 0) {
        printf("Ошибка bind subscriber: %s\n", zmq_strerror(errno));
        return 1;
    }
    
    // Подписываемся на все сообщения
    zmq_setsockopt(subscriber, ZMQ_SUBSCRIBE, "", 0);
    
    printf("Сервер запущен...\n");
    printf("Клиенты подключаются к tcp://localhost:5555 для получения сообщений\n");
    printf("Клиенты подключаются к tcp://localhost:5556 для отправки сообщений\n");
    
    // Основной цикл сервера
    char buffer[256];
    while (1) {
        // Получаем сообщение от клиента
        memset(buffer, 0, sizeof(buffer));
        int size = zmq_recv(subscriber, buffer, sizeof(buffer) - 1, 0);
        
        if (size > 0) {
            buffer[size] = '\0';
            printf("Сервер получил: %s\n", buffer);
            
            // Пересылаем всем подключенным клиентам
            zmq_send(publisher, buffer, strlen(buffer), 0);
            printf("Сервер отправил: %s\n", buffer);
        }
        
        usleep(1000); // Небольшая пауза
    }
    
    zmq_close(publisher);
    zmq_close(subscriber);
    zmq_ctx_destroy(context);
    
    return 0;
}