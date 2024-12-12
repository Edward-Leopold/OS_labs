#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>

#define MAX_THREADS 4


pthread_mutex_t mutex;

typedef struct ThreadObj{
    int len;
    int* result;
    int* array;
} ThreadObj;

void* process_array(void* arg) {
    ThreadObj* args = (ThreadObj*)arg;

    for (int i = 0; i < args->len; i++) {
        pthread_mutex_lock(&mutex);
        args->result[i] += args->array[i];
        pthread_mutex_unlock(&mutex);
    }

    free(args);
    return NULL;
}


void sum_arrays(int len, int* result, int num_arrays, ...) {
    va_list args;
    va_start(args, num_arrays);

    pthread_t threads[MAX_THREADS];
    int threads_created = 0;

    for (int i = 0; i < num_arrays; i++) {
        if (threads_created >= MAX_THREADS) {
            pthread_join(threads[threads_created % MAX_THREADS], NULL);
            threads_created--;
        }

        int* array = va_arg(args, int*);

        ThreadObj* thread_args = (ThreadObj*)malloc(sizeof(ThreadObj));
        thread_args->len = len;
        thread_args->result = result;
        thread_args->array = array;

        pthread_create(&threads[threads_created % MAX_THREADS], NULL, process_array, thread_args);
        threads_created++;
    }

    for (int i = 0; i < threads_created; i++) {
        pthread_join(threads[i], NULL);
    }
    // pthread_exit(NULL);

    va_end(args);
}

int main() {
    int len = 10;
    int num_arrays = 3;

    int arr1[10] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    int arr2[10] = {10, 9, 8, 7, 6, 5, 4, 3, 2, 1};
    int arr3[10] = {2, 2, 2, 2, 2, 2, 2, 2, 2, 2};

    int* result = (int*)calloc(len, sizeof(int));

    sum_arrays(len, result, num_arrays, arr1, arr2, arr3);

    write(STDOUT_FILENO, "Result: ", 8);
    for (int i = 0; i < len; i++) {
        char buffer[12];
        int n = snprintf(buffer, sizeof(buffer), "%d ", result[i]);
        write(STDOUT_FILENO, buffer, n);
    }
    write(STDOUT_FILENO, "\n", 1);

    free(result);

    return 0;
}
