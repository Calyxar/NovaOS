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
#include "../shell/shell.h"
#include "../shell/splash.h"

struct MultibootInfo {
    uint32_t flags;
    uint32_t mem_lower;
    uint32_t mem_upper;
    uint32_t boot_device;
    uint32_t cmdline;
    uint32_t mods_count;
    uint32_t mods_addr;
};

extern "C" void kernel_main(MultibootInfo* mbi, uint32_t magic) {
    (void)mbi; (void)magic;

    Framebuffer::init();
    Framebuffer::clear(0x0A0A0F);
    Framebuffer::print("NovaOS v0.1.0 starting...\n", 0xE8E8F0);

    GDT::init();
    Framebuffer::print("[OK] GDT\n", 0x00C2A8);

    IDT::init();
    Framebuffer::print("[OK] IDT\n", 0x00C2A8);

    PMM::init(mbi);
    Framebuffer::print("[OK] Physical memory manager\n", 0x00C2A8);

    VMM::init();
    Framebuffer::print("[OK] Virtual memory manager\n", 0x00C2A8);

    PIT::init(1000);
    Framebuffer::print("[OK] Timer PIT @ 1000Hz\n", 0x00C2A8);

    Keyboard::init();
    Framebuffer::print("[OK] Keyboard\n", 0x00C2A8);

    VFS::init();
    Framebuffer::print("[OK] Virtual filesystem\n", 0x00C2A8);

    IPC::init();
    Framebuffer::print("[OK] IPC subsystem\n", 0x00C2A8);

    Syscall::init();
    Framebuffer::print("[OK] Syscall table\n", 0x00C2A8);

    Framebuffer::print("[OK] Launching Nova Shell\n", 0x6C63FF);

    Splash::show();
    Shell::init();
    Shell::run(); // Does not return

    for(;;) asm volatile("hlt");
}
