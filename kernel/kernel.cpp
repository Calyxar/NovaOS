#include "arch/x86_64/gdt.h"
#include "arch/x86_64/idt.h"
#include "mm/pmm.h"
#include "mm/vmm.h"
#include "drivers/video/framebuffer.h"
#include "drivers/keyboard/keyboard.h"
#include "drivers/timer/pit.h"
#include "fs/vfs.h"
#include "fs/ramfs.h"
#include "ipc/ipc.h"
#include "syscall/syscall.h"
#include "panic.h"
#include "../shell/shell.h"
#include "../shell/splash.h"
#include "drivers/mouse/mouse.h"

// Serial port debug output
static void serial_init() {
    asm volatile("outb %0, %1" : : "a"((uint8_t)0x00), "Nd"((uint16_t)0x3F9));
    asm volatile("outb %0, %1" : : "a"((uint8_t)0x80), "Nd"((uint16_t)0x3FB));
    asm volatile("outb %0, %1" : : "a"((uint8_t)0x03), "Nd"((uint16_t)0x3F8));
    asm volatile("outb %0, %1" : : "a"((uint8_t)0x00), "Nd"((uint16_t)0x3F9));
    asm volatile("outb %0, %1" : : "a"((uint8_t)0x03), "Nd"((uint16_t)0x3FB));
    asm volatile("outb %0, %1" : : "a"((uint8_t)0xC7), "Nd"((uint16_t)0x3FA));
    asm volatile("outb %0, %1" : : "a"((uint8_t)0x0B), "Nd"((uint16_t)0x3FC));
}

static void serial_putc(char c) {
    uint8_t lsr;
    do {
        asm volatile("inb %1, %0" : "=a"(lsr) : "Nd"((uint16_t)0x3FD));
    } while (!(lsr & 0x20));
    asm volatile("outb %0, %1" : : "a"((uint8_t)c), "Nd"((uint16_t)0x3F8));
}

static void serial_print(const char* s) {
    while (*s) { if (*s == '\n') serial_putc('\r'); serial_putc(*s++); }
}

static void serial_hex(uint32_t v) {
    const char* h = "0123456789ABCDEF";
    serial_print("0x");
    for (int i = 28; i >= 0; i -= 4) serial_putc(h[(v >> i) & 0xF]);
}

struct MultibootInfo {
    uint32_t flags;
    uint32_t mem_lower;
    uint32_t mem_upper;
    uint32_t boot_device;
    uint32_t cmdline;
    uint32_t mods_count;
    uint32_t mods_addr;
    uint32_t syms[4];
    uint32_t mmap_length;
    uint32_t mmap_addr;
    uint32_t drives_length;
    uint32_t drives_addr;
    uint32_t config_table;
    uint32_t boot_loader_name;
    uint32_t apm_table;
    uint32_t vbe_control_info;
    uint32_t vbe_mode_info;
    uint16_t vbe_mode;
    uint16_t vbe_interface_seg;
    uint16_t vbe_interface_off;
    uint16_t vbe_interface_len;
    uint64_t framebuffer_addr;
    uint32_t framebuffer_pitch;
    uint32_t framebuffer_width;
    uint32_t framebuffer_height;
    uint8_t  framebuffer_bpp;
    uint8_t  framebuffer_type;
};

extern "C" void kernel_main(MultibootInfo* mbi, uint32_t magic) {
    (void)magic;
    serial_init();
    serial_print("NovaOS kernel starting\n");

    serial_print("MBI flags: "); serial_hex(mbi ? mbi->flags : 0); serial_print("\n");

    bool got_vesa = false;

    if (mbi && (mbi->flags & (1 << 12))) {
        serial_print("FB addr: "); serial_hex((uint32_t)mbi->framebuffer_addr); serial_print("\n");
        serial_print("FB size: "); serial_hex(mbi->framebuffer_width);
        serial_print("x"); serial_hex(mbi->framebuffer_height); serial_print("\n");
        serial_print("FB bpp: "); serial_hex(mbi->framebuffer_bpp); serial_print("\n");
        serial_print("FB pitch: "); serial_hex(mbi->framebuffer_pitch); serial_print("\n");

        if (mbi->framebuffer_addr && mbi->framebuffer_width > 0 && mbi->framebuffer_bpp == 32) {
            serial_print("Initializing VESA...\n");
            Framebuffer::init_vesa(
                (uint32_t*)(uint32_t)mbi->framebuffer_addr,
                mbi->framebuffer_width,
                mbi->framebuffer_height,
                mbi->framebuffer_pitch,
                mbi->framebuffer_bpp
            );
            serial_print("VESA init done\n");
            got_vesa = true;
        }
    } else {
        serial_print("No VESA framebuffer from multiboot\n");
    }

    serial_print("Init GDT\n");
    GDT::init();
    serial_print("Init IDT\n");
    IDT::init();
    serial_print("Init PMM\n");
    PMM::init(mbi);
    serial_print("Init VMM\n");
    VMM::init();
    serial_print("Init PIT\n");
    PIT::init(1000);
    serial_print("Init keyboard\n");
    Keyboard::init();
    Mouse::init();
    serial_print("Init VFS\n");
    VFS::init();
    serial_print("Init IPC\n");
    IPC::init();
    serial_print("Init syscall\n");
    Syscall::init();

    if (got_vesa) {
        serial_print("Clearing screen\n");
        Framebuffer::clear(0x050520);
        serial_print("Drawing text\n");
        Framebuffer::print_at("NovaOS v0.1.0 - VESA mode", 8, 8, 0x00E5FF);
        serial_print("Showing splash\n");
        Splash::show();
    } else {
        serial_print("Falling back to VGA text\n");
        Framebuffer::init();
        Framebuffer::clear(0);
        Framebuffer::print("NovaOS v0.1.0 - VGA text mode\n", 0x00E5FF);
        Framebuffer::print("VESA not available\n", 0xFFFF00);
        Framebuffer::print("Press any key...\n", 0xFFFFFF);
        Keyboard::getchar();
    }

    serial_print("Starting shell\n");
    Shell::init();
    Shell::run();

    for(;;) asm volatile("hlt");
}
