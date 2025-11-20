#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include "./shared_memory.h"


int main(int argc, char *argv[]) {
    if (argc != 3) {
        // fprintf() вместо perror() из-за отсутствия форматирования во втором
        fprintf(stderr, "Недостаточное количество данных для дочернего процесса: %d", argc);
        exit(EXIT_FAILURE);
    }

    const char *input_shm_name = argv[1];
    const char *output_shm_name = argv[2];

    shared_memory *input_shm = open_shared_memory(input_shm_name);
    shared_memory *output_shm = open_shared_memory(output_shm_name);

    // ожидание готовности от родительского процесса
    while (!input_shm->data_ready) {
        usleep(100 * 1000); // 100ms
    }

    // копия входных данных для безопасной обработки
    char input_copy[SHARED_SIZE];
    strncpy(input_copy, input_shm->data, SHARED_SIZE - 1);
    input_copy[SHARED_SIZE - 1] = '\0';

    char *output_ptr = output_shm->data;
    // Обрабатываем каждую строку отдельно
    char *line_start = input_copy;
    char *line_end;

    while (line_start && *line_start) {
        // поиск конца строки
        line_end = strchr(line_start, '\n');
        if (line_end) {
            *line_end = '\0'; // образ строки
        }

        int sum = 0;
        char *token = strtok(line_start, " \t");
        while (token) {
            if (strlen(token) > 0) {
                sum += atoi(token);
            }
            token = strtok(NULL, " \t");
        }

        output_ptr += sprintf(output_ptr, "%d\n", sum);

        // next line
        if (line_end) {
            line_start = line_end + 1;
        } else {
            break;
        }
    }

    output_shm->process_complete = 1;       // флаг готовности для родительского процесса
    close_shared_memory(input_shm, input_shm_name, 0);
    close_shared_memory(output_shm, output_shm_name, 0);

    return 0;
}
