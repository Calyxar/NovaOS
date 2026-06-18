#include "pit.h"

uint32_t pit_ticks = 0;

static void outb(uint16_t port, uint8_t val) {
    asm volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

void PIT::init(uint32_t frequency_hz) {
    // PIT base frequency is 1,193,180 Hz
    uint32_t divisor = 1193180 / frequency_hz;

    // Command: channel 0, lobyte/hibyte, square wave mode
    outb(0x43, 0x36);
    outb(0x40, (uint8_t)(divisor & 0xFF));
    outb(0x40, (uint8_t)((divisor >> 8) & 0xFF));
}

uint32_t PIT::ticks() { return pit_ticks; }

void PIT::sleep_ms(uint32_t ms) {
    uint32_t target = pit_ticks + ms;
    while (pit_ticks < target) asm volatile("hlt");
}

void PIT::handle_irq() { pit_ticks++; }
