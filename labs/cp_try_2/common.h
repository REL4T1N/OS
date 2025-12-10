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

#define INPUT_PORT "7777"     // Для входящих сообщений (PULL)
#define OUTPUT_PORT "7778"    // Для исходящих сообщений (PUB/SUB)
#define SERVER_IP "tcp://127.0.0.1:"

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
    time_t send_time; // Для отложенных сообщений
} message_t;

// Функция для печати текущего времени
void print_time() {
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    printf("[%02d:%02d:%02d] ", tm->tm_hour, tm->tm_min, tm->tm_sec);
}

#endif