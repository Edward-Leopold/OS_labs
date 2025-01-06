#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <semaphore.h>
#include <sys/wait.h>

#define SHM_NAME "/shared_memory"
#define BUFFER_SIZE 1024

typedef struct {
    char buffer[BUFFER_SIZE];
    int done;
} shared_data_t;

int main() {
    pid_t pid1, pid2;
    char filename1[BUFFER_SIZE], filename2[BUFFER_SIZE];
    int shm_fd;
    shared_data_t *shared_data;
    sem_t *sem_parent, *sem_child1, *sem_child2;

    shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    if (shm_fd == -1) {
        perror("shm_open");
        exit(EXIT_FAILURE);
    }
    if (ftruncate(shm_fd, sizeof(shared_data_t)) == -1) {
        perror("ftruncate");
        exit(EXIT_FAILURE);
    }

    shared_data = mmap(NULL, sizeof(shared_data_t), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (shared_data == MAP_FAILED) {
        perror("mmap");
        exit(EXIT_FAILURE);
    }

    sem_parent = sem_open("/sem_parent", O_CREAT, 0666, 1);
    sem_child1 = sem_open("/sem_child1", O_CREAT, 0666, 0);
    sem_child2 = sem_open("/sem_child2", O_CREAT, 0666, 0);
    if (sem_parent == SEM_FAILED || sem_child1 == SEM_FAILED || sem_child2 == SEM_FAILED) {
        perror("sem_open");
        exit(EXIT_FAILURE);
    }

    write(STDOUT_FILENO, "Enter filename for output file for child 1: ", sizeof(char) * 44);
    ssize_t len = read(STDIN_FILENO, filename1, sizeof(char) * BUFFER_SIZE);
    filename1[len - 1] = '\0';

    write(STDOUT_FILENO, "Enter filename for output file for child 2: ", sizeof(char) * 44);
    len = read(STDIN_FILENO, filename2, sizeof(char) * BUFFER_SIZE);
    filename2[len - 1] = '\0'; 


    pid1 = fork();
    if (pid1 == 0) {
        execl("./child", "child", filename1, "sem_child1", "sem_parent", NULL);
        perror("execl");
        exit(EXIT_FAILURE);
    }

    pid2 = fork();
    if (pid2 == 0) {
        execl("./child", "child", filename2, "sem_child2", "sem_parent", NULL);
        perror("execl");
        exit(EXIT_FAILURE);
    }

    while (1) {
        write(STDOUT_FILENO, "Enter a line (or 'exit' to quit): ", 34);
        // fgets(shared_data->buffer, BUFFER_SIZE, stdin);
        len = read(STDIN_FILENO, shared_data->buffer, BUFFER_SIZE);
        if (len <= 0) {
            break;
        }
        shared_data->buffer[len - 1] = '\0';

        if (strcmp(shared_data->buffer, "exit") == 0) {
            shared_data->done = 1;
            sem_post(sem_child1);
            sem_post(sem_child2);
            break;
        }

        shared_data->done = 0;
        if (strlen(shared_data->buffer) >= 10) {
            sem_post(sem_child2);
        } else {
            sem_post(sem_child1);
        }
        sem_wait(sem_parent);
    }

    wait(NULL);
    wait(NULL);

    munmap(shared_data, sizeof(shared_data_t));
    shm_unlink(SHM_NAME);
    sem_close(sem_parent);
    sem_close(sem_child1);
    sem_close(sem_child2);
    sem_unlink("/sem_parent");
    sem_unlink("/sem_child1");
    sem_unlink("/sem_child2");

    return 0;
}
