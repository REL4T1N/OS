#include "common.h"

#define MAX_CLIENTS 100
#define MAX_DELAYED_MSGS 1000

typedef struct {
    char login[32];
    char address[256];
    time_t last_seen;
} client_info_t;

typedef struct {
    message_t msg;
    int active;
} delayed_message_t;

client_info_t clients[MAX_CLIENTS];
delayed_message_t delayed_msgs[MAX_DELAYED_MSGS];
int client_count = 0;
int delayed_count = 0;
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t delayed_mutex = PTHREAD_MUTEX_INITIALIZER;

// Добавление/обновление клиента
void update_client(const char* login, const char* address) {
    pthread_mutex_lock(&clients_mutex);
    
    int found = 0;
    for (int i = 0; i < client_count; i++) {
        if (strcmp(clients[i].login, login) == 0) {
            strcpy(clients[i].address, address);
            clients[i].last_seen = time(NULL);
            found = 1;
            break;
        }
    }
    
    // Новый клиент
    if (!found && client_count < MAX_CLIENTS) {
        strcpy(clients[client_count].login, login);
        strcpy(clients[client_count].address, address);
        clients[client_count].last_seen = time(NULL);
        client_count++;
        printf("Новый клиент: %s (всего: %d)\n", login, client_count);
    }
    
    pthread_mutex_unlock(&clients_mutex);
}

// Удаление клиента
void remove_client(const char* login) {
    pthread_mutex_lock(&clients_mutex);
    
    for (int i = 0; i < client_count; i++) {
        if (strcmp(clients[i].login, login) == 0) {
            for (int j = i; j < client_count - 1; j++) {
                clients[j] = clients[j + 1];
            }
            client_count--;
            printf("Клиент удален: %s (осталось: %d)\n", login, client_count);
            break;
        }
    }
    
    pthread_mutex_unlock(&clients_mutex);
}

// Добавление отложенного сообщения
void add_delayed_message(message_t* msg) {
    pthread_mutex_lock(&delayed_mutex);
    
    if (delayed_count < MAX_DELAYED_MSGS) {
        delayed_msgs[delayed_count].msg = *msg;
        delayed_msgs[delayed_count].active = 1;
        delayed_count++;
        
        struct tm* tm = localtime(&msg->send_time);
        printf("Отложенное сообщение добавлено: от %s к %s в %02d:%02d:%02d\n",
               msg->sender, msg->recipient,
               tm->tm_hour, tm->tm_min, tm->tm_sec);
    } else {
        printf("Достигнут лимит отложенных сообщений!\n");
    }
    
    pthread_mutex_unlock(&delayed_mutex);
}

// Обработчик отложенных сообщений
void* delayed_messages_thread(void* arg) {
    void* context = zmq_ctx_new();
    void* publisher = zmq_socket(context, ZMQ_PUB);
    zmq_bind(publisher, "tcp://*:5556");
    
    printf("Поток отложенных сообщений запущен\n");
    
    while (1) {
        time_t now = time(NULL);
        
        pthread_mutex_lock(&delayed_mutex);
        
        for (int i = 0; i < delayed_count; i++) {
            if (delayed_msgs[i].active && 
                delayed_msgs[i].msg.send_time <= now) {
                
                // Отправляем отложенное сообщение
                zmq_msg_t msg;
                zmq_msg_init_size(&msg, sizeof(message_t));
                memcpy(zmq_msg_data(&msg), &delayed_msgs[i].msg, sizeof(message_t));
                zmq_msg_send(&msg, publisher, 0);
                zmq_msg_close(&msg);
                
                struct tm* tm = localtime(&now);
                printf("[%02d:%02d:%02d] Отправлено отложенное сообщение от %s к %s: %s\n",
                       tm->tm_hour, tm->tm_min, tm->tm_sec,
                       delayed_msgs[i].msg.sender, 
                       delayed_msgs[i].msg.recipient,
                       delayed_msgs[i].msg.text);
                
                // Удаляем отправленное сообщение
                delayed_msgs[i].active = 0;
            }
        }
        
        pthread_mutex_unlock(&delayed_mutex);
        
        sleep(1); // Проверяем каждую секунду
    }
    
    return NULL;
}

