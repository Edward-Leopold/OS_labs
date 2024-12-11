#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>

#define MAX_LINE_SIZE 1024

int main() {
    int pipe1[2], pipe2[2];
    pid_t pid1, pid2;
    char filename1[MAX_LINE_SIZE], filename2[MAX_LINE_SIZE];
    char buffer[MAX_LINE_SIZE];

    if (pipe(pipe1) == -1 || pipe(pipe2) == -1) {
        exit(EXIT_FAILURE);
    }

    write(STDOUT_FILENO, "Enter filename for output file for child 1: ", sizeof(char) * 44);
    ssize_t len = read(STDIN_FILENO, filename1, sizeof(char) * MAX_LINE_SIZE);
    filename1[len - 1] = '\0';

    write(STDOUT_FILENO, "Enter filename for output file for child 2: ", sizeof(char) * 44);
    len = read(STDIN_FILENO, filename2, sizeof(char) * MAX_LINE_SIZE);
    filename2[len - 1] = '\0'; 

    pid1 = fork();
    if (pid1 == -1) {
        exit(EXIT_FAILURE);
    }

    if (pid1 == 0) {
        close(pipe1[1]); 
        dup2(pipe1[0], STDIN_FILENO); 
        execl("./child1", "child1", filename1, NULL);

        close(pipe1[0]); 

        exit(EXIT_FAILURE);
    }

    pid2 = fork();
    if (pid2 == -1) {
        exit(EXIT_FAILURE);
    }

    if (pid2 == 0) {
        close(pipe2[1]); 
        dup2(pipe2[0], STDIN_FILENO); 
        execl("./child2", "child2", filename2, NULL);
        close(pipe2[0]);
        exit(EXIT_FAILURE);
    }

    close(pipe1[0]); 
    close(pipe2[0]); 

    while (1) {
        write(STDOUT_FILENO, "Enter a line (or 'exit' to quit): ", 34);
        len = read(STDIN_FILENO, buffer, MAX_LINE_SIZE);
        if (len <= 0) {
            break;
        }
        buffer[len - 1] = '\0'; 

        if (strcmp(buffer, "exit") == 0) {
            close(pipe1[1]);
            close(pipe2[1]);    
            break;
        }

        if (strlen(buffer) >= 10) {
            write(pipe2[1], buffer, strlen(buffer) + 1);
        } else {
            write(pipe1[1], buffer, strlen(buffer) + 1);
        }
    }

    close(pipe1[1]);
    close(pipe2[1]);

    if (pid1 != 0){
        wait(NULL);
    }
    if (pid2 != 0){
        wait(NULL);
    }
    // wait(NULL);

    return 0;
}
