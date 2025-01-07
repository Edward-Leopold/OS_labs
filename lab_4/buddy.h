#ifndef OPERATIONAL_SYSTEMS_BUDDY_H
#define OPERATIONAL_SYSTEMS_BUDDY_H

#include <sys/mman.h>
#include <stddef.h>
#include <stdint.h>

#define MIN_BLOCK_SIZE 16
#define MAX_LEVEL 31

typedef struct Block {
    void *address;
    struct Block *next;
} Block;

typedef struct Allocator {
    void *memory;
    size_t size;
    Block *free_lists[MAX_LEVEL + 1];
} Allocator;

Allocator* allocator_create(void* memory, size_t size);

void* allocator_alloc(Allocator *allocator, size_t size);

void allocator_free(Allocator *allocator, void *memory);

void allocator_destroy(Allocator *allocator);

#endif  // OPERATIONAL_SYSTEMS_BUDDY_H
