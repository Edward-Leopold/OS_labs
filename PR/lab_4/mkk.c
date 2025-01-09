#include "mkk.h"

Allocator *allocator_create(void *const memory, const size_t size) {
    Allocator *new_allocator = (Allocator *) mmap(NULL, sizeof(Allocator), PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (new_allocator == MAP_FAILED)
        return NULL;

    size_t k_sizes = size / PAGESIZE + (size % PAGESIZE != 0);
    new_allocator->page_count = k_sizes;

    new_allocator->pages_info = (Page *) mmap(NULL, sizeof(Page) * k_sizes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (new_allocator->pages_info == MAP_FAILED) {
        munmap(new_allocator, sizeof(Allocator));
        return NULL;
    }

    new_allocator->pages_info[0].start_addr = memory;
    new_allocator->pages_info[0].page_size = PAGESIZE;
    if (k_sizes == 1 && size % PAGESIZE > 0) {
        new_allocator->pages_info[0].page_size = size % PAGESIZE;
    }

    new_allocator->pages_info[0].frag_size = 0;

    new_allocator->available_pages = (FreeList *) mmap(NULL, sizeof(FreeList), PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (new_allocator->available_pages == MAP_FAILED) {
        munmap(new_allocator->pages_info, sizeof(Page) * k_sizes);
        munmap(new_allocator, sizeof(Allocator));
        return NULL;
    }
    new_allocator->available_pages->page = &new_allocator->pages_info[0];

    FreeList *freeList = new_allocator->available_pages;

    for (size_t i = 1; i < k_sizes; ++i) {
        new_allocator->pages_info[i].page_size = PAGESIZE;
        if (i == k_sizes - 1 && size % PAGESIZE > 0) {
            new_allocator->pages_info[i].page_size = size % PAGESIZE;
        }
        new_allocator->pages_info[i].start_addr = (char *)memory + i * PAGESIZE;
        new_allocator->pages_info[i].frag_size = 0;

        freeList->next = (FreeList *) mmap(NULL, sizeof(FreeList), PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (freeList->next == MAP_FAILED) {
            destroy_free_pages_list(new_allocator->available_pages);
            munmap(new_allocator->pages_info, sizeof(Page) * k_sizes);
            munmap(new_allocator, sizeof(Allocator));
            return NULL;
        }
        freeList = freeList->next;
        freeList->page = &new_allocator->pages_info[i];
    }
    freeList->next = NULL;

    return new_allocator;
}

void *small_alloc(Allocator *const allocator, const size_t size) {
    unsigned char order = NDX(size);

    if (allocator->small_blocks_free_lists[order]) {
        Buffer *allocated_buffer = allocator->small_blocks_free_lists[order];
        void *allocated_chunk = allocated_buffer->block_address;

        allocator->small_blocks_free_lists[order] = allocated_buffer->next;
        munmap(allocated_buffer, sizeof(Buffer));

        return allocated_chunk;
    }

    if (!allocator->available_pages)
        return NULL;

    Page *available_pages = allocator->available_pages->page;
    allocator->available_pages = allocator->available_pages->next;

    available_pages->frag_size = 1 << (MIN_ORDER + order);

    int fragments = available_pages->page_size / available_pages->frag_size;
    allocator->small_blocks_free_lists[order] = (Buffer *) mmap(NULL, sizeof(Buffer), PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

    Buffer *current_buf = allocator->small_blocks_free_lists[order];
    void *allocated_chunk = available_pages->start_addr;
    current_buf->block_address = (char *)available_pages->start_addr + available_pages->frag_size;

    for (int i = 2; i < fragments; ++i) {
        current_buf->next = (Buffer *) mmap(NULL, sizeof(Buffer), PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        current_buf = current_buf->next;
        current_buf->block_address = (char *)available_pages->start_addr + available_pages->frag_size * i;
    }
    current_buf->next = NULL;

    return allocated_chunk;
}

void *large_alloc(Allocator *const allocator, const size_t size) {
    size_t need_pages = size / PAGESIZE + (size % PAGESIZE != 0);

    if (need_pages > allocator->page_count)
        return NULL;

    FreeList *cur = allocator->available_pages, *prev = NULL;

    size_t available = 0;
    Page *available_start = NULL;

    while (cur) {
        if (available == 0) {
            available_start = cur->page;
        }

        available++;
        if (available == need_pages) {
            break;
        }

        prev = cur;
        cur = cur->next;
    }

    if (available < need_pages) {
        return NULL;
    }

    Page *allocated_page = available_start;
    for (size_t i = 0; i < need_pages; ++i) {
        allocated_page->frag_size = -1;
        allocated_page = &allocator->pages_info[allocated_page - allocator->pages_info + 1];
    }

    if (prev) {
        prev->next = cur ? cur->next : NULL;
    } else {
        allocator->available_pages = cur ? cur->next : NULL;
    }

    return available_start->start_addr;
}

void *allocator_alloc(Allocator *const allocator, const size_t size) {
    if (size <= PAGESIZE / 2) {
        return small_alloc(allocator, size);
    }

    return large_alloc(allocator, size);
}

void allocator_free(Allocator *const allocator, void *const memory) {
    size_t memdiff = (size_t)((char *)memory - (char *)allocator->pages_info[0].start_addr);

    size_t page_id = memdiff / PAGESIZE;
    if (page_id >= allocator->page_count)
        return;

    Page *page = &allocator->pages_info[page_id];

    if (page->frag_size > 0 && page->frag_size <= PAGESIZE / 2) {
        int order = NDX(page->frag_size);

        Buffer *new_free_buffer = (Buffer *) mmap(NULL, sizeof(Buffer), PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (!new_free_buffer)
            return;

        new_free_buffer->block_address = memory;
        new_free_buffer->next = allocator->small_blocks_free_lists[order];
        allocator->small_blocks_free_lists[order] = new_free_buffer;
    } else {
        page->frag_size = 0;

        FreeList *new_free_page = (FreeList *) mmap(NULL, sizeof(FreeList), PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (!new_free_page)
            return;

        new_free_page->page = page;
        new_free_page->next = allocator->available_pages;
        allocator->available_pages = new_free_page;
    }
}

void allocator_destroy(Allocator *const allocator) {
    for (int i = 0; i < MAX_ORDER - MIN_ORDER; ++i) {
        destroy_buffer(allocator->small_blocks_free_lists[i]);
    }
    destroy_free_pages_list(allocator->available_pages);

    munmap(allocator->pages_info, sizeof(Page) * allocator->page_count);
    munmap(allocator, sizeof(Allocator));
}

void destroy_buffer(Buffer *start) {
    Buffer *cur = start, *prev;
    while (cur) {
        prev = cur;
        cur = cur->next;
        munmap(prev, sizeof(Buffer));
    }
}

void destroy_free_pages_list(FreeList *start) {
    FreeList *cur = start, *prev;
    while (cur) {
        prev = cur;
        cur = cur->next;
        munmap(prev, sizeof(FreeList));
    }
}
