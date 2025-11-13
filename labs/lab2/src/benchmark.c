#include "benchmark.h"
#include "merge_sort.h"
#include "common.h"

metrics_t run_comparison() {
    metrics_t metrics = {0};
    
    // printf("DEBUG: Initializing test data for size %d\n", ARRAY_SIZE);
    if (init_test_data(ARRAY_SIZE) != 0) {
        // printf("ERROR: Failed to initialize test data\n");
        metrics.sequentialTime = -1;
        metrics.parallelTime = -1;
        return metrics;
    }
    
    // printf("DEBUG: Allocating sequential and parallel arrays\n");
    int* sequential_data = (int*)malloc(ARRAY_SIZE * sizeof(int));
    int* parallel_data = (int*)malloc(ARRAY_SIZE * sizeof(int));

    if (sequential_data == NULL || parallel_data == NULL) {
        // printf("ERROR: Failed to allocate test arrays\n");
        fprintf(stderr, "ERROR: Ошибка выделения памяти для тестовых данных\n");
        if (sequential_data) free(sequential_data);
        if (parallel_data) free(parallel_data);
        metrics.sequentialTime = -1;
        metrics.parallelTime = -1;
        return metrics;
    }
    
    // printf("DEBUG: Starting sequential sort\n");
    // последовательная сортировка
    memcpy(sequential_data, TEST_ORIGINAL_ARRAY, ARRAY_SIZE * sizeof(int));
    double start_time = get_time();
    if (start_time < 0) {
        // printf("ERROR: Failed to get start time for sequential sort\n");
        metrics.sequentialTime = -1;
    } else {
        int res = sequentialMergeSort(sequential_data, 0, ARRAY_SIZE - 1);
        double end_time = get_time();
        if (end_time < 0 || res != 0) {
            // printf("ERROR: Sequential sort failed with code %d\n", res);
            metrics.sequentialTime = -1;
        } else {
            metrics.sequentialTime = end_time - start_time;
            // printf("DEBUG: Sequential sort completed in %.3f seconds\n", metrics.sequentialTime);
        }
    }

    // printf("DEBUG: Starting parallel sort\n");
    // параллельная сортировка
    memcpy(parallel_data, TEST_ORIGINAL_ARRAY, ARRAY_SIZE * sizeof(int));
    start_time = get_time();
    if (start_time < 0) {
        // printf("ERROR: Failed to get start time for parallel sort\n");
        metrics.sequentialTime = -1;
    } else {
        int res = parallelMergeSort(parallel_data, ARRAY_SIZE);
        double end_time = get_time();
        if (end_time < 0 || res != 0) {
            // printf("ERROR: Parallel sort failed with code %d\n", res);
            metrics.parallelTime = -1;
        } else {
            metrics.parallelTime = end_time - start_time;
            // printf("DEBUG: Parallel sort completed in %.3f seconds\n", metrics.parallelTime);
        }
    }
    // printf("DEBUG: Checking if arrays are sorted\n");
    metrics.isCorrect = isSorted(sequential_data, ARRAY_SIZE) && 
                        isSorted(parallel_data, ARRAY_SIZE) &&
                        arraysEqual(sequential_data, parallel_data, ARRAY_SIZE);
    
    // printf("DEBUG: Freeing test arrays\n");
    free(sequential_data);
    free(parallel_data);
    // printf("DEBUG: Comparison completed\n");

    return metrics;
}

int run_custom_test(int size, int threads, int parallel_thresh, int seq_thresh) {
    int original_size = ARRAY_SIZE;
    int original_threads = MAX_THREADS;
    int original_parallel = PARALLEL_THRESHOLD;
    int original_seq = SEQUENTIAL_THRESHOLD;
    
    ARRAY_SIZE = size;
    MAX_THREADS = threads;
    PARALLEL_THRESHOLD = parallel_thresh;
    SEQUENTIAL_THRESHOLD = seq_thresh;

    metrics_t metrics = run_comparison();
    
    if (metrics.sequentialTime < 0 || metrics.parallelTime < 0) {
        printf("Размер: %9d | Потоки: %2d | Порог пар.: %5d | ОШИБКА ВЫПОЛНЕНИЯ\n",
               size, threads, parallel_thresh);
        
        ARRAY_SIZE = original_size;
        MAX_THREADS = original_threads;
        PARALLEL_THRESHOLD = original_parallel;
        SEQUENTIAL_THRESHOLD = original_seq;
        return -1;
    }
    
    double speedup = (metrics.parallelTime > 0) ? metrics.sequentialTime / metrics.parallelTime : 0.0;
    
    printf("Размер: %9d | Потоки: %2d | Порог пар.: %5d | Послед.: %6.3fс | Паралл.: %6.3fс | Ускорение: %5.2fx | %s\n",
           size, threads, parallel_thresh, metrics.sequentialTime, metrics.parallelTime,
           speedup,
           metrics.isCorrect ? "OK" : "ERROR");
    
    ARRAY_SIZE = original_size;
    MAX_THREADS = original_threads;
    PARALLEL_THRESHOLD = original_parallel;
    SEQUENTIAL_THRESHOLD = original_seq;
    
    return metrics.isCorrect ? 0 : -1;
}

int run_size_test_suite() {
    printf("=== ТЕСТ: ВЛИЯНИЕ РАЗМЕРА МАССИВА ===\n");
    printf("Параметры: потоки=8, порог=1000, последовательный порог=100\n");
    printf("===============================================================================\n");
    int sizes[] = {1000000, 5000000, 10000000, 25000000, 50000000, 75000000, 100000000};
    int error_count = 0;

    for (int i = 0; i < 7; i++) {
        if (run_custom_test(sizes[i], 8, 1000, 100) != 0) error_count++;
    }
    printf("\n");
    return (error_count == 0) ? 0 : -1;
}

int run_threads_test_suite() {
    printf("=== ТЕСТ: ВЛИЯНИЕ КОЛИЧЕСТВА ПОТОКОВ ===\n");
    printf("Параметры: размер=50000000, порог=1000, последовательный порог=100\n");
    printf("===============================================================================\n");
    int threads_list[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12}; 
    int error_count = 0;

    for (int i = 0; i < 12; i++) {
        if (run_custom_test(50000000, threads_list[i], 1000, 100) != 0) error_count++;
    }
    printf("\n");
    return (error_count == 0) ? 0 : -1;
}

int run_threshold_test_suite() {
    printf("=== ТЕСТ: ВЛИЯНИЕ ПОРОГОВЫХ ЗНАЧЕНИЙ ===\n");
    printf("Параметры: размер=50000000, потоки=8, последовательный порог=100\n");
    printf("===============================================================================\n");
    int thresholds[] = {100, 500, 1000, 2500, 5000, 10000, 25000, 50000};
    int error_count = 0;

    for (int i = 0; i < 8; i++) {
        if (run_custom_test(50000000, 8, thresholds[i], 100) != 0) error_count++;
    }
    printf("\n");
    return (error_count == 0) ? 0 : -1;
}