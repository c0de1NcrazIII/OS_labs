#include <stdlib.h>
#include <stdio.h>
#include <dlfcn.h>
#include <sys/mman.h>
#include <time.h>
#include "allocator.h"

Allocator* allocator_create(void* memory, size_t size) {
    return (Allocator*)1; // Заглушка
}

void allocator_destroy(Allocator* allocator) {}

void* allocator_alloc(Allocator* allocator, size_t size) {
    return malloc(size);
}

void allocator_free(Allocator* allocator, void* memory) {
    free(memory);
}

AllocatorStats allocator_get_stats(Allocator* allocator) {
    return (AllocatorStats){0, 0}; // Не поддерживается для fallback
}

// Загрузка аллокатора из библиотеки
typedef Allocator* (*CreateFunc)(void*, size_t);
typedef void (*DestroyFunc)(Allocator*);
typedef void* (*AllocFunc)(Allocator*, size_t);
typedef void (*FreeFunc)(Allocator*, void*);
typedef AllocatorStats (*StatsFunc)(Allocator*);

CreateFunc create_allocator = NULL;
DestroyFunc destroy_allocator = NULL;
AllocFunc alloc = NULL;
FreeFunc free_ptr = NULL;
StatsFunc get_stats = NULL;

void load_default_allocators() {
    create_allocator = allocator_create;
    destroy_allocator = allocator_destroy;
    alloc = allocator_alloc;
    free_ptr = allocator_free;
    get_stats = allocator_get_stats;
}

void test_performance(Allocator* allocator) {
    const int NUM_OPS = 100000;
    void* blocks[NUM_OPS];
    struct timespec start, end;

    clock_gettime(CLOCK_MONOTONIC, &start);
    for (int i = 0; i < NUM_OPS; i++) {
        blocks[i] = alloc(allocator, 32);
    }
    clock_gettime(CLOCK_MONOTONIC, &end);
    double alloc_time = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;

    // Расчет фактора использования
    AllocatorStats stats = get_stats(allocator);
    double utilization = (stats.requested * 100.0) / stats.allocated;

    clock_gettime(CLOCK_MONOTONIC, &start);
    for (int i = 0; i < NUM_OPS; i++) {
        free_ptr(allocator, blocks[i]);
    }
    clock_gettime(CLOCK_MONOTONIC, &end);
    double free_time = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;

    printf("Allocation time: %.6f sec\n", alloc_time);
    printf("Free time: %.6f sec\n", free_time);
    printf("Memory utilization: %.2f%%\n", utilization);
}

int main(int argc, char** argv) {
    void* lib_handle = NULL;
    if (argc > 1) {
        lib_handle = dlopen(argv[1], RTLD_LAZY);
        if (lib_handle) {
            create_allocator = (CreateFunc)dlsym(lib_handle, "allocator_create");
            destroy_allocator = (DestroyFunc)dlsym(lib_handle, "allocator_destroy");
            alloc = (AllocFunc)dlsym(lib_handle, "allocator_alloc");
            free_ptr = (FreeFunc)dlsym(lib_handle, "allocator_free");
            get_stats = (StatsFunc)dlsym(lib_handle, "allocator_get_stats");
            if (!create_allocator || !destroy_allocator || !alloc || !free_ptr || !get_stats) {
                dlclose(lib_handle);
                lib_handle = NULL;
            }
        }
    }
    if (!lib_handle) load_default_allocators();

    size_t size = 1 << 24; // 16 MB 
    void* memory = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    Allocator* allocator = create_allocator(memory, size);

    test_performance(allocator);

    destroy_allocator(allocator);
    munmap(memory, size);
    if (lib_handle) dlclose(lib_handle);
    return 0;
}