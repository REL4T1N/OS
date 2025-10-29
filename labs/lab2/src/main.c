#include "common.h"
#include "merge_sort.h"
#include "benchmark.h"

// Определение глобальных переменных
int PARALLEL_THRESHOLD = 1000;
int SEQUENTIAL_THRESHOLD = 50;
int MAX_THREADS = 8;
int ARRAY_SIZE = 50000000;
int ACTIVE_THREADS = 0;
int* TEST_ORIGINAL_ARRAY = NULL;
int TEST_ARRAY_SIZE = 0;
pthread_mutex_t THREAD_MUTEX = PTHREAD_MUTEX_INITIALIZER;

double get_time() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1000000000.0;
}

void print_usage(const char* program_name) {
    printf("Использование: %s [опции]\n", program_name);
    printf("Опции:\n");
    printf("  -s <размер>      Размер массива\n");
    printf("  -t <потоки>      Максимальное количество потоков\n");
    printf("  -p <порог>       Порог для параллельной сортировки\n");
    printf("  -seq <порог>     Порог для последовательной сортировки\n");
    printf("Тестовые наборы:\n");
    printf("  -size            Тест влияния размера массива\n");
    printf("  -threads         Тест влияния количества потоков\n");  
    printf("  -threshold       Тест влияния пороговых значений\n");
    printf("  -all             Запуск всех тестов\n");
    printf("  -h               Показать эту справку\n");
    printf("\nПримеры:\n");
    printf("  %s -size\n", program_name);
    printf("  %s -threads\n", program_name);  
    printf("  %s -threshold\n", program_name);
    printf("  %s -all\n", program_name);
    printf("  %s -s 50000000 -t 8 -p 2000\n", program_name); 
}

void parse_arguments(int argc, char* argv[]) {
    int run_size_tests = 0;
    int run_threads_tests = 0;  
    int run_threshold_tests = 0;
    
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-s") == 0 && i + 1 < argc) {
            ARRAY_SIZE = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-t") == 0 && i + 1 < argc) {  
            MAX_THREADS = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-p") == 0 && i + 1 < argc) {
            PARALLEL_THRESHOLD = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-seq") == 0 && i + 1 < argc) {
            SEQUENTIAL_THRESHOLD = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-h") == 0) {
            print_usage(argv[0]);
            exit(0);
        } else if (strcmp(argv[i], "-size") == 0) {
            run_size_tests = 1;
        } else if (strcmp(argv[i], "-threads") == 0) {  
            run_threads_tests = 1;
        } else if (strcmp(argv[i], "-threshold") == 0) {
            run_threshold_tests = 1;
        } else if (strcmp(argv[i], "-all") == 0) {
            run_size_tests = run_threads_tests = run_threshold_tests = 1;  
        } 
    }

    if (run_size_tests) {
        run_size_test_suite();
        if (!run_threads_tests && !run_threshold_tests) exit(0);  
    }
    if (run_threads_tests) {  
        run_threads_test_suite();  
        if (!run_threshold_tests) exit(0);
    }
    if (run_threshold_tests) {
        run_threshold_test_suite();
        exit(0);
    }
}

void init_test_data(int size) {
    if (TEST_ORIGINAL_ARRAY != NULL && TEST_ARRAY_SIZE == size) {
        return;
    }
    
    if (TEST_ORIGINAL_ARRAY != NULL) {
        free(TEST_ORIGINAL_ARRAY);
    }
    
    TEST_ORIGINAL_ARRAY = (int*)malloc(size * sizeof(int));
    TEST_ARRAY_SIZE = size;
    getRandomArray(TEST_ORIGINAL_ARRAY, size, 1000000);
}

void cleanup_test_data() {
    if (TEST_ORIGINAL_ARRAY != NULL) {
        free(TEST_ORIGINAL_ARRAY);
        TEST_ORIGINAL_ARRAY = NULL;
        TEST_ARRAY_SIZE = 0;
    }
}

int main(int argc, char* argv[]) {
    pthread_mutex_init(&THREAD_MUTEX, NULL);
    
    parse_arguments(argc, argv);
    
    printf("=== ПАРАЛЛЕЛЬНАЯ СОРТИРОВКА СЛИЯНИЕМ ===\n");
    printf("Параметры:\n");
    printf("  Размер массива: %d\n", ARRAY_SIZE);
    printf("  Макс. потоки: %d\n", MAX_THREADS); 
    printf("  Порог параллелизма: %d\n", PARALLEL_THRESHOLD);
    printf("  Порог последовательной: %d\n\n", SEQUENTIAL_THRESHOLD);
    
    if (argc > 1) {
        metrics_t metrics = run_comparison();
        
        printf("Результаты:\n");
        printf("  Последовательная: %.3f секунд\n", metrics.sequentialTime);
        printf("  Параллельная: %.3f секунд\n", metrics.parallelTime);
        printf("  Ускорение: %.2f раз\n", metrics.sequentialTime / metrics.parallelTime);
        printf("  Корректность: %s\n", metrics.isCorrect ? "ДА" : "НЕТ");
    } else {
        printf("Используйте -h для справки или тестовые флаги для запуска тестов\n");
    }
    
    cleanup_test_data();
    pthread_mutex_destroy(&THREAD_MUTEX);
    return 0;
}