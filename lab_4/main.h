#ifndef OPERATIONAL_SYSTEMS_MAIN_H
#define OPERATIONAL_SYSTEMS_MAIN_H

#include <sys/mman.h>
#include <dlfcn.h>
#include <stdio.h>
#include <unistd.h>

// #include "buddy.h"
#include "mkk.h"

typedef Allocator *(alloc_create_func)(void *memory, size_t size);
typedef void *(alloc_alloc_func)(Allocator *allocator, size_t size);
typedef void (alloc_free_func)(Allocator *allocator, void *memory);
typedef void (alloc_destroy_func)(Allocator *allocator);

#endif // OPERATIONAL_SYSTEMS_MAIN_H
