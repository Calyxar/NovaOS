#include "arch/x86_64/gdt.h"
#include "arch/x86_64/idt.h"

#include "mm/pmm.h"
#include "mm/vmm.h"

#include "drivers/video/framebuffer.h"
#include "drivers/keyboard/keyboard.h"
#include "drivers/timer/pit.h"
#include "drivers/mouse/mouse.h"
#include "drivers/disk/ata.h"
#include "drivers/video/font_renderer.h"

#include "user/user.h"
#include "user/session.h"

#include "fs/vfs.h"
#include "fs/novafs_vfs.h"
#include "fs/ramfs.h"
#include "fs/novafs_disk.h"
#include "fs/novafs_setup.h"

#include "ipc/ipc.h"
#include "syscall/syscall.h"

#include "panic.h"

#include "../shell/splash.h"
#include "../ui/shell/desktop.h"
#include "../ui/pages/login.h"
#include "../ui/pages/setup.h"

// ----------------------------------------------------
// Serial debug output
// ----------------------------------------------------

static void serial_init() {
    asm volatile(
        "outb %0, %1"
        :
        : "a"((uint8_t)0x00),
          "Nd"((uint16_t)0x3F9)
    );

    asm volatile(
        "outb %0, %1"
        :
        : "a"((uint8_t)0x80),
          "Nd"((uint16_t)0x3FB)
    );

    asm volatile(
        "outb %0, %1"
        :
        : "a"((uint8_t)0x03),
          "Nd"((uint16_t)0x3F8)
    );

    asm volatile(
        "outb %0, %1"
        :
        : "a"((uint8_t)0x00),
          "Nd"((uint16_t)0x3F9)
    );

    asm volatile(
        "outb %0, %1"
        :
        : "a"((uint8_t)0x03),
          "Nd"((uint16_t)0x3FB)
    );

    asm volatile(
        "outb %0, %1"
        :
        : "a"((uint8_t)0xC7),
          "Nd"((uint16_t)0x3FA)
    );

    asm volatile(
        "outb %0, %1"
        :
        : "a"((uint8_t)0x0B),
          "Nd"((uint16_t)0x3FC)
    );
}

static void serial_putc(char c) {
    uint8_t lsr;

    do {
        asm volatile(
            "inb %1, %0"
            : "=a"(lsr)
            : "Nd"((uint16_t)0x3FD)
        );
    } while (!(lsr & 0x20));

    asm volatile(
        "outb %0, %1"
        :
        : "a"((uint8_t)c),
          "Nd"((uint16_t)0x3F8)
    );
}

static void serial_print(const char* s) {
    while (*s) {
        if (*s == '\n') {
            serial_putc('\r');
        }

        serial_putc(*s++);
    }
}

static void serial_hex(uint32_t value) {
    const char* hex = "0123456789ABCDEF";

    serial_print("0x");

    for (int i = 28; i >= 0; i -= 4) {
        serial_putc(hex[(value >> i) & 0xF]);
    }
}

// ----------------------------------------------------
// Multiboot information
// ----------------------------------------------------

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

    uint8_t framebuffer_bpp;
    uint8_t framebuffer_type;
};

// ----------------------------------------------------
// Kernel entry point
// ----------------------------------------------------

