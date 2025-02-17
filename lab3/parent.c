#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <semaphore.h>

#define BUFFER_SIZE 1024
#define SHM_NAME "/my_shared_memory"
#define SEM_NAME_PARENT "/my_semaphore_parent"
#define SEM_NAME_CHILD "/my_semaphore_child"

static char CLIENT_PROGRAM_NAME[] = "./child";

void write_string(int fd, const char *str) {
    write(fd, str, strlen(str));
}

void read_string(int fd, char *buffer, size_t size) {
    read(fd, buffer, size);
}

int main() {
    char filename[BUFFER_SIZE];
    const char *prompt = "Введите имя файла: ";
    write_string(STDOUT_FILENO, prompt);
    read_string(STDIN_FILENO, filename, sizeof(filename));
    filename[strcspn(filename, "\n")] = '\0'; 

    int shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    if (shm_fd == -1) {
        write_string(STDOUT_FILENO, "Не удалось создать разделяемую память\n");
        exit(EXIT_FAILURE);
    }
    ftruncate(shm_fd, BUFFER_SIZE);

    char *shared_memory = (char *)mmap(0, BUFFER_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (shared_memory == MAP_FAILED) {
        write_string(STDOUT_FILENO, "Не удалось отобразить разделяемую память\n");
        exit(EXIT_FAILURE);
    }

    sem_t *sem_parent = sem_open(SEM_NAME_PARENT, O_CREAT, 0666, 0);
    sem_t *sem_child = sem_open(SEM_NAME_CHILD, O_CREAT, 0666, 0);
    if (sem_parent == SEM_FAILED || sem_child == SEM_FAILED) {
        write_string(STDOUT_FILENO, "Не удалось создать семафоры\n");
        exit(EXIT_FAILURE);
    }

    pid_t child_pid = fork();
    if (child_pid == -1) {
        write_string(STDOUT_FILENO, "Не удалось создать процесс\n");
        exit(EXIT_FAILURE);
    }

    if (child_pid == 0) {
        char *const args[] = {CLIENT_PROGRAM_NAME, filename, NULL};
        execv(CLIENT_PROGRAM_NAME, args);

        const char msg[] = "error: failed to exec into new executable image\n";
        write(STDERR_FILENO, msg, sizeof(msg));
        exit(EXIT_FAILURE);
    } else {
        char input[BUFFER_SIZE];
        const char *input_prompt = "Введите строки (CTRL+D для завершения):\n";
        write_string(STDOUT_FILENO, input_prompt);

        while (1) {
            ssize_t count = read(STDIN_FILENO, input, sizeof(input) - 1);
            if (count <= 0) {
                break;
            }
            input[count] = '\0';
            input[strcspn(input, "\n")] = '\0';
            strncpy(shared_memory, input, BUFFER_SIZE);
            sem_post(sem_child);
            sem_wait(sem_parent);
        }

        strncpy(shared_memory, "", BUFFER_SIZE);
        sem_post(sem_child);

        wait(NULL);

        munmap(shared_memory, BUFFER_SIZE);
        shm_unlink(SHM_NAME);

        sem_close(sem_parent);
        sem_close(sem_child);
        sem_unlink(SEM_NAME_PARENT);
        sem_unlink(SEM_NAME_CHILD);
    }

    return 0;
}
