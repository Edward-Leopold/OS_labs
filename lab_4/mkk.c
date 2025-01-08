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
    if (size / PAGESIZE <= 0)
        new_allocator->kmemsizes[0].page_size = size % PAGESIZE;

    new_allocator->kmemsizes[0].frag_size = 0;

    new_allocator->free_page = (FreeList *) mmap(NULL, sizeof(FreeList), PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (new_allocator->free_page == MAP_FAILED) {
        munmap(new_allocator->kmemsizes, sizeof(Page) * k_sizes);
        munmap(new_allocator, sizeof(Allocator));
        return NULL;
    }
    new_allocator->free_page->page = &new_allocator->kmemsizes[0];

    FreeList *freeList = new_allocator->free_page;

    for (int i = 1; i < k_sizes; ++i) {
        new_allocator->kmemsizes[i].page_size = PAGESIZE;
        if (i == k_sizes - 1 && size % PAGESIZE > 0) {
            new_allocator->kmemsizes[i].page_size = size % PAGESIZE;
        }
        new_allocator->kmemsizes[i].start_addr = new_allocator->kmemsizes[i - 1].start_addr + PAGESIZE;
        new_allocator->kmemsizes[i].frag_size = 0;

        freeList->next = (FreeList *) mmap(NULL, sizeof(FreeList), PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (freeList->next == MAP_FAILED) {
            // Cleanup and return NULL in case of failure
            munmap(new_allocator->kmemsizes, sizeof(Page) * k_sizes);
            munmap(new_allocator, sizeof(Allocator));
            return NULL;
        }
        freeList = freeList->next;
        freeList->page = &new_allocator->kmemsizes[i];
    }

    return new_allocator;
}

void *small_alloc(Allocator *const allocator, const size_t size) {
    unsigned char order = NDX(size);
    void *allocated_chunk;

    // If free cells of this size exist, then use them
    if (allocator->freelistarr[order]) {
        allocated_chunk = allocator->freelistarr[order]->val;
        Buffer *residue = allocator->freelistarr[order];
        allocator->freelistarr[order] = allocator->freelistarr[order]->next;
        munmap(residue, sizeof(Buffer));

        return allocated_chunk;
    }

    // Else alloc page for this size

    if (!allocator->free_page)
        return NULL;

    Page *free_page = allocator->free_page->page;

    allocator->free_page = allocator->free_page->next;
    free_page->frag_size = 1 << (MIN_ORDER + order);

    int fragments = free_page->page_size / free_page->frag_size;

    allocator->freelistarr[order] = (Buffer *) mmap(NULL, sizeof(Buffer), PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

    Buffer *current_buf = allocator->freelistarr[order];

    allocated_chunk = free_page->start_addr;
    current_buf->val = free_page->start_addr + free_page->frag_size;

    for (int i = 2; i < fragments; ++i) {
        current_buf->next = (Buffer *) mmap(NULL, sizeof(Buffer), PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        current_buf = current_buf->next;
        current_buf->val = free_page->start_addr + free_page->frag_size * i;
    }

    return allocated_chunk;
}

void *large_alloc(Allocator *const allocator, const size_t size) {
    size_t need_pages = size / PAGESIZE + (size % PAGESIZE != 0);

    if (need_pages > allocator->page_count)
        return NULL;

    FreeList *cur = allocator->free_page, *prev = NULL;

    int available = 0;
    Page *available_start = NULL;

    while (cur) {
        if (cur->next && cur->next->page->start_addr - cur->page->page_size == cur->page->start_addr) {
            available++;
            if (!available_start) {
                available_start = cur->page;
            } 
        } else {
            available_start = NULL;
            available = 0;
        }

        if (available >= need_pages) break;

        if (!available) {
            prev = cur;
        }

        cur = cur->next;
    }

    if (available >= need_pages) {
        Page *allocated_page = available_start;
        for (int i = 0; i < need_pages; ++i) {
            allocated_page->frag_size = -1;
            allocated_page = (Page *)((char*)allocated_page->start_addr + allocated_page->page_size);
        }

        if (prev) {
            prev->next = cur ? cur->next : NULL;
        } else {
            allocator->free_page = cur ? cur->next : NULL;
        }

        munmap(cur, sizeof(FreeList));
        return available_start->start_addr;
    }

    return NULL;
}

void *allocator_alloc(Allocator *const allocator, const size_t size) {
    if (size <= PAGESIZE / 2) {
        return small_alloc(allocator, size);
    }

    return large_alloc(allocator, size);
}

void allocator_free(Allocator *const allocator, void *const memory) {
    size_t memdiff = (memory - allocator->kmemsizes[0].start_addr);

    size_t page_id = memdiff / PAGESIZE;
    if (page_id >= allocator->page_count)
        return;

    Page *page = &allocator->kmemsizes[page_id];

    int order = NDX(page->frag_size);

    if (page->frag_size < PAGESIZE) {
        Buffer *new_free_buffer = (Buffer *) mmap(NULL, sizeof(Buffer), PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (!new_free_buffer)
            return;

        new_free_buffer->val = memory;

        new_free_buffer->next = allocator->freelistarr[order];
        allocator->freelistarr[order] = new_free_buffer;
    } else {
        page->frag_size = 0;

        // Free contiguous pages
        if (page_id >= allocator->page_count - 1)
            return;

        int i = 1;
        Page *cur = &allocator->kmemsizes[page_id + i], *prev = page;

        while (cur->frag_size == -1) {
            cur->start_addr = prev->start_addr + prev->page_size;
            cur->frag_size = 0;
            prev = cur;
            i++;
            cur = &allocator->kmemsizes[page_id + i];
        }
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
