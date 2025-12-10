#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>

// Определяем типы функций для удобства
typedef float (*sin_integral_func)(float, float, float);
typedef int (*gcf_func)(int, int);

int main() {
    void* lib_handle = NULL;
    sin_integral_func sinIntegral = NULL;
    gcf_func GCF = NULL;
    
    // Изначально загружаем первую библиотеку
    int current_lib = 1;
    const char* lib_names[] = {"./build/lib/lib1.so", 
                               "./build/lib/lib2.so"};
    
    printf("=== Program 2: Dynamic Loading Demo ===\n");
    printf("Commands:\n");
    printf("  0        - Switch between lib1 and lib2\n");
    printf("  1 A B e  - Compute integral of sin(x) from A to B with step e\n");
    printf("  2 A B    - Compute GCD of A and B\n");
    printf("  3        - Exit program\n");
    printf("========================================\n");
    
    // Функция для загрузки библиотеки
    void load_library(int lib_num) {
        // Выгружаем предыдущую библиотеку, если она была загружена
        if (lib_handle != NULL) {
            dlclose(lib_handle);
            lib_handle = NULL;
            sinIntegral = NULL;
            GCF = NULL;
        }
        
        // Загружаем новую библиотеку
        lib_handle = dlopen(lib_names[lib_num - 1], RTLD_LAZY);
        if (!lib_handle) {
            fprintf(stderr, "Error loading library %d: %s\n", lib_num, dlerror());
            exit(1);
        }
        
        // Получаем указатели на функции
        sinIntegral = (sin_integral_func)dlsym(lib_handle, "sinIntegral");
        GCF = (gcf_func)dlsym(lib_handle, "GCF");
        
        // Проверяем, что функции найдены
        const char* dlsym_error = dlerror();
        if (dlsym_error) {
            fprintf(stderr, "Error loading symbols: %s\n", dlsym_error);
            dlclose(lib_handle);
            exit(1);
        }
        
        current_lib = lib_num;
        printf("Switched to lib%d.so\n", lib_num);
    }
    
    // Изначальная загрузка первой библиотеки
    load_library(1);
    
    char command[256];
    
    while (1) {
        printf("\n[lib%d]> ", current_lib);
        fflush(stdout);
        
        if (!fgets(command, sizeof(command), stdin)) {
            break;
        }
        
        // Удаление символа новой строки
        command[strcspn(command, "\n")] = '\0';
        
        if (strcmp(command, "3") == 0) {
            printf("Exiting...\n");
            break;
        } else if (strcmp(command, "0") == 0) {
            // Переключение библиотек
            int new_lib = (current_lib == 1) ? 2 : 1;
            load_library(new_lib);
        } else if (command[0] == '1') {
            float A, B, e;
            if (sscanf(command + 1, "%f %f %f", &A, &B, &e) == 3) {
                float result = sinIntegral(A, B, e);
                printf("Result (%s method): %.6f\n", 
                       (current_lib == 1) ? "Rectangles" : "Trapezoids", 
                       result);
            } else {
                printf("Error: Invalid arguments for command '1'\n");
                printf("Usage: 1 A B e\n");
            }
        } else if (command[0] == '2') {
            int A, B;
            if (sscanf(command + 1, "%d %d", &A, &B) == 2) {
                int result = GCF(A, B);
                printf("Result (%s algorithm): %d\n",
                       (current_lib == 1) ? "Euclidean" : "Naive",
                       result);
            } else {
                printf("Error: Invalid arguments for command '2'\n");
                printf("Usage: 2 A B\n");
            }
        } else {
            printf("Unknown command. Available: 0, 1 A B e, 2 A B, 3\n");
        }
    }
    
    // Выгружаем библиотеку перед выходом
    if (lib_handle != NULL) {
        dlclose(lib_handle);
    }
    
    return 0;
}