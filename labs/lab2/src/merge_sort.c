#include "merge_sort.h"
#include "common.h"

void getRandomArray(int arr[], int size, int maxValue) {
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

int arraysEqual(int arr1[], int arr2[], int size) {
    for (int i = 0; i < size; i++) {
        if (arr1[i] != arr2[i]) {
            return 0;
        }
    }
    return 1;
}

int merge(int arr[], int left, int right, int mid) {
    int i, j, k;
    int n1 = mid - left + 1;
    int n2 = right - mid;

    // printf("DEBUG merge: left=%d, right=%d, mid=%d, n1=%d, n2=%d\n", left, right, mid, n1, n2);

    int* L = (int*)malloc(n1*sizeof(int));
    if (L == NULL) {
        // printf("ERROR: Failed to allocate L[%d]\n", n1);        
        return -1;
    }

    int* R = (int*)malloc(n2*sizeof(int));
    if (R == NULL) {
        // printf("ERROR: Failed to allocate R[%d]\n", n2);
        free(L);
        return -1;
    }

    // printf("DEBUG merge: arrays allocated successfully\n");

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

    // printf("DEBUG merge: merging completed, freeing memory\n");
    free(L); free(R);
    // printf("DEBUG merge: memory freed\n");
    return 0;
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

int sequentialMergeSort(int arr[], int left, int right) {
    if (left >= right) return 0;

    if (right - left < SEQUENTIAL_THRESHOLD) {
        insertSort(arr, left, right);
        return 0;
    }

    int mid = left + (right - left) / 2;
    if (sequentialMergeSort(arr, left, mid) != 0) {return -1;}
    if (sequentialMergeSort(arr, mid + 1, right) != 0) {return -1;}
    
    if (merge(arr, left, right, mid) != 0) {return -1;}
    return 0;
}

void* parallelMergeSortThread(void* arg) {
    thread_data_t* data = (thread_data_t*)arg;
    int* arr = data->arr;
    int left = data->left;
    int right = data->right;

    if (data->error_code != 0) {
        free(data);
        return NULL;
    }
    
    int res;
    // Базовый случай - маленький массив
    if (right - left < PARALLEL_THRESHOLD) {
        res = sequentialMergeSort(arr, left, right);
        if (res != 0) {
            data->error_code = res;
        }
        free(data);
        return NULL;
    }

    int mid = left + (right - left) / 2;
    int threads_created = 0;

    if (pthread_mutex_lock(&THREAD_MUTEX) != 0) {
        data->error_code = -1;
        return NULL;
    }

    int availableThreads = MAX_THREADS - ACTIVE_THREADS;
    int can_create_threads = (availableThreads >= 2);
    
    pthread_mutex_unlock(&THREAD_MUTEX); 
    if (can_create_threads) {
        // Резервируем потоки
        if (pthread_mutex_lock(&THREAD_MUTEX) != 0) {
            data->error_code = -1;
            return NULL;
        }
        ACTIVE_THREADS += 2;
        pthread_mutex_unlock(&THREAD_MUTEX); 
        threads_created = 1;

        pthread_t left_thread, right_thread;
        int left_created = 0, right_created = 0;
        thread_data_t* left_data = NULL;
        thread_data_t* right_data = NULL;

        left_data = (thread_data_t*)malloc(sizeof(thread_data_t));
        if (left_data == NULL) {
            data->error_code = -2;
            threads_created = 0;
        } else {
            right_data = (thread_data_t*)malloc(sizeof(thread_data_t));
            if (right_data == NULL) {
                data->error_code = -2;
                free(left_data);
                threads_created = 0;
            }
        }

        if (threads_created && data->error_code == 0) {
            left_data->arr = arr;
            left_data->left = left;
            left_data->right = mid;
            left_data->error_code = 0;

            right_data->arr = arr;
            right_data->left = mid + 1;
            right_data->right = right;
            right_data->error_code = 0;

            left_created = (pthread_create(&left_thread, NULL, parallelMergeSortThread, left_data) == 0);
            right_created = (pthread_create(&right_thread, NULL, parallelMergeSortThread, right_data) == 0);

            if (left_created && right_created) {
                if (pthread_join(left_thread, NULL) != 0) {
                    data->error_code = -3;
                }
                if (pthread_join(right_thread, NULL) != 0) {
                    data->error_code = -3;
                }

                // проверка ошибок в дочерних потоках
                if (left_data->error_code != 0) {
                    data->error_code = left_data->error_code;
                }
                if (right_data->error_code != 0) {
                    data->error_code = right_data->error_code;
                }
            } else {
                threads_created = 0;
                if (!left_created) free(left_data);
                if (!right_created) free(right_data);
            }
        }

        if (left_data != NULL && threads_created) {
            free(left_data);
        }
        if (right_data != NULL && threads_created) {
            free(right_data);
        }        

        if (!threads_created && data->error_code == 0) {
            res = sequentialMergeSort(arr, left, mid);
            if (res != 0) {
                data->error_code = res;
            } else {
                res = sequentialMergeSort(arr, mid + 1, right);
                if (res != 0) {
                    data->error_code = res;
                }
            }
        }
        
        if (threads_created) {
            // Возвращаем потоки
            if (pthread_mutex_lock(&THREAD_MUTEX) != 0) {
                if (data->error_code == 0) {
                    data->error_code = -1;
                }
            } else {
                ACTIVE_THREADS -= 2;
                pthread_mutex_unlock(&THREAD_MUTEX);
            }
        }
    } else {
        res = sequentialMergeSort(arr, left, mid);
        if (res != 0) {
            data->error_code = res;
            free(data);
            return NULL;
        }

        res = sequentialMergeSort(arr, mid + 1, right);
        if (res != 0) {
            data->error_code = res;
            free(data);
            return NULL;
        }
    }

    if (data->error_code == 0) {
        res = merge(arr, left, right, mid);
        if (res != 0) {
            data->error_code = res;
        }
    }

    return NULL;
}

int parallelMergeSort(int arr[], int size) {
    if (size <= 1) return 0;

    if (pthread_mutex_lock(&THREAD_MUTEX) != 0 ) {
        return -1;
    }
    ACTIVE_THREADS = 1;
    pthread_mutex_unlock(&THREAD_MUTEX);

    thread_data_t* data = (thread_data_t*)malloc(sizeof(thread_data_t));
    if (data == NULL) {
        return -2;
    }

    data->arr = arr; 
    data->left = 0; 
    data->right = size - 1;
    data->error_code = 0;

    parallelMergeSortThread(data);

    int error_code = data->error_code;

    if (pthread_mutex_lock(&THREAD_MUTEX) != 0) {
        error_code = (error_code == 0) ? -1 : error_code;
    } else {
        ACTIVE_THREADS = 0;
        pthread_mutex_unlock(&THREAD_MUTEX);
    }

    return error_code;
}