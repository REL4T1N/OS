#ifndef MERGE_SORT_H
#define MERGE_SORT_H

#include "common.h"

// Функции сортировки
void getRandomArray(int arr[], int size, int maxValue);
int isSorted(int arr[], int size);
void merge(int arr[], int left, int right, int mid);
void insertSort(int arr[], int left, int right);
void sequentialMergeSort(int arr[], int left, int right);
void* parallelMergeSortThread(void* arg);
void parallelMergeSort(int arr[], int size);

#endif