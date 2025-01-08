#ifndef OPERATIONAL_SYSTEMS_MAIN_H
#define OPERATIONAL_SYSTEMS_MAIN_H

#include <sys/mman.h>
#include <dlfcn.h>
#include <stdio.h>
#include <unistd.h>

// #include "buddy.h"
#include "mkk.h"

typedef Allocator *(alloc_create_func)(void *const memory, const size_t size);
typedef void *(alloc_alloc_func)(Allocator *const allocator, const size_t size);
typedef void (alloc_free_func)(Allocator *const allocator, void *const memory);
typedef void (alloc_destroy_func)(Allocator *const allocator);

#endif // OPERATIONAL_SYSTEMS_MAIN_H
