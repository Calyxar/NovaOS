/**
 * novaOS — Physical Memory Manager Implementation
 * Bitmap-based page frame allocator. O(n) — good enough for early kernel.
 * TODO: Replace with buddy allocator for better performance.
 */
#include "pmm.h"

static uint32_t* bitmap      = nullptr;
static size_t    total_frames = 0;
static size_t    used_frames  = 0;

static void  bitmap_set  (size_t f) { bitmap[f/32] |=  (1u << (f%32)); }
static void  bitmap_clear(size_t f) { bitmap[f/32] &= ~(1u << (f%32)); }
static bool  bitmap_test (size_t f) { return bitmap[f/32] & (1u << (f%32)); }

void PMM::init(MultibootInfo* mbi) {
    bitmap       = (uint32_t*)0x100000;
    total_frames = (32 * 1024 * 1024) / PAGE_SIZE; // Assume 32MB

    // Mark all used, then free available region
    for (size_t i = 0; i < total_frames / 32; i++)
        bitmap[i] = 0xFFFFFFFF;
    used_frames = total_frames;

    size_t usable_start = 0x200000; // 2MB — after kernel
    size_t usable_end   = 0x2000000;
    for (size_t addr = usable_start; addr < usable_end; addr += PAGE_SIZE) {
        bitmap_clear(addr / PAGE_SIZE);
        used_frames--;
    }
}

void* PMM::alloc_page() {
    for (size_t i = 0; i < total_frames; i++) {
        if (!bitmap_test(i)) {
            bitmap_set(i);
            used_frames++;
            return (void*)(i * PAGE_SIZE);
        }
    }
    return nullptr; // OOM
}

void PMM::free_page(void* addr) {
    size_t frame = (size_t)addr / PAGE_SIZE;
    bitmap_clear(frame);
    used_frames--;
}

size_t PMM::free_pages()  { return total_frames - used_frames; }
size_t PMM::total_pages() { return total_frames; }
