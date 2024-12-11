#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>

#define MAX_LINE_LENGTH 1024

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
    if (argc != 2) {
        write(STDERR_FILENO, "Usage: child1 <filename>\n", 25);
        exit(EXIT_FAILURE);
    }

    int file = open(argv[1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (file == -1) {
        exit(EXIT_FAILURE);
    }

    char buffer[MAX_LINE_LENGTH];
    ssize_t len;
    while ((len = read(STDIN_FILENO, buffer, MAX_LINE_LENGTH - 1)) > 0) {
        buffer[len] = '\0'; 
        if (strcmp(buffer, "exit\n") == 0 || strcmp(buffer, "exit") == 0) {
            break;
        }

        char* str = remove_vowels(buffer);
        if (!str) {
            const char *error_msg = "Memory allocation failed\n";
            write(STDERR_FILENO, error_msg, strlen(error_msg));
            close(file);
            exit(EXIT_FAILURE);
        }

        write(file, str, strlen(str));
        write(file, "\n", 1);
        free(str);
    }

    close(file);
    return 0;
}