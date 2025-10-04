#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h> 

#define MAX_LINE_LENGTH 1024
#define OUTPUT_BUFFER_SIZE 32
#define FILENAME_SIZE 256    

int main(int argc, char *argv[]) {
    char line[MAX_LINE_LENGTH];
    char output_buffer[OUTPUT_BUFFER_SIZE];

    
    while (fgets(line, sizeof(line), stdin) != NULL) {
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
        

        int len = snprintf(output_buffer, sizeof(output_buffer), "%d\n", sum);
        if (len > 0) {
            write(STDOUT_FILENO, output_buffer, len);
        }
    }
    
    return 0;
}