#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <semaphore.h>

#define BUFFER_SIZE 1024
#define SHM_NAME "/my_shared_memory"
#define SEM_NAME_PARENT "/my_semaphore_parent"
#define SEM_NAME_CHILD "/my_semaphore_child"

int main(int argc, char *argv[]) {
    int shm_fd = shm_open(SHM_NAME, O_RDWR, 0666);
    if (shm_fd == -1) {
        write(STDOUT_FILENO, "Не удалось открыть разделяемую память\n",
              sizeof("Не удалось открыть разделяемую память\n"));
        exit(EXIT_FAILURE);
    }

    char *shared_memory = (char *)mmap(0, BUFFER_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (shared_memory == MAP_FAILED) {
        write(STDOUT_FILENO, "Не удалось отобразить разделяемую память\n",
              sizeof("Не удалось отобразить разделяемую память\n"));
        exit(EXIT_FAILURE);
    }

    // Открываем семафоры
    sem_t *sem_parent = sem_open(SEM_NAME_PARENT, 0);
    sem_t *sem_child = sem_open(SEM_NAME_CHILD, 0);
    if (sem_parent == SEM_FAILED || sem_child == SEM_FAILED) {
        write(STDOUT_FILENO, "Не удалось открыть семафоры\n",
              sizeof("Не удалось открыть семафоры\n"));
        exit(EXIT_FAILURE);
    }

    if (argc < 2) {
        write(STDOUT_FILENO, "Не передано имя файла\n",
              sizeof("Не передано имя файла\n"));
        exit(EXIT_FAILURE);
    }
    int file_descriptor = open(argv[1], O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
    if (file_descriptor == -1) {
        write(STDOUT_FILENO, "Не удалось открыть файл\n",
              sizeof("Не удалось открыть файл\n"));
        exit(EXIT_FAILURE);
    }

    char input[BUFFER_SIZE];
    char output_msg[BUFFER_SIZE];

    while (1) {
        sem_wait(sem_child);

        strncpy(input, shared_memory, BUFFER_SIZE);
        input[BUFFER_SIZE - 1] = '\0';

        if (strlen(input) == 0) {
            break;
        }

        int len = strlen(input);
        if (len > 0 && (input[len - 1] == ';' || input[len - 1] == '.')) {
            snprintf(output_msg, sizeof(output_msg), "Валидная строка: %s\n", input);
            write(STDOUT_FILENO, output_msg, strlen(output_msg));
            write(file_descriptor, output_msg, strlen(output_msg));
        } else {
            snprintf(output_msg, sizeof(output_msg), "Не валидная строка: %s\n", input);
            write(STDOUT_FILENO, output_msg, strlen(output_msg));
            write(file_descriptor, output_msg, strlen(output_msg));
        }
        sem_post(sem_parent);
    }

    munmap(shared_memory, BUFFER_SIZE);
    close(shm_fd);
    sem_close(sem_parent);
    sem_close(sem_child);
    close(file_descriptor);

    return 0;
}
