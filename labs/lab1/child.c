#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>

#define MAX_LINE_LENGTH 1024

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <filename>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    
    char *filename = argv[1];
    FILE *file;
    char line[MAX_LINE_LENGTH];
    
    // Открываем файл для чтения
    file = fopen(filename, "r");
    if (file == NULL) {
        perror("fopen failed");
        exit(EXIT_FAILURE);
    }
    
    // Читаем файл построчно
    while (fgets(line, sizeof(line), file) != NULL) {
        int sum = 0;
        int number;
        char *token;
        
        // Разбиваем строку на числа
        token = strtok(line, " \t\n");
        while (token != NULL) {
            // Пропускаем пустые токены
            if (strlen(token) > 0) {
                number = atoi(token);
                sum += number;
            }
            token = strtok(NULL, " \t\n");
        }
        
        // Выводим результат в stdout (который перенаправлен в pipe)
        printf("%d\n", sum);
        fflush(stdout);  // Важно для немедленной отправки через pipe
    }
    
    fclose(file);
    return 0;
}