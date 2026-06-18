#include "idt.h"
#include "../../drivers/keyboard/keyboard.h"
#include "../../drivers/timer/pit.h"

static IDTEntry   idt[256];
static IDTPointer idt_ptr;

static void outb(uint16_t port, uint8_t val) {
    asm volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

void IDT::set_gate(uint8_t num, uint32_t base, uint16_t sel, uint8_t flags) {
    idt[num].base_low  = base & 0xFFFF;
    idt[num].base_high = (base >> 16) & 0xFFFF;
    idt[num].selector  = sel;
    idt[num].zero      = 0;
    idt[num].flags     = flags;
}

extern "C" void isr_keyboard();
extern "C" void isr_timer();

extern "C" void keyboard_handler() {
    Keyboard::handle_irq();
    outb(0x20, 0x20);
}

extern "C" void timer_handler() {
    PIT::handle_irq();
    outb(0x20, 0x20);
}

void IDT::init() {
    idt_ptr.limit = sizeof(IDTEntry) * 256 - 1;
    idt_ptr.base  = (uint32_t)&idt;

    // Remap PIC: IRQ0-7 → INT 0x20-0x27
    outb(0x20, 0x11); outb(0xA0, 0x11);
    outb(0x21, 0x20); outb(0xA1, 0x28);
    outb(0x21, 0x04); outb(0xA1, 0x02);
    outb(0x21, 0x01); outb(0xA1, 0x01);
    outb(0x21, 0xFC); outb(0xA1, 0xFF); // Unmask IRQ0 (timer) + IRQ1 (keyboard)

    set_gate(0x20, (uint32_t)isr_timer,    0x08, 0x8E); // IRQ0 — timer
    set_gate(0x21, (uint32_t)isr_keyboard, 0x08, 0x8E); // IRQ1 — keyboard

    asm volatile("lidt %0" : : "m"(idt_ptr));
    asm volatile("sti");
}
