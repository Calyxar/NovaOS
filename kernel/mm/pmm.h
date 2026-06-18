/**
 * novaOS — Physical Memory Manager
 * Manages physical RAM with a bitmap allocator.
 * Each bit represents one 4KB page frame.
 */
#pragma once
#include <stdint.h>
#include <stddef.h>

struct MultibootInfo;

namespace PMM {
    constexpr size_t PAGE_SIZE = 4096;
    void   init(MultibootInfo* mbi);
    void*  alloc_page();
    void   free_page(void* addr);
    size_t free_pages();
    size_t total_pages();
}
