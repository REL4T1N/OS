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
    time_t original_send_time;
} delayed_message_t;

client_info_t clients[MAX_CLIENTS];
delayed_message_t delayed_msgs[MAX_DELAYED_MSGS];
int client_count = 0;
int delayed_count = 0;
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t delayed_mutex = PTHREAD_MUTEX_INITIALIZER;

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

void add_delayed_message(message_t* msg) {
    pthread_mutex_lock(&delayed_mutex);
    
    if (delayed_count < MAX_DELAYED_MSGS) {
        delayed_msgs[delayed_count].msg = *msg;
        delayed_msgs[delayed_count].active = 1;
        delayed_msgs[delayed_count].original_send_time = msg->send_time;
        delayed_count++;
        
        time_t now = time(NULL);
        int diff = (int)difftime(msg->send_time, now);
        
        printf("[");
        print_time_struct(now);
        printf("] Отложено: %s -> %s\n", msg->sender, msg->recipient);
        printf("    Создано: ");
        print_time_struct(msg->created_time);
        printf(", Отправка: ");
        print_time_struct(msg->send_time);
        printf(" (через %d сек)\n", diff);
        printf("    Текст: %s\n", msg->text);
    } else {
        printf("Достигнут лимит отложенных сообщений!\n");
    }
    
    pthread_mutex_unlock(&delayed_mutex);
}

// Обработчик отложенных сообщений теперь получает publisher как аргумент
void* delayed_messages_thread(void* arg) {
    void* publisher = (void*)arg; // publisher передается как аргумент
    
    printf("Поток отложенных сообщений запущен\n");
    
    while (1) {
        time_t now = time(NULL);
        
        pthread_mutex_lock(&delayed_mutex);
        
        for (int i = 0; i < delayed_count; i++) {
            if (delayed_msgs[i].active && 
                delayed_msgs[i].original_send_time <= now) {
                
                // Меняем тип на обычное сообщение
                delayed_msgs[i].msg.type = MSG_TYPE_TEXT;
                delayed_msgs[i].msg.send_time = now;
                
                // Отправляем через ОСНОВНОЙ publisher (порт 7779)
                zmq_msg_t msg;
                zmq_msg_init_size(&msg, sizeof(message_t));
                memcpy(zmq_msg_data(&msg), &delayed_msgs[i].msg, sizeof(message_t));
                zmq_msg_send(&msg, publisher, 0);
                zmq_msg_close(&msg);
            
                printf("[");
                print_time_struct(now);
                printf("] Отправлено отложенное сообщение: %s -> %s\n",
                       delayed_msgs[i].msg.sender, 
                       delayed_msgs[i].msg.recipient);
                printf("    Создано: ");
                print_time_struct(delayed_msgs[i].msg.created_time);
                printf(", должно было отправиться: ");
                print_time_struct(delayed_msgs[i].original_send_time);
                printf("\n    Текст: %s\n", delayed_msgs[i].msg.text);
                    
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
    
    void* context = zmq_ctx_new();
    
    // Сокет для приема сообщений от клиентов
    void* receiver = zmq_socket(context, ZMQ_PULL);
    if (zmq_bind(receiver, "tcp://*:7777") != 0) {
        printf("Ошибка привязки сокета 7777\n");
        return 1;
    }
    
    // Сокет для рассылки сообщений клиентам (ОСНОВНОЙ)
    void* publisher = zmq_socket(context, ZMQ_PUB);
    if (zmq_bind(publisher, "tcp://*:7779") != 0) {
        printf("Ошибка привязки сокета 7779\n");
        return 1;
    }
    
    printf("Сервер запущен:\n");
    printf("  7777 - прием сообщений от клиентов\n");
    printf("  7779 - рассылка сообщений клиентам\n");
    printf("Ожидание подключений...\n\n");
    
    // Запускаем поток для отложенных сообщений, передаем publisher
    pthread_t delayed_thread;
    pthread_create(&delayed_thread, NULL, delayed_messages_thread, publisher);
    
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
        
        time_t now = time(NULL);
        struct tm* tm = localtime(&now);
        
        switch (msg.type) {
            case MSG_TYPE_JOIN:
                printf("[");
                print_time_struct(now);
                printf("] %s присоединился\n", msg.sender);
                update_client(msg.sender, msg.text);
                break;
                
            case MSG_TYPE_LEAVE:
                printf("[");
                print_time_struct(now);
                printf("] %s вышел\n", msg.sender);
                remove_client(msg.sender);
                break;
                
            // case MSG_TYPE_TEXT:
            //     printf("[");
            //     print_time_struct(now);
            //     printf("] %s -> %s: %s\n", 
            //            msg.sender, msg.recipient, msg.text);
            //     printf("    Создано: ");
            //     print_time_struct(msg.created_time);
            //     printf("\n");
                
            //     // Рассылаем только если не себе
            //     if (strcmp(msg.sender, msg.recipient) != 0) {
            //         zmq_msg_t pub_msg;
            //         zmq_msg_init_size(&pub_msg, sizeof(message_t));
            //         memcpy(zmq_msg_data(&pub_msg), &msg, sizeof(message_t));
            //         zmq_msg_send(&pub_msg, publisher, 0);
            //         zmq_msg_close(&pub_msg);
            //     }
            //     break;
            case MSG_TYPE_TEXT:
                printf("[");
                print_time_struct(now);
                printf("] %s -> %s: %s\n", 
                    msg.sender, msg.recipient, msg.text);
                printf("    Создано: ");
                print_time_struct(msg.created_time);
                printf("\n");
                
                // УБИРАЕМ проверку "если не себе" - рассылаем ВСЕ сообщения
                // Включая сообщения себе
                zmq_msg_t pub_msg;
                zmq_msg_init_size(&pub_msg, sizeof(message_t));
                memcpy(zmq_msg_data(&pub_msg), &msg, sizeof(message_t));
                zmq_msg_send(&pub_msg, publisher, 0);
                zmq_msg_close(&pub_msg);
                
                // Если получатель = отправитель, это сообщение себе
                // Оно тоже должно быть доставлено
                break;
                
            case MSG_TYPE_DELAYED: {
                
                // Парсим задержку из текста
                if (strncmp(msg.text, "DELAY:", 6) == 0) {
                    int delay_seconds = atoi(msg.text + 6);
                    char* colon = strchr(msg.text + 6, ':');
                    if (colon) {
                        msg.send_time = now + delay_seconds;
                        strcpy(msg.text, colon + 1);  // Извлекаем оригинальный текст
                    }
                }
                
                printf("[");
                print_time_struct(now);
                printf("] Получено отложенное сообщение от %s\n", msg.sender);
                
                add_delayed_message(&msg);
                break;
            }
                
            default:
                printf("[");
                print_time_struct(now);
                printf("] Неизвестный тип: %d\n", msg.type);
        }
    }
    
    return 0;
}