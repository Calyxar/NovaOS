/**
 * novaOS — Interrupt Descriptor Table
 * Maps CPU interrupts and exceptions to handler functions.
 * Also used for syscalls (int 0x80).
 */
#pragma once
#include <stdint.h>

struct IDTEntry {
    uint16_t base_low;
    uint16_t selector;
    uint8_t  zero;
    uint8_t  flags;
    uint16_t base_high;
} __attribute__((packed));

struct IDTPointer {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

namespace IDT {
    void init();
    void set_gate(uint8_t num, uint32_t base, uint16_t sel, uint8_t flags);
}
