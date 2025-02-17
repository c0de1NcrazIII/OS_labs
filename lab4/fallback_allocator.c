#include "allocator.h"
#include <sys/mman.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    size_t size;
} FallbackBlock;

Allocator* allocator_create(void* memory, size_t size) {
    return (Allocator*)1; // Заглушка, не используется
}

void allocator_destroy(Allocator* allocator) {}

void* allocator_alloc(Allocator* allocator, size_t size) {
    size_t total_size = size + sizeof(size_t);
    FallbackBlock* block = mmap(NULL, total_size, PROT_READ | PROT_WRITE, 
                                MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (block == MAP_FAILED) return NULL;
    block->size = total_size;
    return block + 1;
}

void allocator_free(Allocator* allocator, void* ptr) {
    if (!ptr) return;
    FallbackBlock* block = (FallbackBlock*)ptr - 1;
    munmap(block, block->size);
}