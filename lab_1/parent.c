#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/wait.h>

#define BUFFER_SIZE 1024
#define ERROR_BUFFER_SIZE 256

void write_string(int fd, const char *str) {
    write(fd, str, strlen(str));
}

void read_string(int fd, char *buffer, size_t size) {
    read(fd, buffer, size);
}

int main() {
    int pipe1[2];
    int pipe2[2];
    char filename[BUFFER_SIZE];

    if (pipe(pipe1) == -1 || pipe(pipe2) == -1) {
        write_string(STDOUT_FILENO,"Failed to create pipes");
        exit(EXIT_FAILURE);
    }

    const char *prompt = "Введите имя файла: ";
    write_string(STDOUT_FILENO, prompt);
    read_string(STDIN_FILENO, filename, sizeof(filename));
    filename[strcspn(filename, "\n")] = '\0'; // Удаляем символ новой строки

    int file_descriptor = open(filename, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
    if (file_descriptor == -1) {
        write_string(STDOUT_FILENO,"Не удалось открыть файл");
        exit(EXIT_FAILURE);
    }

    pid_t child_pid = fork();
    if (child_pid == -1) {
        write_string(STDOUT_FILENO,"Не удалось создать процесс");
        exit(EXIT_FAILURE);
    }

    if (child_pid == 0) {
        close(pipe1[1]);
        close(pipe2[0]);

        dup2(pipe1[0], STDIN_FILENO);
        dup2(pipe2[1], STDOUT_FILENO);

        char *args[] = {"./child", NULL};
        execvp(args[0], args);
        write_string(STDOUT_FILENO,"Не удалось запустить дочерний процесс");
        exit(EXIT_FAILURE);
    } else {
        char input[BUFFER_SIZE];
        char valid_msg[BUFFER_SIZE];

        close(pipe1[0]);
        close(pipe2[1]);

        const char *input_prompt = "Введите строки (CTRL+D для завершения):\n";
        write_string(STDOUT_FILENO, input_prompt);

        while (read(STDIN_FILENO, input, sizeof(input)) > 0) {
            input[strcspn(input, "\n")] = '\0'; // Удаляем символ новой строки
            write(pipe1[1], input, strlen(input) + 1);
        }
        close(pipe1[1]);

        while (1) {
            ssize_t bytes_read = read(pipe2[0], valid_msg, sizeof(valid_msg));
            if (bytes_read > 0) {
                valid_msg[bytes_read] = '\0'; // Завершение строки
                write(file_descriptor, valid_msg, strlen(valid_msg)); // Запись валидной строки в файл
            } else if (bytes_read == 0) {
                break; // Конец чтения
            }
        }

        wait(NULL);
        close(pipe2[0]);
        close(file_descriptor);
    }

    return 0;
}