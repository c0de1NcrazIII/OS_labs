#include "allocator.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

typedef struct Block {
    size_t size;
    struct Block* next;
    bool free;
} Block;

typedef struct {
    void* memory;
    size_t total_size;
    Block* free_head;
    size_t total_requested;
    size_t total_allocated;
} BestFitAllocator;

Allocator* allocator_create(void* memory, size_t size) {
    if (size < sizeof(Block)) {
        fprintf(stderr, "Memory size too small\n");
        return NULL;
    }

    BestFitAllocator* alloc = malloc(sizeof(BestFitAllocator));
    if (!alloc) return NULL;

    alloc->memory = memory;
    alloc->total_size = size;
    
    Block* initial = (Block*)memory;
    initial->size = size - sizeof(Block);
    initial->next = NULL;
    initial->free = true;
    
    alloc->free_head = initial;
    alloc->total_requested = 0;
    alloc->total_allocated = 0;
    
    return (Allocator*)alloc;
}

void allocator_destroy(Allocator* allocator) {
    free((BestFitAllocator*)allocator);
}

void* allocator_alloc(Allocator* allocator, size_t size) {
    BestFitAllocator* alloc = (BestFitAllocator*)allocator;
    if (size == 0 || size > alloc->total_size) return NULL;

    Block* best = NULL;
    Block** best_prev = NULL;
    Block* current = alloc->free_head;
    Block** prev = &alloc->free_head;

    // Поиск наилучшего блока
    while (current) {
        if (current->size >= size) {
            if (!best || current->size < best->size) {
                best = current;
                best_prev = prev;
            }
        }
        prev = &current->next;
        current = current->next;
    }

    if (!best) return NULL;

    // Удаляем из списка свободных
    *best_prev = best->next;

    // Разделяем блок при необходимости
    const size_t required = size + sizeof(Block);
    if (best->size > required) {
        Block* new_block = (Block*)((char*)best + required);
        new_block->size = best->size - required;
        new_block->free = true;
        new_block->next = alloc->free_head;
        alloc->free_head = new_block;
        
        best->size = size;
    }

    best->free = false;
    alloc->total_requested += size;
    alloc->total_allocated += best->size + sizeof(Block);
    
    return (void*)(best + 1);
}

void allocator_free(Allocator* allocator, void* ptr) {
    BestFitAllocator* alloc = (BestFitAllocator*)allocator;
    if (!ptr) return;

    Block* block = (Block*)ptr - 1;
    if ((char*)block < (char*)alloc->memory) return;

    block->free = true;

    Block* prev_block = NULL;
    if (block != (Block*)alloc->memory) {
        prev_block = (Block*)((char*)block - sizeof(Block) - ((Block*)((char*)block - sizeof(Block)))->size);
        if ((char*)prev_block < (char*)alloc->memory || !prev_block->free) {
            prev_block = NULL;
        }
    }

    if (prev_block && (char*)prev_block + sizeof(Block) + prev_block->size == (char*)block) {
        prev_block->size += sizeof(Block) + block->size;
        block = prev_block;
    }

    Block* next_block = (Block*)((char*)block + sizeof(Block) + block->size);
    if ((char*)next_block < (char*)alloc->memory + alloc->total_size && 
        next_block->free &&
        (char*)block + sizeof(Block) + block->size == (char*)next_block) {
        block->size += sizeof(Block) + next_block->size;
    }

    block->next = alloc->free_head;
    alloc->free_head = block;

    alloc->total_requested -= block->size;
    alloc->total_allocated -= block->size + sizeof(Block);
}

AllocatorStats allocator_get_stats(Allocator* allocator) {
    BestFitAllocator* alloc = (BestFitAllocator*)allocator;
    double utilization = (alloc->total_allocated > 0) 
        ? (alloc->total_requested * 100.0) / alloc->total_allocated 
        : 0.0;

    return (AllocatorStats){
        .requested = alloc->total_requested,
        .allocated = alloc->total_allocated
    };
}