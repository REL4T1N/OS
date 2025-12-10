#include "./utils.h"
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <time.h>
#include <stdarg.h>
#include <errno.h>

// ============================================
// Глобальные переменные для логгера
// ============================================

static FILE *log_file = NULL;
static int log_initialized = 0;

// ============================================
// Вспомогательные функции из protocol.h
// ============================================

int is_valid_login(const char *login) {
    if (!login || login[0] == '\0') return 0;

    size_t len = strlen(login);
    if (len == 0 || len >= MAX_LOGIN_LENGTH) return 0;

    for (size_t i = 0; i < len; i++) {
        char c = login[i];
        if (!isalnum((unsigned char)c) && c != '_' && c != '-') {
            return 0;
        }
    }

    return 1;
}

int is_valid_message(const Message *msg) {
    if (!msg) return 0;

    if (msg->type < MSG_TYPE_REGISTER || msg->type > MSG_TYPE_CHAT_MESSAGE) return 0;

    size_t text_len = strlen(msg->text);
    if (text_len >= MAX_MESSAGE_LENGTH) {
        return 0;
    }

    if (msg->type == MSG_TYPE_TEXT_MESSAGE || msg->type == MSG_TYPE_BROADCAST || msg->type == MSG_TYPE_CHAT_MESSAGE) {
        if (!is_valid_login(msg->sender)) return 0;
    }

    return 1;
}

uint32_t get_current_timestamp() {
    return (uint32_t)time(NULL);
}

uint32_t generate_message_id() {
    static uint32_t counter = 0;
    return (get_current_timestamp() << 16) | (++counter & 0xFFFF); // чё нахуй мы сделали сейчас
}

// ============================================
// Функции сериализации/десериализации
// ============================================

size_t serialize_message(const Message *msg, char *buffer, size_t buffer_size) {
    if (!msg || !buffer || buffer_size < sizeof(Message)) return 0;

    memcpy(buffer, msg, sizeof(Message)); // чо бля
    return sizeof(Message); // нахуя
}

int deserialize_message(const char *buffer, size_t buffer_size, Message *msg) {
    if (!buffer || !msg || buffer_size < sizeof(Message)) return 0;

    memcpy(msg, buffer, sizeof(Message));   // каво бля, нахуй, что просиходит

    if (!is_valid_message(msg)) {
        memset(msg, 0, sizeof(Message));
        return -1;
    }

    return 0;
}

size_t serialize_userlist(const UserList *list, char *buffer, size_t buffer_size) {
    if (!list || !buffer) return 0;

    size_t required_size = sizeof(uint64_t) + list->count * sizeof(UserInfo);
    if (buffer_size < required_size) return 0;

    memcpy(buffer, &list->count, sizeof(uint64_t));
    if (list->count > 0) {
        memcpy(buffer + sizeof(uint64_t), list->users, list->count * sizeof(UserInfo));
    }

    return required_size;
}

int desirialize_userlist(const char *buffer, size_t buffer_size, UserList *list) {
    if (!buffer || !list) return -1;

    if (buffer_size < sizeof(uint64_t)) return -1;

    memcpy(&list->count, buffer, sizeof(uint64_t));

    if (list->count > MAX_USERS) {
        list->count = 0;
        return -1;
    }

    size_t required_size = sizeof(uint64_t) + list->count * sizeof(UserInfo);
    if (buffer_size < required_size) {
        list->count = 0;
        return -1;
    }

    if (list->count > 0) {
        memcpy(list->users, buffer + sizeof(uint64_t), list->count * sizeof(UserInfo));
    }

    return 0;
}

// ============================================
// Функции форматирования/парсинга
// ============================================

void format_message_for_display(const Message *msg, char *output, size_t output_size) {
    if (!msg || !output || output_size == 0) {
        if (output && output_size > 0) {
            output[0] = '\0';
        }
        return;
    }
    
    // Преобразуем timestamp в читаемое время
    char time_str[16];
    time_t msg_time = msg->timestamp;
    struct tm *tm_info = localtime(&msg_time);
    strftime(time_str, sizeof(time_str), "%H:%M", tm_info);
    
    // Форматируем сообщение
    if (msg->flags & FLAG_SYSTEM) {
        snprintf(output, output_size, "[%s] %s", time_str, msg->text);
    } else {
        snprintf(output, output_size, "[%s] <%s>: %s", 
                time_str, msg->sender, msg->text);
    }
    
    // Гарантируем завершающий ноль
    output[output_size - 1] = '\0';
}

