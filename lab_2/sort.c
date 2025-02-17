#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>
#include <pthread.h>
#include <dispatch/dispatch.h>  // Для GCD-семафоров (macOS)

typedef struct {
    int *array;
    int left;
    int right;
} ThreadArgs;

static int merge_calls = 0;      
static int sort_calls = 0;       
static int threads_created = 0;  

static dispatch_semaphore_t gcd_sem = NULL;


void merge(int *array, int l, int m, int r) {
    __sync_fetch_and_add(&merge_calls, 1);

    int n1 = m - l + 1;
    int n2 = r - m;
    int *L = (int *)malloc(n1 * sizeof(int));
    int *R = (int *)malloc(n2 * sizeof(int));
    if (!L || !R) {
        perror("malloc for merge");
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < n1; i++) {
        L[i] = array[l + i];
    }
    for (int j = 0; j < n2; j++) {
        R[j] = array[m + 1 + j];
    }

    int i = 0, j = 0;
    int k = l;
    while (i < n1 && j < n2) {
        if (L[i] <= R[j]) {
            array[k++] = L[i++];
        } else {
            array[k++] = R[j++];
        }
    }
    while (i < n1) {
        array[k++] = L[i++];
    }
    while (j < n2) {
        array[k++] = R[j++];
    }

    free(L);
    free(R);
}



void seq_merge_sort(int *array, int left, int right) {
    if (left < right) {
        int mid = left + (right - left) / 2;
        seq_merge_sort(array, left, mid);
        seq_merge_sort(array, mid + 1, right);
        merge(array, left, mid, right);
    }
}


void *threaded_mergesort(void *args) {
    __sync_fetch_and_add(&sort_calls, 1);

    ThreadArgs *threadArgs = (ThreadArgs *)args;
    int *array = threadArgs->array;
    int left = threadArgs->left;
    int right = threadArgs->right;

    if (left < right) {
        int mid = left + (right - left) / 2;

        ThreadArgs leftArgs  = { array, left, mid };
        ThreadArgs rightArgs = { array, mid + 1, right };

        pthread_t leftThread, rightThread;
        int leftCreated = 0, rightCreated = 0;

        long left_wait_res = dispatch_semaphore_wait(gcd_sem, DISPATCH_TIME_NOW);
        if (left_wait_res == 0) {
            int ret = pthread_create(&leftThread, NULL, threaded_mergesort, &leftArgs);
            if (ret == 0) {
                __sync_fetch_and_add(&threads_created, 1);
                leftCreated = 1;
            } else {
                perror("pthread_create (left) failed");
                dispatch_semaphore_signal(gcd_sem);
                seq_merge_sort(array, left, mid);
            }
        } else {
            seq_merge_sort(array, left, mid);
        }

        long right_wait_res = dispatch_semaphore_wait(gcd_sem, DISPATCH_TIME_NOW);
        if (right_wait_res == 0) {
            int ret = pthread_create(&rightThread, NULL, threaded_mergesort, &rightArgs);
            if (ret == 0) {
                __sync_fetch_and_add(&threads_created, 1);
                rightCreated = 1;
            } else {
                perror("pthread_create (right) failed");
                dispatch_semaphore_signal(gcd_sem);
                seq_merge_sort(array, mid + 1, right);
            }
        } else {
            seq_merge_sort(array, mid + 1, right);
        }

        if (leftCreated) {
            pthread_join(leftThread, NULL);
            dispatch_semaphore_signal(gcd_sem);
        }
        if (rightCreated) {
            pthread_join(rightThread, NULL);
            dispatch_semaphore_signal(gcd_sem);
        }

        merge(array, left, mid, right);
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

    int *array = (int *)malloc(array_size * sizeof(int));
    if (!array) {
        perror("malloc");
        return EXIT_FAILURE;
    }

    srand((unsigned)time(NULL));
    for (int i = 0; i < array_size; i++) {
        array[i] = rand() % 1000000; 
    }

    gcd_sem = dispatch_semaphore_create(max_threads);

    struct timeval start, end;
    gettimeofday(&start, NULL);

    ThreadArgs args = { array, 0, array_size - 1 };
    threaded_mergesort(&args);

    gettimeofday(&end, NULL);

    double elapsed = (end.tv_sec - start.tv_sec) +
                     (end.tv_usec - start.tv_usec) / 1000000.0;


    int sorted = 1;
    for (int i = 1; i < array_size; i++) {
        if (array[i] < array[i - 1]) {
            sorted = 0;
            break;
        }
    }

    printf("Array is %s\n", (sorted ? "sorted" : "NOT sorted"));
    printf("Time taken: %.6f seconds\n", elapsed);
    // printf("Number of mergesort (threaded) calls: %d\n", sort_calls);
    // printf("Number of merges: %d\n", merge_calls);
    // printf("Threads created: %d\n", threads_created);

    free(array);
    return EXIT_SUCCESS;
}