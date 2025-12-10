#ifndef UTILS_H
#define UTILS_H

#include "./protocol.h"
#include <stdio.h>

// ============================================
// Функции сериализации/десериализации
// ============================================

// Сериализация Message в буфер
// Возвращает количество записанных байт
size_t serialize_message(const Message *msg, char *buffer, size_t buffer_size);

// Десериализация Message из буфера
// Возвращает 0 при успехе, -1 при ошибке
int deserialize_message(const char *buffer, size_t buffer_size, Message *msg);

// Сериализация UserList в буфер
size_t serialize_userlist(const UserList *list, char *buffer, size_t buffer_size);

// Десериализация UserList из буфера
int deserialize_userlist(const char *buffer, size_t buffer_size, UserList *list);

// ============================================
// Функции форматирования/парсинга
// ============================================

// Форматирование сообщения для отображения пользователю
// Формат: "[12:34] <alice>: Привет!"
void format_message_for_display(const Message *msg, char *output, size_t output_size);

// Парсинг ввода пользователя (команды)
// Формат команд: "/command arg1 arg2" или "username message"
int parse_user_input(const char *input, char *command, char *arg1, char *arg2);

// Преобразование статуса в строку
const char* status_to_string(UserStatus status);

// Преобразование строки в статус
UserStatus string_to_status(const char *str);

// ============================================
// Функции работы со строками
// ============================================

// Безопасное копирование строк с ограничением длины
void safe_strcpy(char *dest, const char *src, size_t dest_size);

// Безопасная конкатенация строк
void safe_strcat(char *dest, const char *src, size_t dest_size);

// Функция дублирования строк (аналог strdup) -- у меня он почему-то из posix не запустился
char *my_strdup(const char *s);

int my_strcasecmp(const char *s1, const char *s2);

// Удаление символов новой строки
void trim_newline(char *str);

// ============================================
// Функции логирования
// ============================================

// Инициализация логгера
void init_logger(const char *filename);

// Логирование сообщения
void log_message(const char *level, const char *format, ...);

// Макросы для удобного логирования
#define LOG_ERROR(format, ...) log_message("ERROR", format, ##__VA_ARGS__)
#define LOG_INFO(format, ...)  log_message("INFO",  format, ##__VA_ARGS__)
#define LOG_DEBUG(format, ...) log_message("DEBUG", format, ##__VA_ARGS__)

#endif