#include "buddy.h"

size_t next_power_of_two(size_t size) {
    size_t power = 1;
    while (power < size) {
        power <<= 1;
    }
    return power;
}

size_t get_level(size_t size) {
    size_t level = 0;
    while (size > MIN_BLOCK_SIZE && level <= MAX_LEVEL) {
        size >>= 1;
        level++;
    }
    return level;
}

size_t get_block_size(size_t level) {
    return MIN_BLOCK_SIZE << level;
}

Allocator* allocator_create(void* memory, size_t size) {
    if (!memory || size < MIN_BLOCK_SIZE) return NULL;

    size_t aligned_size = next_power_of_two(size);
    Allocator *allocator = mmap(NULL, sizeof(Allocator), PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (allocator == MAP_FAILED) return NULL;

    allocator->memory = memory;
    allocator->size = aligned_size;

    for (size_t i = 0; i <= MAX_LEVEL; i++) {
        allocator->free_lists[i] = NULL;
    }

    size_t max_level = get_level(aligned_size);
    allocator->free_lists[max_level] = mmap(NULL, sizeof(Block), PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (allocator->free_lists[max_level] == MAP_FAILED) {
        munmap(allocator, sizeof(Allocator));
        return NULL;
    }

    allocator->free_lists[max_level]->address = memory;
    allocator->free_lists[max_level]->next = NULL;

    return allocator;
}

void* allocator_alloc(Allocator *allocator, size_t size) {
    if (!allocator || size == 0 || size > allocator->size)
        return NULL;

    size_t required_size = next_power_of_two(size);
    size_t level = get_level(required_size);

    if (level > MAX_LEVEL)
        return NULL;

    for (size_t l = level; l <= MAX_LEVEL; l++) {
        if (allocator->free_lists[l]) {
            Block *block = allocator->free_lists[l];
            allocator->free_lists[l] = block->next;

            while (l > level) {
                size_t block_size = get_block_size(l);
                void *lower = block->address;
                void *upper = (char*)block->address + block_size / 2;

                Block *buddy = mmap(NULL, sizeof(Block), PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
                if (buddy == MAP_FAILED) {
                    return NULL;
                }
                buddy->address = upper;
                buddy->next = allocator->free_lists[l - 1];
                allocator->free_lists[l - 1] = buddy;

                block->address = lower;
                l--;
            }

            return block->address;
        }
    }

    return NULL;
}

void allocator_free(Allocator *allocator, void *memory) {
    if (!allocator || !memory)
        return;

    size_t offset = (size_t)((char*)memory - (char*)allocator->memory);
    size_t level = 0;
    size_t block_size = get_block_size(0);

    while (offset >= block_size && level <= MAX_LEVEL) {
        block_size <<= 1;
        level++;
    }

    Block *block = mmap(NULL, sizeof(Block), PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (block == MAP_FAILED)
        return;

    block->address = memory;
    block->next = allocator->free_lists[level];
    allocator->free_lists[level] = block;

    while (level < MAX_LEVEL) {
        size_t buddy_offset = offset ^ get_block_size(level);
        void *buddy_address = (void*)((char*)allocator->memory + buddy_offset);

        Block **prev_ptr = &(allocator->free_lists[level]);
        Block *buddy = allocator->free_lists[level];
        while (buddy && buddy->address != buddy_address) {
            prev_ptr = &(buddy->next);
            buddy = buddy->next;
        }

        if (buddy) {
            *prev_ptr = buddy->next;

            if ((uintptr_t)memory < (uintptr_t)buddy_address) {
                block->address = memory;
            } else {
                block->address = buddy_address;
            }

            level++;
            block->next = allocator->free_lists[level];
            allocator->free_lists[level] = block;
        } else {
            break;
        }
    }
}

void allocator_destroy(Allocator *allocator) {
    if (!allocator)
        return;

    for (size_t i = 0; i <= MAX_LEVEL; i++) {
        Block *current = allocator->free_lists[i];
        while (current) {
            Block *next = current->next;
            munmap(current, sizeof(Block));
            current = next;
        }
    }

    munmap(allocator, sizeof(Allocator));
}
