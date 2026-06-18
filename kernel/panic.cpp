#include "panic.h"
#include "drivers/video/framebuffer.h"
#include <stdint.h>

static void p(const char* s, uint32_t c) { Framebuffer::print(s, c); }

static void print_num(uint32_t n) {
    if (n == 0) { p("0", 0xFFFFFF); return; }
    char buf[12]; int i = 0;
    while (n > 0) { buf[i++] = '0' + (n % 10); n /= 10; }
    char out[12];
    for (int j = 0; j < i; j++) out[j] = buf[i-1-j];
    out[i] = '\0';
    p(out, 0xFFFFFF);
}

extern "C" void kernel_panic(const char* message,
                              const char* file,
                              uint32_t    line) {
    // Disable interrupts immediately
    asm volatile("cli");

    // Clear screen to dark red
    uint16_t* vga = (uint16_t*)0xB8000;
    for (int i = 0; i < 80 * 25; i++)
        vga[i] = (uint16_t)' ' | (0x4F << 8); // White on red

    // Reset framebuffer cursor
    Framebuffer::clear(0xFF0000);

    // Top border
    p("\n", 0xFFFFFF);
    p("  !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n", 0xFFFF00);
    p("  !!                                                     !!\n", 0xFFFF00);
    p("  !!              ", 0xFFFF00);
    p("NOVAOS KERNEL PANIC", 0xFFFFFF);
    p("                !!\n", 0xFFFF00);
    p("  !!                                                     !!\n", 0xFFFF00);
    p("  !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n\n", 0xFFFF00);

    // Error message
    p("  REASON:  ", 0xFF6B6B);
    p(message, 0xFFFFFF);
    p("\n\n", 0xFFFFFF);

    // File and line
    p("  FILE:    ", 0xFF6B6B);
    p(file, 0xFFFFFF);
    p("\n", 0xFFFFFF);

    p("  LINE:    ", 0xFF6B6B);
    print_num(line);
    p("\n\n", 0xFFFFFF);

    // Divider
    p("  -----------------------------------------------------------\n\n", 0xFF6B6B);

    // System state
    p("  SYSTEM STATE\n", 0xFFFF00);
    p("  CPU:      ", 0x888888); p("x86 i686 — halted\n", 0xFFFFFF);
    p("  MODE:     ", 0x888888); p("32-bit Protected Mode\n", 0xFFFFFF);
    p("  INTERRUPTS:", 0x888888); p("disabled\n\n", 0xFFFFFF);

    // Divider
    p("  -----------------------------------------------------------\n\n", 0xFF6B6B);

    // Recovery info
    p("  The system has been halted to prevent damage.\n", 0x888888);
    p("  Please restart your computer.\n\n", 0x888888);

    p("  NovaOS v0.1.0  |  ", 0x444444);
    p("github.com/A-Bak-tech/NovaOS\n\n", 0x444444);

    // Bottom border
    p("  !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n", 0xFFFF00);
    p("  !!                  SYSTEM HALTED                      !!\n", 0xFFFF00);
    p("  !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n", 0xFFFF00);

    // Halt the CPU forever
    for(;;) asm volatile("hlt");
}
