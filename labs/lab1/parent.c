#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <fcntl.h>

#define BUFFER_SIZE 1024

int main() {
    int pipe1[2];  // pipe1: child stdout -> parent stdin
    pid_t pid;
    char filename[256];
    char buffer[BUFFER_SIZE];
    int bytes_read;
    
    // Создаем pipe1
    if (pipe(pipe1) == -1) {
        perror("pipe1 failed");
        exit(EXIT_FAILURE);
    }
    
    // Получаем имя файла от пользователя
    printf("Введите имя файла: ");
    if (fgets(filename, sizeof(filename), stdin) == NULL) {
        perror("fgets failed");
        exit(EXIT_FAILURE);
    }
    
    // Убираем символ новой строки
    filename[strcspn(filename, "\n")] = 0;
    
    // Создаем дочерний процесс
    pid = fork();
    if (pid == -1) {
        perror("fork failed");
        exit(EXIT_FAILURE);
    }
    
    if (pid == 0) {
        // Дочерний процесс
        close(pipe1[0]);  // Закрываем чтение из pipe1
        
        // Перенаправляем stdout в pipe1[1]
        dup2(pipe1[1], STDOUT_FILENO);
        close(pipe1[1]);
        
        // Запускаем программу дочернего процесса
        execl("./child", "child", filename, NULL);
        
        // Если execl вернулся, значит ошибка
        perror("execl failed");
        exit(EXIT_FAILURE);
    } else {
        // Родительский процесс
        close(pipe1[1]);  // Закрываем запись в pipe1
        
        // Читаем данные из pipe1 и выводим на stdout
        while ((bytes_read = read(pipe1[0], buffer, BUFFER_SIZE - 1)) > 0) {
            buffer[bytes_read] = '\0';
            printf("%s", buffer);
            fflush(stdout);
        }
        
        close(pipe1[0]);
        
        // Ждем завершения дочернего процесса
        wait(NULL);
        printf("Дочерний процесс завершен.\n");
    }
    
    return 0;
}