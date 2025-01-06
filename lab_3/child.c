#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <semaphore.h>

#define SHM_NAME "/shared_memory"
#define BUFFER_SIZE 1024
#define EXIT_FLAG "exit"

typedef struct {
    char buffer[BUFFER_SIZE];
    int done; 
} shared_data_t;

char* remove_vowels(const char *str) {
    int len = strlen(str);
    char* new_str = (char*)malloc((len + 1) * sizeof(char));
    if (!new_str) return NULL;

    int pos = 0;
    const char* vowels = "AaEeIiOoUu";
    for (int i = 0; i < len; ++i) {
        char ch = str[i];
        int is_vowel = 0;
        for (int j = 0; vowels[j] != '\0'; ++j) {
            if (ch == vowels[j]) {
                is_vowel = 1;
                break;
            }
        }
        if (!is_vowel) {
            new_str[pos++] = ch;
        }
    }
    new_str[pos] = '\0';
    return new_str;
}

int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <filename> <child_sem> <parent_sem>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    const char *filename = argv[1];
    const char *child_sem_name = argv[2];
    const char *parent_sem_name = argv[3];

    int shm_fd = shm_open(SHM_NAME, O_RDWR, 0666);
    if (shm_fd == -1) {
        perror("shm_open");
        exit(EXIT_FAILURE);
    }

    shared_data_t *shared_data = mmap(NULL, sizeof(shared_data_t), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (shared_data == MAP_FAILED) {
        perror("mmap");
        exit(EXIT_FAILURE);
    }

    sem_t *sem_child = sem_open(child_sem_name, 0);
    sem_t *sem_parent = sem_open(parent_sem_name, 0);
    if (sem_child == SEM_FAILED || sem_parent == SEM_FAILED) {
        perror("sem_open");
        exit(EXIT_FAILURE);
    }

    int file = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (file == -1) {
        perror("open");
        exit(EXIT_FAILURE);
    }

    while (1) {
        sem_wait(sem_child);
        if (shared_data->done) break;

        char *processed = remove_vowels(shared_data->buffer);
        if (processed) {
            write(file, processed, strlen(processed));
            write(file, "\n", 1);
            free(processed);
        }
        sem_post(sem_parent);
    }

    close(file);
    munmap(shared_data, sizeof(shared_data_t));
    sem_close(sem_child);
    sem_close(sem_parent);

    return 0;
}
