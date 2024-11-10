#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#define BUFFER_SIZE 1024
#define ERROR_BUFFER_SIZE 256

int main() {
    char input[BUFFER_SIZE];
    char error_msg[ERROR_BUFFER_SIZE];

    // Чтение из стандартного ввода (должно быть по pipe)
    while (read(STDIN_FILENO, input, sizeof(input)) > 0) {
        // Удаляем символ новой строки
        input[strcspn(input, "\n")] = '\0';

        // Проверяем окончание строки
        if (input[strlen(input) - 1] == ';' || input[strlen(input) - 1] == '.') {
            char tmp[1100];
            int ret = snprintf(tmp, sizeof(tmp), "Валидная строка: %s\n", input);
            if (ret < 0) {
                abort();
            }
            write(STDERR_FILENO, tmp, strlen(tmp));
            write(STDOUT_FILENO, tmp, strlen(tmp));
        } else {
            if (strlen(input)) {
                int ret = snprintf(error_msg, sizeof(error_msg), "Не валидная строка: %s\n", input);
                if (ret < 0) {
                    abort();
                }
                write(STDOUT_FILENO, error_msg, strlen(error_msg));
            }
        }
    }

    return 0;
}
