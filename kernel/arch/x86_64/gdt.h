/**
 * novaOS — Global Descriptor Table
 * Defines memory segments and CPU privilege rings.
 * Ring 0 = kernel, Ring 3 = userspace apps.
 */
#pragma once
#include <stdint.h>

struct GDTEntry {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_middle;
    uint8_t  access;
    uint8_t  granularity;
    uint8_t  base_high;
} __attribute__((packed));

struct GDTPointer {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

namespace GDT {
    void init();
    void set_entry(int num, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran);
}
