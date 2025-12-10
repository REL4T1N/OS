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
    time_t original_send_time; // Когда должно быть отправлено
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
        delayed_msgs[delayed_count].original_send_time = msg->send_time;
        delayed_count++;
        
        struct tm* tm = localtime(&msg->send_time);
        printf("Отложенное сообщение добавлено: от %s к %s (отправка в %02d:%02d:%02d, сейчас: ", 
               msg->sender, msg->recipient,
               tm->tm_hour, tm->tm_min, tm->tm_sec);
        
        time_t now = time(NULL);
        struct tm* now_tm = localtime(&now);
        printf("%02d:%02d:%02d)\n", 
               now_tm->tm_hour, now_tm->tm_min, now_tm->tm_sec);
    } else {
        printf("Достигнут лимит отложенных сообщений!\n");
    }
    
    pthread_mutex_unlock(&delayed_mutex);
}

// Обработчик отложенных сообщений
void* delayed_messages_thread(void* arg) {
    void* context = zmq_ctx_new();
    void* publisher = zmq_socket(context, ZMQ_PUB);
    
    if (zmq_bind(publisher, "tcp://*:7778") != 0) {
        printf("Ошибка привязки сокета 7778\n");
        return NULL;
    }
    
    printf("Поток отложенных сообщений запущен на порту 7778\n");
    
    while (1) {
        time_t now = time(NULL);
        
        pthread_mutex_lock(&delayed_mutex);
        
        for (int i = 0; i < delayed_count; i++) {
            if (delayed_msgs[i].active && 
                delayed_msgs[i].original_send_time <= now) {
                
                // Меняем тип на обычное сообщение перед отправкой
                delayed_msgs[i].msg.type = MSG_TYPE_TEXT;
                
                // Обновляем время отправки на текущее
                delayed_msgs[i].msg.send_time = now;
                
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
                
                delayed_msgs[i].active = 0;
            }
        }
        
        pthread_mutex_unlock(&delayed_mutex);
        
        sleep(1);
    }
    
    return NULL;
}

int main() {
    printf("Запуск сервера чата...\n");
    
    memset(delayed_msgs, 0, sizeof(delayed_msgs));
    
    // Запуск потока для отложенных сообщений
    pthread_t delayed_thread;
    pthread_create(&delayed_thread, NULL, delayed_messages_thread, NULL);
    
    void* context = zmq_ctx_new();
    
    // Сокет для приема сообщений от клиентов
    void* receiver = zmq_socket(context, ZMQ_PULL);
    
    if (zmq_bind(receiver, "tcp://*:7777") != 0) {
        printf("Ошибка привязки сокета 7777\n");
        return 1;
    }
    
    // Сокет для рассылки сообщений клиентам
    void* publisher = zmq_socket(context, ZMQ_PUB);
    if (zmq_bind(publisher, "tcp://*:7779") != 0) {
        printf("Ошибка привязки сокета 7779\n");
        return 1;
    }
    
    printf("Основной сокет запущен на порту 7777\n");
    printf("Сокет рассылки запущен на порту 7779\n");
    printf("Сервер запущен. Ожидание подключений...\n");
    
    while (1) {
        message_t msg;
        zmq_msg_t zmq_msg;
        
        zmq_msg_init(&zmq_msg);
        if (zmq_msg_recv(&zmq_msg, receiver, 0) == -1) {
            continue;
        }
        
        if (zmq_msg_size(&zmq_msg) != sizeof(message_t)) {
            zmq_msg_close(&zmq_msg);
            continue;
        }
        
        memcpy(&msg, zmq_msg_data(&zmq_msg), sizeof(message_t));
        zmq_msg_close(&zmq_msg);
        
        // Обрабатываем тип сообщения
        time_t now = time(NULL);
        struct tm* tm = localtime(&now);
        
        switch (msg.type) {
            case MSG_TYPE_JOIN:
                printf("[%02d:%02d:%02d] %s присоединился к чату\n", 
                       tm->tm_hour, tm->tm_min, tm->tm_sec, msg.sender);
                update_client(msg.sender, msg.text);
                break;
                
            case MSG_TYPE_LEAVE:
                printf("[%02d:%02d:%02d] %s покинул чат\n", 
                       tm->tm_hour, tm->tm_min, tm->tm_sec, msg.sender);
                remove_client(msg.sender);
                break;
                
            case MSG_TYPE_TEXT:
                printf("[%02d:%02d:%02d] %s -> %s: %s\n", 
                    tm->tm_hour, tm->tm_min, tm->tm_sec,
                    msg.sender, msg.recipient, msg.text);
                
                // Не рассылаем сообщения, адресованные себе
                if (strcmp(msg.sender, msg.recipient) != 0) {
                    // Рассылаем сообщение всем клиентам
                    zmq_msg_t pub_msg;
                    zmq_msg_init_size(&pub_msg, sizeof(message_t));
                    memcpy(zmq_msg_data(&pub_msg), &msg, sizeof(message_t));
                    zmq_msg_send(&pub_msg, publisher, 0);
                    zmq_msg_close(&pub_msg);
                }
                break;
                
            case MSG_TYPE_DELAYED: {
                time_t send_time = msg.send_time;
                struct tm* send_tm = localtime(&send_time);
                printf("[%02d:%02d:%02d] Отложенное сообщение от %s к %s (отправка в %02d:%02d:%02d): %s\n", 
                       tm->tm_hour, tm->tm_min, tm->tm_sec,
                       msg.sender, msg.recipient,
                       send_tm->tm_hour, send_tm->tm_min, send_tm->tm_sec,
                       msg.text);
                add_delayed_message(&msg);
                break;
            }
                
            default:
                printf("[%02d:%02d:%02d] Неизвестный тип сообщения: %d\n", 
                       tm->tm_hour, tm->tm_min, tm->tm_sec, msg.type);
        }
    }
    
    return 0;
}