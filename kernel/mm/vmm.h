/**
 * novaOS — Virtual Memory Manager
 * Manages page tables and virtual address spaces.
 * Each process gets its own page directory (4GB virtual space).
 */
#pragma once
#include <stdint.h>
#include <stddef.h>

namespace VMM {
    constexpr uint32_t PAGE_PRESENT  = 0x1;
    constexpr uint32_t PAGE_WRITE    = 0x2;
    constexpr uint32_t PAGE_USER     = 0x4;

    void   init();
    void   map_page(uint32_t virt, uint32_t phys, uint32_t flags);
    void   unmap_page(uint32_t virt);
    uint32_t virt_to_phys(uint32_t virt);
    uint32_t* create_address_space();
    void   switch_address_space(uint32_t* page_dir);
}
