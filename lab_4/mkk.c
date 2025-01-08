#include "mkk.h"

Allocator *allocator_create(void *const memory, const size_t size) {
    Allocator *new_allocator = (Allocator *) mmap(NULL, sizeof(Allocator), PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (new_allocator == MAP_FAILED)
        return NULL;

    size_t k_sizes = size / PAGESIZE + (size % PAGESIZE != 0);
    new_allocator->page_count = k_sizes;

    new_allocator->kmemsizes = (Page *) mmap(NULL, sizeof(Page) * k_sizes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (new_allocator->kmemsizes == MAP_FAILED) {
        munmap(new_allocator, sizeof(Allocator));
        return NULL;
    }

    new_allocator->kmemsizes[0].start_addr = memory;
    new_allocator->kmemsizes[0].page_size = PAGESIZE;
    if (k_sizes == 1 && size % PAGESIZE > 0) {
        new_allocator->kmemsizes[0].page_size = size % PAGESIZE;
    }

    new_allocator->kmemsizes[0].frag_size = 0;

    new_allocator->free_page = (FreeList *) mmap(NULL, sizeof(FreeList), PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (new_allocator->free_page == MAP_FAILED) {
        munmap(new_allocator->kmemsizes, sizeof(Page) * k_sizes);
        munmap(new_allocator, sizeof(Allocator));
        return NULL;
    }
    new_allocator->free_page->page = &new_allocator->kmemsizes[0];

    FreeList *freeList = new_allocator->free_page;

    for (size_t i = 1; i < k_sizes; ++i) {
        new_allocator->kmemsizes[i].page_size = PAGESIZE;
        if (i == k_sizes - 1 && size % PAGESIZE > 0) {
            new_allocator->kmemsizes[i].page_size = size % PAGESIZE;
        }
        new_allocator->kmemsizes[i].start_addr = (char *)memory + i * PAGESIZE;
        new_allocator->kmemsizes[i].frag_size = 0;

        freeList->next = (FreeList *) mmap(NULL, sizeof(FreeList), PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (freeList->next == MAP_FAILED) {
            destroy_free_pages_list(new_allocator->free_page);
            munmap(new_allocator->kmemsizes, sizeof(Page) * k_sizes);
            munmap(new_allocator, sizeof(Allocator));
            return NULL;
        }
        freeList = freeList->next;
        freeList->page = &new_allocator->kmemsizes[i];
    }
    freeList->next = NULL;

    return new_allocator;
}

void *small_alloc(Allocator *const allocator, const size_t size) {
    unsigned char order = NDX(size);

    if (allocator->freelistarr[order]) {
        Buffer *allocated_buffer = allocator->freelistarr[order];
        void *allocated_chunk = allocated_buffer->val;

        allocator->freelistarr[order] = allocated_buffer->next;
        munmap(allocated_buffer, sizeof(Buffer));

        return allocated_chunk;
    }

    if (!allocator->free_page)
        return NULL;

    Page *free_page = allocator->free_page->page;
    allocator->free_page = allocator->free_page->next;

    free_page->frag_size = 1 << (MIN_ORDER + order);

    int fragments = free_page->page_size / free_page->frag_size;
    allocator->freelistarr[order] = (Buffer *) mmap(NULL, sizeof(Buffer), PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

    Buffer *current_buf = allocator->freelistarr[order];
    void *allocated_chunk = free_page->start_addr;
    current_buf->val = (char *)free_page->start_addr + free_page->frag_size;

    for (int i = 2; i < fragments; ++i) {
        current_buf->next = (Buffer *) mmap(NULL, sizeof(Buffer), PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        current_buf = current_buf->next;
        current_buf->val = (char *)free_page->start_addr + free_page->frag_size * i;
    }
    current_buf->next = NULL;

    return allocated_chunk;
}

void *large_alloc(Allocator *const allocator, const size_t size) {
    size_t need_pages = size / PAGESIZE + (size % PAGESIZE != 0);

    if (need_pages > allocator->page_count)
        return NULL;

    FreeList *cur = allocator->free_page, *prev = NULL;

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
        allocated_page = &allocator->kmemsizes[allocated_page - allocator->kmemsizes + 1];
    }

    if (prev) {
        prev->next = cur ? cur->next : NULL;
    } else {
        allocator->free_page = cur ? cur->next : NULL;
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
    size_t memdiff = (size_t)((char *)memory - (char *)allocator->kmemsizes[0].start_addr);

    size_t page_id = memdiff / PAGESIZE;
    if (page_id >= allocator->page_count)
        return;

    Page *page = &allocator->kmemsizes[page_id];

    if (page->frag_size > 0 && page->frag_size <= PAGESIZE / 2) {
        int order = NDX(page->frag_size);

        Buffer *new_free_buffer = (Buffer *) mmap(NULL, sizeof(Buffer), PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (!new_free_buffer)
            return;

        new_free_buffer->val = memory;
        new_free_buffer->next = allocator->freelistarr[order];
        allocator->freelistarr[order] = new_free_buffer;
    } else {
        page->frag_size = 0;

        FreeList *new_free_page = (FreeList *) mmap(NULL, sizeof(FreeList), PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (!new_free_page)
            return;

        new_free_page->page = page;
        new_free_page->next = allocator->free_page;
        allocator->free_page = new_free_page;
    }
}

void allocator_destroy(Allocator *const allocator) {
    for (int i = 0; i < MAX_ORDER - MIN_ORDER; ++i) {
        destroy_buffer(allocator->freelistarr[i]);
    }
    destroy_free_pages_list(allocator->free_page);

    munmap(allocator->kmemsizes, sizeof(Page) * allocator->page_count);
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
