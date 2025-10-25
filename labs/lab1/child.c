#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h> 

#define OUTPUT_BUFFER_SIZE 32

int main(int argc, char *argv[]) {
    char *line = NULL;
    size_t line_size = 0;
    ssize_t len;
    char output_buffer[OUTPUT_BUFFER_SIZE];

    
    while ((len = getline(&line, &line_size, stdin)) != -1) {
        int sum = 0;
        int number;
        char *token;
        
        token = strtok(line, " \t\n");
        while (token != NULL) {
            if (strlen(token) > 0) {
                number = atoi(token);
                sum += number;
            }
            token = strtok(NULL, " \t\n");
        }
        

        int output_len = snprintf(output_buffer, sizeof(output_buffer), "%d\n", sum);
        if (output_len > 0) {
            write(STDOUT_FILENO, output_buffer, output_len);
        }
    }
    free(line);
    return 0;
}