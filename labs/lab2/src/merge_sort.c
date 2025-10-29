#include "merge_sort.h"
#include "common.h"

void getRandomArray(int arr[], int size, int maxValue) {
    static int seeded = 0;
    if (!seeded) {
        srand(time(NULL));
        seeded = 1;
    }
    for (int i = 0; i < size; i++) {
        arr[i] = rand() % maxValue;
    }
}

int isSorted(int arr[], int size) {
    for (int i = 0; i < size - 1; i++) {
        if (arr[i] > arr[i+1]) {
            return 0;
        }
    }
    return 1;
}

void merge(int arr[], int left, int right, int mid) {
    int i, j, k;
    int n1 = mid - left + 1;
    int n2 = right - mid;

    int* L = (int*)malloc(n1*sizeof(int));
    int* R = (int*)malloc(n2*sizeof(int));

    for (i = 0; i < n1; i++) {
        L[i] = arr[left + i];
    }
    for (j = 0; j < n2; j++) {
        R[j] = arr[mid + 1 + j];
    }

    i = 0, j = 0, k = left;
    while (i < n1 && j < n2) {
        if (L[i] <= R[j]) {
            arr[k] = L[i];
            i++;
        } else {
            arr[k] = R[j];
            j++;
        }
        k++;
    }

    while (i < n1) arr[k++] = L[i++];
    while (j < n2) arr[k++] = R[j++];

    free(L); free(R);
}

void insertSort(int arr[], int left, int right) {
    for (int i = left + 1; i <= right; i++) {
        int key = arr[i];
        int j = i - 1;

        while (j >= left && arr[j] > key) {
            arr[j + 1] = arr[j];
            j--;
        }
        arr[j + 1] = key;
    }
}

void sequentialMergeSort(int arr[], int left, int right) {
    if (left >= right) return;

    if (right - left < SEQUENTIAL_THRESHOLD) {
        insertSort(arr, left, right);
        return;
    }

    int mid = left + (right - left) / 2;
    sequentialMergeSort(arr, left, mid);
    sequentialMergeSort(arr, mid + 1, right);
    merge(arr, left, right, mid);
}

void* parallelMergeSortThread(void* arg) {
    thread_data_t* data = (thread_data_t*)arg;
    int* arr = data->arr;
    int left = data->left;
    int right = data->right;

    // Базовый случай - маленький массив
    if (right - left < PARALLEL_THRESHOLD) {
        sequentialMergeSort(arr, left, right);
        free(data);
        return NULL;
    }

    int mid = left + (right - left) / 2;

    pthread_mutex_lock(&THREAD_MUTEX);
    int availableThreads = MAX_THREADS - ACTIVE_THREADS;
    // printf("ДО проверки: left=%d, right=%d, active=%d, max=%d, available=%d\n", 
    //        left, right, ACTIVE_THREADS, MAX_THREADS, availableThreads);
    int can_create_threads = (availableThreads >= 2);
    pthread_mutex_unlock(&THREAD_MUTEX);

    if (can_create_threads) {
        // Резервируем потоки
        pthread_mutex_lock(&THREAD_MUTEX);
        ACTIVE_THREADS += 2;
        // printf("СОЗДАЮ потоки: active стало=%d\n", ACTIVE_THREADS);
        pthread_mutex_unlock(&THREAD_MUTEX);

        pthread_t left_thread, right_thread;
        int left_created = 0, right_created = 0;

        thread_data_t* left_data = (thread_data_t*)malloc(sizeof(thread_data_t));
        thread_data_t* right_data = (thread_data_t*)malloc(sizeof(thread_data_t));

        left_data->arr = arr;
        left_data->left = left;
        left_data->right = mid;

        right_data->arr = arr;
        right_data->left = mid + 1;
        right_data->right = right;

        left_created = (pthread_create(&left_thread, NULL, parallelMergeSortThread, left_data) == 0);
        right_created = (pthread_create(&right_thread, NULL, parallelMergeSortThread, right_data) == 0);

        // printf("Создано потоков: left=%d, right=%d\n", left_created, right_created);

        if (left_created && right_created) {
            pthread_join(left_thread, NULL);
            pthread_join(right_thread, NULL);
        } else {
            // Если не удалось создать - освобождаем память и работаем последовательно
            if (!left_created) free(left_data);
            if (!right_created) free(right_data);
            sequentialMergeSort(arr, left, mid);
            sequentialMergeSort(arr, mid + 1, right);
        }

        // Возвращаем потоки
        pthread_mutex_lock(&THREAD_MUTEX);
        ACTIVE_THREADS -= 2;
        // printf("ЗАВЕРШИЛ потоки: active стало=%d\n", ACTIVE_THREADS);
        pthread_mutex_unlock(&THREAD_MUTEX);
    } else {
        // printf("НЕ создаю потоки - недостаточно available\n");
        sequentialMergeSort(arr, left, mid);
        sequentialMergeSort(arr, mid + 1, right);
    }

    merge(arr, left, right, mid);
    free(data);
    return NULL;
}

void parallelMergeSort(int arr[], int size) {
    if (size <= 1) return;

    pthread_mutex_lock(&THREAD_MUTEX);
    ACTIVE_THREADS = 1;
    pthread_mutex_unlock(&THREAD_MUTEX);

    thread_data_t* data = (thread_data_t*)malloc(sizeof(thread_data_t));
    data->arr = arr; 
    data->left = 0; 
    data->right = size - 1;

    parallelMergeSortThread(data);
    
    pthread_mutex_lock(&THREAD_MUTEX);
    ACTIVE_THREADS = 0;
    pthread_mutex_unlock(&THREAD_MUTEX);
}