int parse_user_input(const char *input, char *command, char *arg1, char *arg2) {
    if (!input || !command) {
        return -1;
    }
    
    // Инициализируем выходные буферы
    command[0] = '\0';
    if (arg1) arg1[0] = '\0';
    if (arg2) arg2[0] = '\0';
    
    // Пропускаем начальные пробелы
    while (*input && isspace((unsigned char)*input)) {
        input++;
    }
    
    if (*input == '\0') {
        return 0; // Пустая строка
    }
    
    // Проверяем, является ли это командой (начинается с '/')
    if (*input == '/') {
        // Это команда
        input++; // Пропускаем '/'
        
        // Извлекаем команду
        const char *end = input;
        while (*end && !isspace((unsigned char)*end)) {
            end++;
        }
        
        size_t cmd_len = end - input;
        if (cmd_len >= MAX_LOGIN_LENGTH) {
            cmd_len = MAX_LOGIN_LENGTH - 1;
        }
        
        strncpy(command, input, cmd_len);
        command[cmd_len] = '\0';
        
        // Парсим аргументы если они есть
        input = end;
        if (arg1) {
            while (*input && isspace((unsigned char)*input)) {
                input++;
            }
            
            if (*input) {
                end = input;
                while (*end && !isspace((unsigned char)*end)) {
                    end++;
                }
                
                size_t arg_len = end - input;
                if (arg_len >= MAX_LOGIN_LENGTH) {
                    arg_len = MAX_LOGIN_LENGTH - 1;
                }
                
                strncpy(arg1, input, arg_len);
                arg1[arg_len] = '\0';
                
                input = end;
            }
        }
        
        if (arg2) {
            while (*input && isspace((unsigned char)*input)) {
                input++;
            }
            
            if (*input) {
                // Берем все оставшееся как второй аргумент
                size_t arg_len = strlen(input);
                if (arg_len >= MAX_MESSAGE_LENGTH) {
                    arg_len = MAX_MESSAGE_LENGTH - 1;
                }
                
                strncpy(arg2, input, arg_len);
                arg2[arg_len] = '\0';
            }
        }
        
        return 1; // Успешно распарсена команда
    } else {
        // Это сообщение в формате "получатель текст"
        const char *space = strchr(input, ' ');
        if (!space) {
            // Нет пробела - вероятно, только получатель
            safe_strcpy(command, input, MAX_LOGIN_LENGTH);
            return 2; // Только получатель
        }
        
        // Извлекаем получателя
        size_t receiver_len = space - input;
        if (receiver_len >= MAX_LOGIN_LENGTH) {
            receiver_len = MAX_LOGIN_LENGTH - 1;
        }
        
        strncpy(command, input, receiver_len);
        command[receiver_len] = '\0';
        
        // Остальное - текст сообщения
        input = space + 1;
        while (*input && isspace((unsigned char)*input)) {
            input++;
        }
        
        if (arg1) {
            safe_strcpy(arg1, input, MAX_MESSAGE_LENGTH);
        }
        
        return 3; // Получатель и сообщение
    }
}

const char* status_to_string(UserStatus status) {
    switch (status) {
        case USER_STATUS_OFFLINE:  return "offline";
        case USER_STATUS_ONLINE:   return "online";
        case USER_STATUS_AWAY:     return "away";
        case USER_STATUS_BUSY:     return "busy";
        case USER_STATUS_INVISIBLE: return "invisible";
        default:                   return "unknown";
    }
}

UserStatus string_to_status(const char *str) {
    if (!str) return USER_STATUS_OFFLINE;
    
    char lower_str[32];
    int i;
    for (i = 0; str[i] && i < 31; i++) {
        lower_str[i] = tolower((unsigned char)str[i]);
    }
    lower_str[i] = '\0';
    
    if (strcmp(lower_str, "online") == 0) return USER_STATUS_ONLINE;
    if (strcmp(lower_str, "away") == 0) return USER_STATUS_AWAY;
    if (strcmp(lower_str, "busy") == 0) return USER_STATUS_BUSY;
    if (strcmp(lower_str, "invisible") == 0) return USER_STATUS_INVISIBLE;
    
    return USER_STATUS_OFFLINE;
}
// ============================================
// Функции работы со строками
// ============================================

void safe_strcpy(char *dest, const char *src, size_t dest_size) {
    if (!dest || dest_size == 0) {
        return;
    }
    
    if (!src) {
        dest[0] = '\0';
        return;
    }
    
    size_t i;
    for (i = 0; i < dest_size - 1 && src[i] != '\0'; i++) {
        dest[i] = src[i];
    }
    
    dest[i] = '\0';
}

void safe_strcat(char *dest, const char *src, size_t dest_size) {
    if (!dest || !src || dest_size == 0) {
        return;
    }
    
    size_t dest_len = strlen(dest);
    if (dest_len >= dest_size) {
        return; // Нет места
    }
    
    safe_strcpy(dest + dest_len, src, dest_size - dest_len);
}

char *my_strdup(const char *s) {
    if (!s) return NULL;
    
    size_t len = strlen(s);
    char *dup = (char*)malloc(len + 1);  // +1 для нулевого байта
    if (!dup) {
        return NULL;
    }
    
    memcpy(dup, s, len + 1);  // Копируем с нулевым байтом
    return dup;
}

int my_strcasecmp(const char *s1, const char *s2) {
    while (*s1 && *s2) {
        int c1 = tolower((unsigned char)*s1);
        int c2 = tolower((unsigned char)*s2);
        if (c1 != c2) return c1 - c2;
        s1++;
        s2++;
    }
    return tolower((unsigned char)*s1) - tolower((unsigned char)*s2);
}

void trim_newline(char *str) {
    if (!str) return;
    
    size_t len = strlen(str);
    while (len > 0 && (str[len-1] == '\n' || str[len-1] == '\r')) {
        str[--len] = '\0';
    }
}

// ============================================
// Функции логирования
// ============================================

void init_logger(const char *filename) {
    if (log_initialized) {
        return;
    }
    
    if (filename) {
        log_file = fopen(filename, "a");
        if (!log_file) {
            fprintf(stderr, "Failed to open log file: %s\n", strerror(errno));
            log_file = stderr;
        }
    } else {
        log_file = stderr;
    }
    
    log_initialized = 1;
}

void log_message(const char *level, const char *format, ...) {
    if (!log_initialized) {
        init_logger(NULL);
    }
    
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    char time_str[20];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", tm_info);
    
    fprintf(log_file, "[%s] [%s] ", time_str, level);
    
    va_list args;
    va_start(args, format);
    vfprintf(log_file, format, args);
    va_end(args);
    
    fprintf(log_file, "\n");
    fflush(log_file);
}