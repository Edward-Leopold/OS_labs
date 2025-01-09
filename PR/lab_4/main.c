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

    // Test 1
    char *small_block1 = (char *)alloc_alloc(allocator, 32);
    char *small_block2 = (char *)alloc_alloc(allocator, 64);
    if (!small_block1 || !small_block2) {
        fprintf(stderr, "Failed to allocate small blocks\n");
        return 3;
    }

    small_block1[0] = 'S';
    small_block1[1] = '1';
    small_block1[2] = '\0';
    small_block2[0] = 'S';
    small_block2[1] = '2';
    small_block2[2] = '\0';

    write(STDOUT_FILENO, small_block1, 3);
    write(STDOUT_FILENO, small_block2, 3);

    alloc_free(allocator, small_block1);
    alloc_free(allocator, small_block2);

    // Test 2
    char *large_block1 = (char *)alloc_alloc(allocator, 2048);
    char *large_block2 = (char *)alloc_alloc(allocator, 4096);
    if (!large_block1 || !large_block2) {
        fprintf(stderr, "Failed to allocate large blocks\n");
        return 3;
    }

    large_block1[0] = 'L';
    large_block1[1] = '1';
    large_block1[2] = '\0';
    large_block2[0] = 'L';
    large_block2[1] = '2';
    large_block2[2] = '\0';

    write(STDOUT_FILENO, large_block1, 3);
    write(STDOUT_FILENO, large_block2, 3);

    alloc_free(allocator, large_block1);
    alloc_free(allocator, large_block2);

    // Test 3
    char *mixed_block1 = (char *)alloc_alloc(allocator, 128);
    char *mixed_block2 = (char *)alloc_alloc(allocator, 1024);
    char *mixed_block3 = (char *)alloc_alloc(allocator, 8);
    if (!mixed_block1 || !mixed_block2 || !mixed_block3) {
        fprintf(stderr, "Failed to allocate mixed blocks\n");
        return 3;
    }

    mixed_block1[0] = 'M';
    mixed_block1[1] = '1';
    mixed_block1[2] = '\0';
    mixed_block2[0] = 'M';
    mixed_block2[1] = '2';
    mixed_block2[2] = '\0';
    mixed_block3[0] = 'M';
    mixed_block3[1] = '3';
    mixed_block3[2] = '\0';

    write(STDOUT_FILENO, mixed_block1, 3);
    write(STDOUT_FILENO, mixed_block2, 3);
    write(STDOUT_FILENO, mixed_block3, 3);

    alloc_free(allocator, mixed_block1);
    alloc_free(allocator, mixed_block2);
    alloc_free(allocator, mixed_block3);

    // Test 4
    char *max_block = (char *)alloc_alloc(allocator, 1024 * 20);
    if (max_block) {
        max_block[0] = 'X';
        max_block[1] = 'M';
        max_block[2] = '\0';
        write(STDOUT_FILENO, max_block, 3);
        alloc_free(allocator, max_block);
    } else {
        fprintf(stderr, "Failed to allocate maximum block\n");
    }

    alloc_destroy(allocator);

    if (munmap(memory, 100000) == -1) {
        perror("munmap");
    }

    dlclose(library);

    return 0;
}
