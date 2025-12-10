#include "common.h"
#include <locale.h>

volatile int running = 1;
pthread_mutex_t display_mutex = PTHREAD_MUTEX_INITIALIZER;

// Структура для передачи данных в поток
typedef struct {
    void* context;
    char login[32];
} thread_data_t;

// Функция для безопасного вывода
void safe_print(const char* format, ...) {
    pthread_mutex_lock(&display_mutex);
    
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    
    pthread_mutex_unlock(&display_mutex);
}

// Функция для очистки строки ввода
void clear_input_line() {
    printf("\r\033[K");
    fflush(stdout);
}

void* receive_messages(void* arg) {
    thread_data_t* data = (thread_data_t*)arg;
    void* context = data->context;
    char* login = data->login;
    
    void* subscriber = zmq_socket(context, ZMQ_SUB);
    zmq_connect(subscriber, "tcp://127.0.0.1:7779"); // Исправляем порт
    zmq_setsockopt(subscriber, ZMQ_SUBSCRIBE, "", 0);
    
    while (running) {
        message_t msg;
        zmq_msg_t zmq_msg;
        
        zmq_msg_init(&zmq_msg);
        if (zmq_msg_recv(&zmq_msg, subscriber, ZMQ_DONTWAIT) == -1) {
            usleep(100000);
            continue;
        }
        
        if (zmq_msg_size(&zmq_msg) != sizeof(message_t)) {
            zmq_msg_close(&zmq_msg);
            continue;
        }
        
        memcpy(&msg, zmq_msg_data(&zmq_msg), sizeof(message_t));
        zmq_msg_close(&zmq_msg);
        
        // Показываем только если сообщение адресовано нам или всем
        if (strcmp(msg.recipient, "ALL") == 0 || 
            strcmp(msg.recipient, login) == 0) {
            
            if (strcmp(msg.sender, login) != 0) {
                clear_input_line();
                print_time();
                safe_print("%s: %s\n", msg.sender, msg.text);
                printf("%s> ", login);
                fflush(stdout);
            }
        }
    }
    
    zmq_close(subscriber);
    free(data);
    return NULL;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printf("Использование: %s <логин>\n", argv[0]);
        return 1;
    }

    setlocale(LC_ALL, "");
    setlocale(LC_CTYPE, "UTF-8");
    
    char login[32];
    strcpy(login, argv[1]);
    
    void* context = zmq_ctx_new();
    
    // Сокет для отправки сообщений на сервер
    void* sender = zmq_socket(context, ZMQ_PUSH);
    zmq_connect(sender, "tcp://127.0.0.1:7777");
    
    // Отправляем сообщение о подключении
    message_t join_msg;
    memset(&join_msg, 0, sizeof(message_t));
    strcpy(join_msg.sender, login);
    strcpy(join_msg.recipient, "SERVER");
    strcpy(join_msg.text, "присоединился");
    join_msg.type = MSG_TYPE_JOIN;
    join_msg.send_time = time(NULL);
    
    zmq_msg_t msg;
    zmq_msg_init_size(&msg, sizeof(message_t));
    memcpy(zmq_msg_data(&msg), &join_msg, sizeof(message_t));
    zmq_msg_send(&msg, sender, 0);
    zmq_msg_close(&msg);
    
    printf("Добро пожаловать, %s!\n", login);
    printf("Команды:\n");
    printf("  @user сообщение    - отправить пользователю\n");
    printf("  ALL сообщение      - отправить всем\n");
    printf("  сообщение          - отправить себе (для заметок)\n");
    printf("  delay @user N текст - отложить сообщение на N секунд\n");
    printf("  exit               - выход\n");
    printf("\n");
    
    // Подготовка данных для потока
    thread_data_t* thread_data = malloc(sizeof(thread_data_t));
    thread_data->context = context;
    strcpy(thread_data->login, login);
    
    // Запускаем поток для приема сообщений
    pthread_t recv_thread;
    pthread_create(&recv_thread, NULL, receive_messages, thread_data);
    
    // Основной цикл ввода
    char input[512];
    while (1) {
        printf("%s> ", login);
        fflush(stdout);
        
        if (!fgets(input, sizeof(input), stdin)) {
            break;
        }
        
        input[strcspn(input, "\n")] = 0;
        
        if (strcmp(input, "exit") == 0) {
            break;
        }
        
        if (strlen(input) == 0) {
            continue;
        }
        
        // Парсим команду
        message_t new_msg;
        memset(&new_msg, 0, sizeof(message_t));
        strcpy(new_msg.sender, login);
        new_msg.type = MSG_TYPE_TEXT;
        new_msg.send_time = time(NULL);
        
        if (strncmp(input, "@", 1) == 0) {
            // Личное сообщение
            char* space = strchr(input, ' ');
            if (space) {
                *space = 0;
                strcpy(new_msg.recipient, input + 1); // Пропускаем @
                strcpy(new_msg.text, space + 1);
            } else {
                clear_input_line();
                printf("Неправильный формат. Используйте: @user сообщение\n");
                printf("%s> ", login);
                fflush(stdout);
                continue;
            }
        } else if (strncmp(input, "ALL ", 4) == 0) {
            // Широковещательное сообщение
            strcpy(new_msg.recipient, "ALL");
            strcpy(new_msg.text, input + 4);
        } else if (strncmp(input, "delay ", 6) == 0) {
            // Отложенное сообщение - новая логика парсинга
            new_msg.type = MSG_TYPE_DELAYED;
            
            // Парсим: delay @user время текст
            char* args = input + 6;
            char* recipient_str = strtok(args, " ");
            char* time_str = strtok(NULL, " ");
            char* text = strtok(NULL, "");
            
            if (!recipient_str || !time_str || !text) {
                clear_input_line();
                printf("Неправильный формат. Используйте: delay @user время сообщение\n");
                printf("Пример: delay @Bob 10 Привет через 10 секунд!\n");
                printf("%s> ", login);
                fflush(stdout);
                continue;
            }
            
            // Получатель
            if (recipient_str[0] == '@') {
                strcpy(new_msg.recipient, recipient_str + 1);
            } else if (strcmp(recipient_str, "ALL") == 0) {
                strcpy(new_msg.recipient, "ALL");
            } else if (strcmp(recipient_str, "me") == 0) {
                strcpy(new_msg.recipient, login);
            } else {
                // Возможно это число (время) - значит получатель ALL
                char* endptr;
                long num = strtol(recipient_str, &endptr, 10);
                if (*endptr == '\0') {
                    // Это число - значит формат delay время текст
                    strcpy(new_msg.recipient, "ALL");
                    text = time_str;  // Сдвигаем аргументы
                    time_str = recipient_str;
                    recipient_str = "ALL";
                } else {
                    strcpy(new_msg.recipient, recipient_str);
                }
            }
            
            // Время
            int delay_seconds = atoi(time_str);
            if (delay_seconds <= 0) {
                clear_input_line();
                printf("Время должно быть положительным числом секунд\n");
                printf("%s> ", login);
                fflush(stdout);
                continue;
            }
            
            new_msg.send_time = time(NULL) + delay_seconds;
            strcpy(new_msg.text, text);
            
            clear_input_line();
            printf("Отложенное сообщение для %s будет отправлено через %d секунд\n", 
                   new_msg.recipient, delay_seconds);
            printf("%s> ", login);
            fflush(stdout);
            
            // Продолжаем - отправим сообщение
        } else {
            // По умолчанию - отправляем себе (для заметок)
            strcpy(new_msg.recipient, login);
            strcpy(new_msg.text, input);
        }
        
        // Отправляем сообщение
        zmq_msg_t zmq_msg;
        zmq_msg_init_size(&zmq_msg, sizeof(message_t));
        memcpy(zmq_msg_data(&zmq_msg), &new_msg, sizeof(message_t));
        zmq_msg_send(&zmq_msg, sender, 0);
        zmq_msg_close(&zmq_msg);
    }
    
    running = 0;
    
    message_t leave_msg;
    memset(&leave_msg, 0, sizeof(message_t));
    strcpy(leave_msg.sender, login);
    strcpy(leave_msg.recipient, "SERVER");
    strcpy(leave_msg.text, "покинул чат");
    leave_msg.type = MSG_TYPE_LEAVE;
    leave_msg.send_time = time(NULL);
    
    zmq_msg_init_size(&msg, sizeof(message_t));
    memcpy(zmq_msg_data(&msg), &leave_msg, sizeof(message_t));
    zmq_msg_send(&msg, sender, 0);
    zmq_msg_close(&msg);
    
    pthread_join(recv_thread, NULL);
    
    zmq_close(sender);
    zmq_ctx_destroy(context);
    
    printf("\nДо свидания, %s!\n", login);
    return 0;
}