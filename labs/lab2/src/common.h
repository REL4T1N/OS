#ifndef COMMON_H
#define COMMON_H

#define _POSIX_C_SOURCE 199309L

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

// Глобальные конфигурационные переменные
extern int PARALLEL_THRESHOLD;
extern int SEQUENTIAL_THRESHOLD;
extern int MAX_THREADS;
extern int ARRAY_SIZE;
extern int ACTIVE_THREADS;
extern int* TEST_ORIGINAL_ARRAY;
extern int TEST_ARRAY_SIZE;
extern pthread_mutex_t THREAD_MUTEX;

// Структуры
typedef struct {
    int* arr;
    int left;
    int right;
} thread_data_t;

typedef struct {
    double sequentialTime;
    double parallelTime;
    int threadsUsed;
    int isCorrect;
} metrics_t;

// Утилиты
double get_time();
void print_usage(const char* program_name);
void parse_arguments(int argc, char* argv[]);
void init_test_data(int size);
void cleanup_test_data();

#endif