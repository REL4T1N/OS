#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>

// типы функций
typedef float (*sin_integral_func)(float, float, float);
typedef int (*gcf_func)(int, int);

int main() {
    void* lib_handle = NULL;
    sin_integral_func sinIntegral = NULL;
    gcf_func GCF = NULL;
    
    // по дефолту сначала грузится первая библиотека
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
    
    // загрузка библиотеки
    void load_library(int lib_num) {
        // если уже загружена библиотека - выгружаем
        if (lib_handle != NULL) {
            /*
            dlclose() - уменьшает количество ссылок на библиотеку. Если счётчик равен 0, то библиотека выгружается из памяти
            Принимает:
                - handle - дескриптор обрабатываемой библиотеки
            Возвращает:
                - 0 при успехе
                - число != 0, означающее ошибку
            */
            if (dlclose(lib_handle) != 0) {
                const char* error_msg = dlerror();
                if (error_msg) {
                    fprintf(stderr, "Warning: Failed to unload library: %s\n", error_msg);
                }
            }
            lib_handle = NULL;
            sinIntegral = NULL;
            GCF = NULL;
        }
        
        /*
        dlopen() - загружает динамическую библиотеку (.so файл) в память процесса и возвращает дескриптор для работы с ней
        Принимает:
            - filename - путь к библиотеке
            - Флаги загрузки:
                - RTLD_LAZY     ленивое связывание 
                - RTLD_NOW      немедленное связывание
                - RTLD_GLOBAL   Символы доступны другим библиотекам   
                - RTLD_LOCAL    Символы доступны только для этой загрузки (установлен по умолчанию)
        Возвращает:
            - Файловый дескриптор в случае удачи
            - NULL в случае ошибки
        */
        lib_handle = dlopen(lib_names[lib_num - 1], RTLD_LAZY);
        if (!lib_handle) {
            /*
            dlerror() - Возвращает строку с описанием последеней ошибки при вызове функций dl*
            Возвращает:
                - char* - строка с описанием ошибки
                - NULL если ошибок ещё не было или с последнего вызова dlerror новых не возникло
            */
            fprintf(stderr, "Error loading library %d: %s\n", lib_num, dlerror());
            exit(1);
        }
        
        // получить указатели на функции
        dlerror(); // сброс ошибок перед dlsym
        sinIntegral = (sin_integral_func)dlsym(lib_handle, "sinIntegral");
        /*
        dlsym() - находит функцию или переменную в загруженной библиотеке и возвращает указатель на неё
        Принимает:
            - handle - дескриптор библиотеки, в которой необходимо искать
            - symbol - Имя функции или переменной в виде строки, которое нужно искать
        Возвращает:
            - void* - указатель на фукнцию или переменную при успехе
            - NULL - в случае ошибки или ненахода
        */
        GCF = (gcf_func)dlsym(lib_handle, "GCF");
        
        // проверка, все ли функции определились
        const char* dlsym_error = dlerror();
        if (dlsym_error) {
            fprintf(stderr, "Error loading symbols: %s\n", dlsym_error);
            dlclose(lib_handle);
            exit(1);
        }
        
        current_lib = lib_num;
        printf("Switched to lib%d.so\n", lib_num);
    }
    
    // загрузка первой библиотеки
    load_library(1);
    
    char command[256];
    
    while (1) {
        printf("\n[lib%d]> ", current_lib);
        fflush(stdout);
        
        if (!fgets(command, sizeof(command), stdin)) {
            break;
        }
        
        // удалить \n
        command[strcspn(command, "\n")] = '\0';
        
        if (strcmp(command, "3") == 0) {                // выход из программы
            printf("Exiting...\n");
            break;
        } else if (strcmp(command, "0") == 0) {         // переключение библиотеки
            int new_lib = (current_lib == 1) ? 2 : 1;
            load_library(new_lib);
        } else if (command[0] == '1') {                 // интегральная функция
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
        } else if (command[0] == '2') {                 // НОД
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
    
    // выгрузить библиотеку перед выходом
    if (lib_handle != NULL) {
        if (dlclose(lib_handle) != 0) {
            const char* error_msg = dlerror();
            if (error_msg) {
                fprintf(stderr, "Warning: Failed to unload library when exiting the program: %s\n", error_msg);
            }
        }
    }
    
    return 0;
}