#include "idt.h"
#include "../../drivers/keyboard/keyboard.h"
#include "../../drivers/timer/pit.h"
#include "../../drivers/mouse/mouse.h"

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
extern "C" void isr_mouse();

extern "C" void keyboard_handler() {
    Keyboard::handle_irq();
    outb(0x20, 0x20);
}

extern "C" void timer_handler() {
    PIT::handle_irq();
    outb(0x20, 0x20);
}

extern "C" void mouse_handler() {
    Mouse::handle_irq();
    outb(0xA0, 0x20); // ACK slave PIC
    outb(0x20, 0x20); // ACK master PIC
}

void IDT::init() {
    idt_ptr.limit = sizeof(IDTEntry) * 256 - 1;
    idt_ptr.base  = (uint32_t)&idt;

    // Remap PIC
    outb(0x20, 0x11); outb(0xA0, 0x11);
    outb(0x21, 0x20); outb(0xA1, 0x28);
    outb(0x21, 0x04); outb(0xA1, 0x02);
    outb(0x21, 0x01); outb(0xA1, 0x01);
    outb(0x21, 0xF8); outb(0xA1, 0xEF); // Unmask IRQ0,1,2 + IRQ12

    set_gate(0x20, (uint32_t)isr_timer,    0x08, 0x8E);
    set_gate(0x21, (uint32_t)isr_keyboard, 0x08, 0x8E);
    set_gate(0x2C, (uint32_t)isr_mouse,    0x08, 0x8E); // IRQ12

    asm volatile("lidt %0" : : "m"(idt_ptr));
    asm volatile("sti");
}
