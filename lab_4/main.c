#include "main.h"

static alloc_create_func *alloc_create;
static alloc_alloc_func *alloc_alloc;
static alloc_free_func *alloc_free;
static alloc_destroy_func *alloc_destroy;

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <library_path>\n", argv[0]);
        return 1;
    }

    void *library = dlopen(argv[1], RTLD_LAZY);
    if (!library) {
        fprintf(stderr, "Error loading library: %s\n", dlerror());
        return 1;
    }

    alloc_create = (alloc_create_func *)dlsym(library, "allocator_create");
    if (!alloc_create) {
        fprintf(stderr, "Error finding symbol: %s\n", dlerror());
        return 1;
    }

    alloc_alloc = (alloc_alloc_func *)dlsym(library, "allocator_alloc");
    if (!alloc_alloc) {
        fprintf(stderr, "Error finding symbol: %s\n", dlerror());
        return 1;
    }

    alloc_free = (alloc_free_func *)dlsym(library, "allocator_free");
    if (!alloc_free) {
        fprintf(stderr, "Error finding symbol: %s\n", dlerror());
        return 1;
    }

    alloc_destroy = (alloc_destroy_func *)dlsym(library, "allocator_destroy");
    if (!alloc_destroy) {
        fprintf(stderr, "Error finding symbol: %s\n", dlerror());
        return 1;
    }

    void *memory = mmap(NULL, 100000, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (memory == MAP_FAILED) {
        perror("mmap");
        return 1;
    }

    Allocator *allocator = alloc_create(memory, 1024 * 20);
    if (!allocator) {
        return 2;
    }

    char *allocated_mem = (char *) alloc_alloc(allocator, 500);
    if (!allocated_mem) {
        return 3;
    }

    char *allocated_mem2 = (char *) alloc_alloc(allocator, 32);
    if (!allocated_mem2) {
        return 3;
    }

    allocated_mem[0] = '1';
    allocated_mem[1] = '\0';
    allocated_mem2[0] = 'l';
    allocated_mem2[1] = '\0';

    write(STDOUT_FILENO, allocated_mem, 2);
    write(STDOUT_FILENO, allocated_mem2, 2);

    alloc_free(allocator, allocated_mem);
    alloc_destroy(allocator);

    if (munmap(memory, 100000) == -1) {
        perror("munmap");
    }

    dlclose(library);

    return 0;
}
