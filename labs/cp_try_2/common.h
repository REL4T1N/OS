#ifndef COMMON_H
#define COMMON_H

#include <zmq.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>
#include <stdarg.h>

// Порт для входящих сообщений (PUSH/PULL)
#define INPUT_PORT 7777
// Порт для рассылки сообщений (PUB/SUB)
#define OUTPUT_PORT 7779
#define SERVER_IP "tcp://127.0.0.1"

// Типы сообщений
#define MSG_TYPE_TEXT 1
#define MSG_TYPE_DELAYED 2
#define MSG_TYPE_JOIN 3
#define MSG_TYPE_LEAVE 4

// Структура сообщения
typedef struct {
    char sender[32];
    char recipient[32];
    char text[256];
    int type;
    time_t created_time;    // Когда сообщение создано на клиенте
    time_t send_time;       // Когда должно быть отправлено (для отложенных)
} message_t;

// Функция для печати текущего времени
void print_time() {
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    printf("[%02d:%02d:%02d] ", tm->tm_hour, tm->tm_min, tm->tm_sec);
}

// Функция для форматированного вывода времени
void print_time_struct(time_t t) {
    struct tm *tm = localtime(&t);
    printf("%02d:%02d:%02d", tm->tm_hour, tm->tm_min, tm->tm_sec);
}

// Используйте единый формат для всех
void print_unified_time(time_t t, const char* prefix) {
    struct tm *tm = localtime(&t);
    printf("%s[%02d:%02d:%02d] ", prefix ? prefix : "", 
           tm->tm_hour, tm->tm_min, tm->tm_sec);
}

// И в коде замените все print_time_struct на:
void print_time_with_label(time_t t, const char* label) {
    struct tm *tm = localtime(&t);
    printf("%s: %02d:%02d:%02d", label, tm->tm_hour, tm->tm_min, tm->tm_sec);
}

void print_message_info(message_t* msg, const char* action) {
    time_t now = time(NULL);
    printf("[");
    print_time_struct(now);
    printf("] %s: %s -> %s\n", action, msg->sender, msg->recipient);
    printf("    Создано: ");
    print_time_struct(msg->created_time);
    if (msg->type == MSG_TYPE_DELAYED) {
        printf(", отправка: ");
        print_time_struct(msg->send_time);
    }
    printf("\n    Текст: %s\n", msg->text);
}

#endif