#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <time.h>
#include <sys/time.h>

#define MAX_THREADS 8 

typedef struct {
    int *array;
    int low;
    int high;
} ThreadArgs;

sem_t semaphore;

void swap(int *a, int *b) {
    int temp = *a;
    *a = *b;
    *b = temp;
}

int partition(int *array, int low, int high) {
    int pivot = array[high];
    int i = low - 1;

    for (int j = low; j < high; j++) {
        if (array[j] <= pivot) {
            i++;
            swap(&array[i], &array[j]);
        }
    }
    swap(&array[i + 1], &array[high]);
    return i + 1;
}

void quicksort(int *array, int low, int high) {
    if (low < high) {
        int pi = partition(array, low, high);
        quicksort(array, low, pi - 1);
        quicksort(array, pi + 1, high);
    }
}

void *threaded_quicksort(void *args) {
    ThreadArgs *threadArgs = (ThreadArgs *)args;
    int *array = threadArgs->array;
    int low = threadArgs->low;
    int high = threadArgs->high;

    if (low < high) {
        int pi = partition(array, low, high);

        ThreadArgs leftArgs = {array, low, pi - 1};
        ThreadArgs rightArgs = {array, pi + 1, high};

        pthread_t leftThread, rightThread;
        int leftCreated = 0, rightCreated = 0;

        // Create threads if available
        if (sem_trywait(&semaphore) == 0) {
            pthread_create(&leftThread, NULL, threaded_quicksort, &leftArgs);
            leftCreated = 1;
        } else {
            quicksort(array, low, pi - 1);
        }

        if (sem_trywait(&semaphore) == 0) {
            pthread_create(&rightThread, NULL, threaded_quicksort, &rightArgs);
            rightCreated = 1;
        } else {
            quicksort(array, pi + 1, high);
        }

        // Wait for threads to finish
        if (leftCreated) {
            pthread_join(leftThread, NULL);
            sem_post(&semaphore);
        }
        if (rightCreated) {
            pthread_join(rightThread, NULL);
            sem_post(&semaphore);
        }
    }

    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <max_threads> <array_size>\n", argv[0]);
        return EXIT_FAILURE;
    }

    int max_threads = atoi(argv[1]);
    int array_size = atoi(argv[2]);

    if (max_threads <= 0 || array_size <= 0) {
        fprintf(stderr, "Error: max_threads and array_size must be positive integers.\n");
        return EXIT_FAILURE;
    }

    int *array = malloc(array_size * sizeof(int));
    if (!array) {
        perror("malloc");
        return EXIT_FAILURE;
    }

    srand(time(NULL));
    for (int i = 0; i < array_size; i++) {
        array[i] = rand() % 1000;
    }

    // printf("Unsorted array:\n");
    // for (int i = 0; i < array_size; i++) {
    //     printf("%d ", array[i]);
    // }
    // printf("\n");

    sem_init(&semaphore, 0, max_threads);

    struct timeval start, end;
    gettimeofday(&start, NULL);

    ThreadArgs args = {array, 0, array_size - 1};
    threaded_quicksort(&args);

    gettimeofday(&end, NULL);

    double elapsed = (end.tv_sec - start.tv_sec) + (end.tv_usec - start.tv_usec) / 1000000.0;

    // printf("Sorted array:\n");
    // for (int i = 0; i < array_size; i++) {
    //     printf("%d ", array[i]);
    // }
    // printf("\n");

    printf("Time taken: %.6f seconds\n", elapsed);

    sem_destroy(&semaphore);
    free(array);

    return EXIT_SUCCESS;
}
