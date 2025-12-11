#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../include/lib_contract.h"

int main() {
    printf("=== Program 1: Static Linking Demo ===\n");
    printf("This program is linked with lib1.so at compile time\n");
    printf("Commands:\n");
    printf("  1 A B e  - Compute integral of sin(x) from A to B with step e\n");
    printf("  2 A B    - Compute GCD of A and B\n");
    printf("  3        - Exit program\n");
    printf("  0        - Switching NOT supported (ignored)\n");
    printf("======================================\n");
    
    char command[256];
    
    while (1) {
        printf("\n> ");
        fflush(stdout);
        
        if (!fgets(command, sizeof(command), stdin)) {
            break;
        }
        
        // удалить \n
        command[strcspn(command, "\n")] = '\0';
        
        if (strcmp(command, "3") == 0) {            // выход
            printf("Exiting...\n");
            break;
        } else if (strcmp(command, "0") == 0) {     // тут нельзя переключаться, для аналогичности интерфейса сделана
            printf("Warning: Library switching is not supported in Program 1\n");
            printf("This program is hard-linked to lib1.so\n");
        } else if (command[0] == '1') {             // интеграл
            float A, B, e;
            /*
            scanf() - чтение из стандартного ввода (stdin):
            sscanf() - чтение из строки (буфера):
            */
            if (sscanf(command + 1, "%f %f %f", &A, &B, &e) == 3) {
                float result = sinIntegral(A, B, e);
                printf("Result (Rectangles method): %.6f\n", result);
            } else {
                printf("Error: Invalid arguments for command '1'\n");
                printf("Usage: 1 A B e\n");
            }
        } else if (command[0] == '2') {             // НОД
            int A, B;
            if (sscanf(command + 1, "%d %d", &A, &B) == 2) {
                int result = GCF(A, B);
                printf("Result (Euclidean algorithm): %d\n", result);
            } else {
                printf("Error: Invalid arguments for command '2'\n");
                printf("Usage: 2 A B\n");
            }
        } else {
            printf("Unknown command. Available: 0, 1 A B e, 2 A B, 3\n");
        }
    }
    
    return 0;
}