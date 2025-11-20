#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include "./shared_memory.h"


#define SHM_NAME_1 "/sum_calc_shm_1"
#define SHM_NAME_2 "/sum_calc_shm_2"
#define FILENAME_SIZE 256

int main() {
    char filename[FILENAME_SIZE];
    printf("Введите имя файла: ");
    if (scanf("%255s", filename) != 1) {
        perror("scanf failed");
        exit(EXIT_FAILURE);
    }

    shared_memory *input_shm = create_shared_memory(SHM_NAME_1);
    shared_memory *output_data = create_shared_memory(SHM_NAME_2);

    // загрузка данных из файла в shared memory
    FILE *file =  fopen(filename, "r");
    if (!file) {
        perror("fopen");
        close_shared_memory(input_shm, SHM_NAME_1, 1);
        close_shared_memory(output_data, SHM_NAME_2, 1);
        exit(EXIT_FAILURE);
    }

    size_t bytes_read = fread(input_shm->data, 1, SHARED_SIZE - 1, file);
    input_shm->data[bytes_read] = '\0';
    fclose(file);

    pid_t child_pid = fork_process();

    if (child_pid == 0) {
        // после execl закрыть отображения будет невозможно, поэтому закрываем + в ребёнке мы заново их октрываем
        close_shared_memory(input_shm, SHM_NAME_1, 0);
        close_shared_memory(output_data, SHM_NAME_2, 0);

        execl("./child", "child", SHM_NAME_1, SHM_NAME_2, NULL);
        perror("execl failed");
        exit(EXIT_FAILURE);

    } else {
        input_shm->data_ready = 1;      // флаг готовности для дочернего процесса

        // ожидание готовности от дочернего процесса
        while (!output_data->process_complete) {
            usleep(100 * 1000); // 100ms
        }
        
        printf("Результат обработки:\n");
        printf("%s", output_data->data);
        /*
        waitpid() - заставляет родительский процесс ждать завершения дочернего процесса с указанным PID
        Принимает:
            - child_pid - pid дочернего процесса
            - NULL - указатель на переменную для сохранения статуса завершения (в данном случае не нужен)
            - 0 - опции: <0> означает, что waitpid() будет ждать до завершения дочернего процесса
        Возвращает:
            - число > 0 - PID завершившегося дочернего процесса
            - 0 - дочерний процесс ещё не завершился
            - -1 - ошибка
        */
        if (waitpid(child_pid, NULL, 0) == -1) {
            perror("waitpid failed");
        }

        close_shared_memory(input_shm, SHM_NAME_1, 1);
        close_shared_memory(output_data, SHM_NAME_2, 1);

        printf("Обработка завершена\n");
    }

    return 0;
}   