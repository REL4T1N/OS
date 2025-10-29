#ifndef BENCHMARK_H
#define BENCHMARK_H

#include "common.h"

// Функции тестирования
metrics_t run_comparison();
void run_size_test_suite();
void run_threads_test_suite();
void run_threshold_test_suite();
void run_custom_test(int size, int depth, int parallel_thresh, int seq_thresh);

#endif