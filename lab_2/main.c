#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <semaphore.h>

#define MAX_THREADS 4

pthread_mutex_t mutex;

sem_t semaphore;

typedef struct ThreadObj {
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
    sem_post(&semaphore); 

    return NULL;
}

void sum_arrays(int len, int* result, int num_arrays, ...) {
    va_list args;
    va_start(args, num_arrays);

    pthread_t threads[MAX_THREADS];
    int threads_count = 0;

    for (int i = 0; i < num_arrays; i++) {
        sem_wait(&semaphore);

        int* array = va_arg(args, int*);

        ThreadObj* thread_args = (ThreadObj*)malloc(sizeof(ThreadObj));
        thread_args->len = len;
        thread_args->result = result;
        thread_args->array = array;

        pthread_create(&threads[threads_count % MAX_THREADS], NULL, process_array, thread_args);
        threads_count++;
    }

    va_end(args);

    for (int i = 0; i < (threads_count < MAX_THREADS ? threads_count : MAX_THREADS); i++) {
        pthread_join(threads[i], NULL);
    }
}

int main() {
    int len = 10;
    int num_arrays = 8;

    int arr1[10] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    int arr2[10] = {10, 9, 8, 7, 6, 5, 4, 3, 2, 1};
    int arr3[10] = {2, 2, 2, 2, 2, 2, 2, 2, 2, 2};
    int arr4[10] = {2, 2, 2, 2, 2, 2, 2, 2, 2, 2};
    int arr5[10] = {2, 2, 2, 2, 2, 2, 2, 2, 2, 2};
    int arr6[10] = {2, 2, 2, 2, 2, 2, 2, 2, 2, 2};
    int arr7[10] = {2, 2, 2, 2, 2, 2, 2, 2, 2, 2};
    int arr8[10] = {2, 2, 2, 2, 2, 2, 2, 2, 2, 2};

    int* result = (int*)calloc(len, sizeof(int));

    pthread_mutex_init(&mutex, NULL);
    sem_init(&semaphore, 0, MAX_THREADS);

    sum_arrays(len, result, num_arrays, arr1, arr2, arr3, arr4, arr5, arr6, arr7, arr8);

    write(STDOUT_FILENO, "Result: ", 8);
    for (int i = 0; i < len; i++) {
        char buffer[12];
        int n = snprintf(buffer, sizeof(buffer), "%d ", result[i]);
        write(STDOUT_FILENO, buffer, n);
    }
    write(STDOUT_FILENO, "\n", 1);

    free(result);
    pthread_mutex_destroy(&mutex);
    sem_destroy(&semaphore);

    return 0;
}