int main() {
    printf("Запуск сервера чата...\n");
    
    // Инициализация delayed_messages
    memset(delayed_msgs, 0, sizeof(delayed_msgs));
    
    // Запуск потока для отложенных сообщений
    pthread_t delayed_thread;
    pthread_create(&delayed_thread, NULL, delayed_messages_thread, NULL);
    
    void* context = zmq_ctx_new();
    
    // Сокет для приема сообщений от клиентов
    void* receiver = zmq_socket(context, ZMQ_PULL);
    int rc = zmq_bind(receiver, "tcp://*:7777");
    if (rc != 0) {
        perror("Ошибка привязки сокета 7777");
        return 1;
    }
    
    // Сокет для публикации сообщений всем
    void* publisher = zmq_socket(context, ZMQ_PUB);
    rc = zmq_bind(publisher, "tcp://*:7778");
    if (rc != 0) {
        perror("Ошибка привязки сокета 7778");
        return 1;
    }
    
    printf("Сервер запущен на портах 7777 (вход) и 7778 (рассылка)\n");
    
    while (1) {
        message_t msg;
        zmq_msg_t zmq_msg;
        
        // Получаем сообщение
        zmq_msg_init(&zmq_msg);
        if (zmq_msg_recv(&zmq_msg, receiver, 0) == -1) {
            perror("Ошибка приема сообщения");
            continue;
        }
        
        // Проверяем размер сообщения
        if (zmq_msg_size(&zmq_msg) != sizeof(message_t)) {
            printf("Получено сообщение неверного размера: %ld байт\n", 
                   zmq_msg_size(&zmq_msg));
            zmq_msg_close(&zmq_msg);
            continue;
        }
        
        memcpy(&msg, zmq_msg_data(&zmq_msg), sizeof(message_t));
        zmq_msg_close(&zmq_msg);
        
        // Обрабатываем тип сообщения
        switch (msg.type) {
            case MSG_TYPE_JOIN: {
                struct tm* tm = localtime(&msg.send_time);
                printf("[%02d:%02d:%02d] %s присоединился к чату\n", 
                       tm->tm_hour, tm->tm_min, tm->tm_sec, msg.sender);
                update_client(msg.sender, msg.text); // text хранит адрес
                break;
            }
                
            case MSG_TYPE_LEAVE: {
                struct tm* tm = localtime(&msg.send_time);
                printf("[%02d:%02d:%02d] %s покинул чат\n", 
                       tm->tm_hour, tm->tm_min, tm->tm_sec, msg.sender);
                remove_client(msg.sender);
                break;
            }
                
            case MSG_TYPE_TEXT: {
                struct tm* tm = localtime(&msg.send_time);
                printf("[%02d:%02d:%02d] %s -> %s: %s\n", 
                       tm->tm_hour, tm->tm_min, tm->tm_sec,
                       msg.sender, msg.recipient, msg.text);
                
                // Отправляем всем подписчикам
                zmq_msg_t pub_msg;
                zmq_msg_init_size(&pub_msg, sizeof(message_t));
                memcpy(zmq_msg_data(&pub_msg), &msg, sizeof(message_t));
                zmq_msg_send(&pub_msg, publisher, 0);
                zmq_msg_close(&pub_msg);
                break;
            }
                
            case MSG_TYPE_DELAYED: {
                struct tm* tm = localtime(&msg.send_time);
                printf("[%02d:%02d:%02d] Отложенное сообщение от %s\n", 
                       tm->tm_hour, tm->tm_min, tm->tm_sec, msg.sender);
                
                // Сохраняем отложенное сообщение
                add_delayed_message(&msg);
                break;
            }
                
            default:
                printf("Неизвестный тип сообщения: %d\n", msg.type);
        }
    }
    
    zmq_close(receiver);
    zmq_close(publisher);
    zmq_ctx_destroy(context);
    
    return 0;
}