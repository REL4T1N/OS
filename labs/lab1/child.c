#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAX_LINE_LENGTH 1024

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <filename>\n", argv[0]);
        exit(-1);
    }
    
    char *filename = argv[1];
    FILE *file;
    char line[MAX_LINE_LENGTH];
    
    file = fopen(filename, "r");
    if (file == NULL) {
        perror("fopen failed");
        exit(-1);
    }
    
    while (fgets(line, sizeof(line), file) != NULL) {
        int sum = 0;
        int number;
        char *token;
        
        // разбить строку на числа (аналог split())
        token = strtok(line, " \t\n");
        while (token != NULL) {
            if (strlen(token) > 0) {
                number = atoi(token);
                sum += number;
            }
            token = strtok(NULL, " \t\n");
        }
        
        // результат в stdout
        printf("%d\n", sum);
        fflush(stdout);
    }
    
    fclose(file);
    return 0;
}