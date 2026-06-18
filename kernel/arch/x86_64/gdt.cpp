/**
 * novaOS — GDT Implementation
 */
#include "gdt.h"

static GDTEntry  gdt_entries[6];
static GDTPointer gdt_ptr;

extern "C" void gdt_flush(uint32_t);

void GDT::set_entry(int num, uint32_t base, uint32_t limit,
                    uint8_t access, uint8_t gran) {
    gdt_entries[num].base_low    = (base & 0xFFFF);
    gdt_entries[num].base_middle = (base >> 16) & 0xFF;
    gdt_entries[num].base_high   = (base >> 24) & 0xFF;
    gdt_entries[num].limit_low   = (limit & 0xFFFF);
    gdt_entries[num].granularity = ((limit >> 16) & 0x0F) | (gran & 0xF0);
    gdt_entries[num].access      = access;
}

void GDT::init() {
    gdt_ptr.limit = sizeof(GDTEntry) * 6 - 1;
    gdt_ptr.base  = (uint32_t)&gdt_entries;

    set_entry(0, 0, 0,          0,    0);      // Null
    set_entry(1, 0, 0xFFFFFFFF, 0x9A, 0xCF);  // Kernel code  Ring 0
    set_entry(2, 0, 0xFFFFFFFF, 0x92, 0xCF);  // Kernel data  Ring 0
    set_entry(3, 0, 0xFFFFFFFF, 0xFA, 0xCF);  // User code    Ring 3
    set_entry(4, 0, 0xFFFFFFFF, 0xF2, 0xCF);  // User data    Ring 3

    gdt_flush((uint32_t)&gdt_ptr);
}