extern "C" void kernel_main(
    MultibootInfo* mbi,
    uint32_t magic
) {
    (void)magic;

    // ------------------------------------------------
    // Serial
    // ------------------------------------------------

    serial_init();

    serial_print("NovaOS kernel starting\n");

    serial_print("MBI flags: ");
    serial_hex(mbi ? mbi->flags : 0);
    serial_print("\n");

    // ------------------------------------------------
    // Framebuffer detection
    // ------------------------------------------------

    bool got_vesa = false;

    if (mbi && (mbi->flags & (1 << 12))) {

        serial_print("FB addr: ");
        serial_hex((uint32_t)mbi->framebuffer_addr);
        serial_print("\n");

        serial_print("FB size: ");
        serial_hex(mbi->framebuffer_width);
        serial_print("x");
        serial_hex(mbi->framebuffer_height);
        serial_print("\n");

        serial_print("FB bpp: ");
        serial_hex(mbi->framebuffer_bpp);
        serial_print("\n");

        serial_print("FB pitch: ");
        serial_hex(mbi->framebuffer_pitch);
        serial_print("\n");

        if (
            mbi->framebuffer_addr &&
            mbi->framebuffer_width > 0 &&
            mbi->framebuffer_height > 0 &&
            mbi->framebuffer_bpp == 32
        ) {
            serial_print("Initializing VESA framebuffer...\n");

            Framebuffer::init_vesa(
    (uint32_t*)(uint32_t)mbi->framebuffer_addr,
    mbi->framebuffer_width,
    mbi->framebuffer_height,
    mbi->framebuffer_pitch,
    mbi->framebuffer_bpp
);

serial_print("VESA framebuffer ready\n");

serial_print("Initializing modern font renderer\n");
FontRenderer::init();

got_vesa = true;
        }

    } else {
        serial_print(
            "No VESA framebuffer from multiboot\n"
        );
    }

    // ------------------------------------------------
    // CPU / architecture
    // ------------------------------------------------

    serial_print("Init GDT\n");
    GDT::init();

    serial_print("Init IDT\n");
    IDT::init();

    // ------------------------------------------------
    // Memory
    // ------------------------------------------------

    serial_print("Init PMM\n");
    PMM::init(mbi);

    serial_print("Init VMM\n");
    VMM::init();

    // ------------------------------------------------
    // Timer
    // ------------------------------------------------

    serial_print("Init PIT\n");
    PIT::init(1000);

    // ------------------------------------------------
    // Input
    // ------------------------------------------------

    serial_print("Init keyboard\n");
    Keyboard::init();

    serial_print("Init mouse\n");
    Mouse::init();

    if (got_vesa) {
        Mouse::set_bounds(
            (int32_t)mbi->framebuffer_width,
            (int32_t)mbi->framebuffer_height
        );
    }

    // ------------------------------------------------
    // Storage
    // ------------------------------------------------

    serial_print("Init ATA\n");
    ATA::init();

    serial_print("Init NovaFS disk\n");
    NovaFSDisk::init();

    // ------------------------------------------------
    // Filesystem
    // ------------------------------------------------

    serial_print("Init VFS\n");
    VFS::init();

    serial_print("Mount NovaFS at /\n");

    if (NovaFSVFS::mount_root()) {
        serial_print("NovaFS mounted at /\n");
    } else {
        serial_print("ERROR: NovaFS mount failed\n");
    }

    serial_print("Creating NovaFS default layout\n");
    NovaFSSetup::create_default_layout();

NovaFSDisk::create_directory(
    NOVAFS_ROOT_PARENT,
    "System"
);

NovaFSDisk::create_directory(
    NOVAFS_ROOT_PARENT,
    "Users"
);

NovaFSDisk::create_directory(
    NOVAFS_ROOT_PARENT,
    "Apps"
);

NovaFSDisk::create_directory(
    NOVAFS_ROOT_PARENT,
    "Documents"
);

NovaFSDisk::create_directory(
    NOVAFS_ROOT_PARENT,
    "Downloads"
);

NovaFSDisk::create_directory(
    NOVAFS_ROOT_PARENT,
    "Temp"
);

int systemDir =
    NovaFSDisk::find_directory(
        NOVAFS_ROOT_PARENT,
        "System"
    );

int usersDir =
    NovaFSDisk::find_directory(
        NOVAFS_ROOT_PARENT,
        "Users"
    );

int documentsDir =
    NovaFSDisk::find_directory(
        NOVAFS_ROOT_PARENT,
        "Documents"
    );

    if (systemDir >= 0) {
    NovaFSDisk::create_directory(
        (uint32_t)systemDir,
        "Drivers"
    );

    NovaFSDisk::create_directory(
        (uint32_t)systemDir,
        "Config"
    );

    NovaFSDisk::create_directory(
        (uint32_t)systemDir,
        "Logs"
    );
}

if (documentsDir >= 0) {
    NovaFSDisk::create_directory(
        (uint32_t)documentsDir,
        "Projects"
    );

    NovaFSDisk::create_directory(
        (uint32_t)documentsDir,
        "Notes"
    );
}

    // ------------------------------------------------
// NovaOS User & Session subsystem
// ------------------------------------------------

serial_print("Initializing user subsystem\n");
User::init();

serial_print("Initializing session subsystem\n");
Session::init();

    // ------------------------------------------------
    // IPC
    // ------------------------------------------------

    serial_print("Init IPC\n");
    IPC::init();

    // ------------------------------------------------
    // Syscalls
    // ------------------------------------------------

    serial_print("Init syscall layer\n");
    Syscall::init();

    // ------------------------------------------------
    // Display setup
    // ------------------------------------------------

    if (got_vesa) {

        serial_print("Clearing VESA framebuffer\n");

        Framebuffer::clear(
            0x050520
        );

        serial_print(
            "Drawing NovaOS startup text\n"
        );

        Framebuffer::print_at(
            "NovaOS v0.1.0 - VESA mode",
            8,
            8,
            0x00E5FF
        );

        serial_print("Showing splash\n");

        Splash::show();

    } else {

        serial_print(
            "Falling back to VGA text mode\n"
        );

        Framebuffer::init();

        Framebuffer::clear(
            0x000000
        );

        Framebuffer::print(
            "NovaOS v0.1.0 - VGA text mode\n",
            0x00E5FF
        );

        Framebuffer::print(
            "VESA framebuffer unavailable\n",
            0xFFFF00
        );

        Framebuffer::print(
            "Press any key...\n",
            0xFFFFFF
        );

        Keyboard::getchar();
    }

// ------------------------------------------------
// NovaOS Account Setup / Login
// ------------------------------------------------

if (User::count() == 0) {

    // --------------------------------------------
    // First boot — no user accounts exist
    // --------------------------------------------

    serial_print(
        "No users found - starting first boot setup\n"
    );

    SetupPage::init();

    if (!SetupPage::run()) {
        serial_print(
            "ERROR: Setup exited unexpectedly\n"
        );

        for (;;) {
            asm volatile("hlt");
        }
    }

    serial_print(
        "First user account created and logged in\n"
    );

} else {

    // --------------------------------------------
    // Returning user — show login screen
    // --------------------------------------------

    serial_print("Starting login screen\n");

    LoginPage::init();

    if (!LoginPage::run()) {
        serial_print(
            "ERROR: Login screen exited unexpectedly\n"
        );

        for (;;) {
            asm volatile("hlt");
        }
    }

    serial_print("Login successful\n");
}


// ------------------------------------------------
// NovaOS Desktop
// ------------------------------------------------

serial_print("Starting desktop\n");

Desktop::init();
Desktop::run();


// We should never normally reach this point.
serial_print("ERROR: Desktop exited unexpectedly\n");

for (;;) {
    asm volatile("hlt");
}

// ------------------------------------------------
// VGA fallback idle loop
// ------------------------------------------------

serial_print("Desktop unavailable without VESA\n");

for (;;) {
    asm volatile("hlt");
}
}