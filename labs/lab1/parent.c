#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>

#define BUFFER_SIZE 1024

int main() {
    int pipe1[2];  
    pid_t pid;
    char filename[256];
    char buffer[BUFFER_SIZE];
    int bytes_read;
    
    // создать pipe1
    if (pipe(pipe1) == -1) {
        perror("pipe1 failed");
        exit(EXIT_FAILURE);
    }
    
    printf("Введите имя файла: ");
    if (scanf("s", filename) != 1) {
        perror("scanf failed");
        exit(-1);
    }
    
    // копия процесса
    pid = fork();
    if (pid == -1) {
        perror("fork failed");
        exit(-1);
    }
    
    if (pid == 0) {
        close(pipe1[0]);  
        
        // stdout -> pipe1[1]
        dup2(pipe1[1], 1); // или dup2(pipe1[1], STDOUT_FILENO);
        /*STDOUT_FILENO - это стандартный файловый дескриптор для стандартного 
        потока вывода.*/
        close(pipe1[1]); 
        
        execl("./child", "child", filename, NULL);
        
        // execl = ошибка
        perror("execl failed");
        exit(-1);
    } else {
        close(pipe1[1]); 
        
        while ((bytes_read = read(pipe1[0], buffer, BUFFER_SIZE - 1)) > 0) {
            buffer[bytes_read] = '\0';
            printf("%s", buffer);
            fflush(stdout);
        }
        
        close(pipe1[0]);
        
        // ожидание завершения
        wait(NULL);
        printf("Дочерний процесс завершен.\n");
    }
    
    return 0;
}