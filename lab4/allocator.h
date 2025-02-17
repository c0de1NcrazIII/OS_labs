#ifndef ALLOCATOR_H
#define ALLOCATOR_H

#include <stddef.h>

typedef struct Allocator Allocator;

// Статистика использования памяти
typedef struct {
    size_t requested;  // Запрошенная пользователем память
    size_t allocated;  // Реально выделенная память (с метаданными)
} AllocatorStats;

// Интерфейс аллокатора
Allocator* allocator_create(void* memory, size_t size);
void allocator_destroy(Allocator* allocator);
void* allocator_alloc(Allocator* allocator, size_t size);
void allocator_free(Allocator* allocator, void* ptr);
AllocatorStats allocator_get_stats(Allocator* allocator);

#endif