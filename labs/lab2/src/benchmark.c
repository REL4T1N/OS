#include "benchmark.h"
#include "merge_sort.h"
#include "common.h"

metrics_t run_comparison() {
    metrics_t metrics = {0};
    
    init_test_data(ARRAY_SIZE);
    
    int* sequential_data = (int*)malloc(ARRAY_SIZE * sizeof(int));
    int* parallel_data = (int*)malloc(ARRAY_SIZE * sizeof(int));
    
    memcpy(sequential_data, TEST_ORIGINAL_ARRAY, ARRAY_SIZE * sizeof(int));
    double start_time = get_time();
    sequentialMergeSort(sequential_data, 0, ARRAY_SIZE - 1);
    double end_time = get_time();
    metrics.sequentialTime = end_time - start_time;
    
    memcpy(parallel_data, TEST_ORIGINAL_ARRAY, ARRAY_SIZE * sizeof(int));
    start_time = get_time();
    parallelMergeSort(parallel_data, ARRAY_SIZE);
    end_time = get_time();
    metrics.parallelTime = end_time - start_time;

    metrics.isCorrect = isSorted(sequential_data, ARRAY_SIZE) && 
                        isSorted(parallel_data, ARRAY_SIZE);
    
    free(sequential_data);
    free(parallel_data);
    
    return metrics;
}

void run_custom_test(int size, int threads, int parallel_thresh, int seq_thresh) {
    int original_size = ARRAY_SIZE;
    int original_threads = MAX_THREADS;
    int original_parallel = PARALLEL_THRESHOLD;
    int original_seq = SEQUENTIAL_THRESHOLD;
    
    ARRAY_SIZE = size;
    MAX_THREADS = threads;
    PARALLEL_THRESHOLD = parallel_thresh;
    SEQUENTIAL_THRESHOLD = seq_thresh;

    metrics_t metrics = run_comparison();
    
    printf("Размер: %9d | Потоки: %2d | Порог пар.: %5d | Послед.: %6.3fс | Паралл.: %6.3fс | Ускорение: %5.2fx | %s\n",
           size, threads, parallel_thresh, metrics.sequentialTime, metrics.parallelTime,
           metrics.sequentialTime / metrics.parallelTime,
           metrics.isCorrect ? "OK" : "ERROR");
    
    ARRAY_SIZE = original_size;
    MAX_THREADS = original_threads;
    PARALLEL_THRESHOLD = original_parallel;
    SEQUENTIAL_THRESHOLD = original_seq;
}

void run_size_test_suite() {
    printf("=== ТЕСТ: ВЛИЯНИЕ РАЗМЕРА МАССИВА ===\n");
    printf("Параметры: потоки=8, порог=1000, последовательный порог=100\n");
    printf("===============================================================================\n");
    int sizes[] = {1000000, 5000000, 10000000, 25000000, 50000000, 75000000, 100000000};
    for (int i = 0; i < 7; i++) {
        run_custom_test(sizes[i], 8, 1000, 100);
    }
    printf("\n");
}

void run_threads_test_suite() {
    printf("=== ТЕСТ: ВЛИЯНИЕ КОЛИЧЕСТВА ПОТОКОВ ===\n");
    printf("Параметры: размер=50000000, порог=1000, последовательный порог=100\n");
    printf("===============================================================================\n");
    int threads_list[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20}; 
    for (int i = 0; i < 20; i++) {
        run_custom_test(50000000, threads_list[i], 1000, 100);
    }
    printf("\n");
}

void run_threshold_test_suite() {
    printf("=== ТЕСТ: ВЛИЯНИЕ ПОРОГОВЫХ ЗНАЧЕНИЙ ===\n");
    printf("Параметры: размер=50000000, потоки=8, последовательный порог=100\n");
    printf("===============================================================================\n");
    int thresholds[] = {100, 500, 1000, 2500, 5000, 10000, 25000, 50000};
    for (int i = 0; i < 8; i++) {
        run_custom_test(50000000, 8, thresholds[i], 100);
    }
    printf("\n");
}