#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <fcntl.h> 

#define FILENAME_SIZE 256 

int main() {
    int pipe1[2];  
    pid_t pid;
    char filename[FILENAME_SIZE];
    int file_fd;
    
    // создать pipe1
    if (pipe(pipe1) == -1) {
        perror("pipe1 failed");
        exit(EXIT_FAILURE);
    }
    
    printf("Введите имя файла: ");
    if (scanf("%255s", filename) != 1) {
        perror("scanf failed");
        exit(EXIT_FAILURE);
    }
    
    file_fd = open(filename, O_RDONLY);
    if (file_fd == -1) {
        perror("open failed");
        exit(EXIT_FAILURE);
    }
    printf("Файл '%s' открыт для чтения\n", filename);

    // копия процесса
    pid = fork();
    if (pid == -1) {
        perror("fork failed");
        close(file_fd);
        exit(EXIT_FAILURE);
    }
    
    if (pid == 0) {
        close(pipe1[0]);  

        // stdin -> file_fd
        if (dup2(file_fd, STDIN_FILENO) == -1) {
            perror("dup2 stdin failed");
            close(file_fd);
            exit(EXIT_FAILURE);
        }
        close(file_fd);
        
        // stdout -> pipe1[1]
        if (dup2(pipe1[1], STDOUT_FILENO) == -1) {
            perror("dup2 failed");
            close(pipe1[1]);
            exit(EXIT_FAILURE);
        }
        close(pipe1[1]); 
        
        execl("./child", "child", NULL);
        
        // execl = ошибка
        perror("execl failed");
        exit(-1);
    } else {
        close(pipe1[1]); 
        close(file_fd);

        char *buffer = NULL;
        size_t buffer_size = 0;
        ssize_t len;
        
        FILE *pipe_read = fdopen(pipe1[0], "r");
        if (pipe_read == NULL) {
            perror("fdopen failed");
            close(pipe1[0]);
            exit(EXIT_FAILURE);
        }
        
        // Читаем построчно из pipe
        while ((len = getline(&buffer, &buffer_size, pipe_read)) != -1) {
            write(STDOUT_FILENO, buffer, len);
        }
        
        free(buffer);
        fclose(pipe_read);

        wait(NULL);
        printf("Дочерний процесс завершен.\n");
    }
    
    return 0;
}