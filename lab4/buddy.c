#include "allocator.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

typedef struct BuddyBlock {
    size_t size;
    bool free;
    struct BuddyBlock* next;
} BuddyBlock;

typedef struct {
    void* memory;
    size_t total_size;
    size_t min_order;
    BuddyBlock* free_lists[32];
    size_t total_requested;
    size_t total_allocated;
} BuddyAllocator;

static size_t next_pow2(size_t size) {
    if (size == 0) return 1;
    return 1ull << (64 - __builtin_clzl(size - 1));
}

static size_t get_level(size_t size) {
    return 64 - __builtin_clzl(size) - 1;
}

Allocator* allocator_create(void* memory, size_t size) {
    BuddyAllocator* alloc = malloc(sizeof(BuddyAllocator));
    memset(alloc, 0, sizeof(BuddyAllocator));
    
    size_t actual_size = next_pow2(size);
    alloc->memory = memory;
    alloc->total_size = actual_size;
    alloc->min_order = 5; // Минимальный блок 32 байта
    
    BuddyBlock* block = (BuddyBlock*)memory;
    block->size = actual_size;
    block->free = true;
    block->next = NULL;
    
    size_t level = get_level(actual_size);
    alloc->free_lists[level] = block;
    alloc->total_requested = 0;
    alloc->total_allocated = 0;
    
    return (Allocator*)alloc;
}

void allocator_destroy(Allocator* allocator) {
    free((BuddyAllocator*)allocator);
}

void* allocator_alloc(Allocator* allocator, size_t size) {
    BuddyAllocator* alloc = (BuddyAllocator*)allocator;
    size_t required = size + sizeof(BuddyBlock);
    size_t block_size = next_pow2(required);
    
    if (block_size < (1u << alloc->min_order)) {
        block_size = 1u << alloc->min_order;
    }
    
    size_t level = get_level(block_size);
    if (level >= 32) return NULL;
    
    int current_level = level;
    while (current_level < 32 && !alloc->free_lists[current_level]) {
        current_level++;
    }
    
    if (current_level >= 32) return NULL;
    
    BuddyBlock* block = alloc->free_lists[current_level];
    while (current_level > level) {
        alloc->free_lists[current_level] = block->next;
        current_level--;
        size_t new_size = block->size / 2;
        
        BuddyBlock* buddy = (BuddyBlock*)((char*)block + new_size);
        block->size = new_size;
        buddy->size = new_size;
        block->free = true;
        buddy->free = true;
        
        block->next = alloc->free_lists[current_level];
        buddy->next = alloc->free_lists[current_level];
        alloc->free_lists[current_level] = block;
        alloc->free_lists[current_level] = buddy;
        
        block = alloc->free_lists[current_level];
    }
    
    alloc->free_lists[current_level] = block->next;
    block->free = false;
    alloc->total_requested += size;
    alloc->total_allocated += block->size + sizeof(BuddyBlock);
    return (void*)(block + 1);
}

void allocator_free(Allocator* allocator, void* ptr) {
    if (!ptr) return;
    
    BuddyAllocator* alloc = (BuddyAllocator*)allocator;
    BuddyBlock* block = (BuddyBlock*)ptr - 1;
    block->free = true;
    
    alloc->total_requested -= block->size;
    alloc->total_allocated -= block->size + sizeof(BuddyBlock);
    
    size_t current_size = block->size;
    while (1) {
        size_t level = get_level(current_size);
        BuddyBlock* buddy = (BuddyBlock*)((uintptr_t)block ^ current_size);
        
        if ((uintptr_t)buddy < (uintptr_t)alloc->memory || 
            (uintptr_t)buddy + current_size > (uintptr_t)alloc->memory + alloc->total_size ||
            !buddy->free || buddy->size != current_size) {
            break;
        }
        
        BuddyBlock** prev = &alloc->free_lists[level];
        while (*prev && *prev != buddy) prev = &(*prev)->next;
        if (*prev) *prev = buddy->next;
        
        block = (block < buddy) ? block : buddy;
        current_size *= 2;
        block->size = current_size;
    }
    
    size_t final_level = get_level(current_size);
    block->next = alloc->free_lists[final_level];
    alloc->free_lists[final_level] = block;
}

AllocatorStats allocator_get_stats(Allocator* allocator) {
    BuddyAllocator* alloc = (BuddyAllocator*)allocator;
    return (AllocatorStats){
        .requested = alloc->total_requested,
        .allocated = alloc->total_allocated
    };
}