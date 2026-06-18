#include "splash.h"
#include "../kernel/drivers/video/framebuffer.h"
#include "../kernel/drivers/keyboard/keyboard.h"

#define CLR_WHITE  0xFFFFFF
#define CLR_GREEN  0x00C2A8
#define CLR_BLUE   0x6C63FF
#define CLR_CYAN   0x00FFFF
#define CLR_YELLOW 0xFFFF00
#define CLR_GRAY   0x888888
#define CLR_RED    0xFF6B6B
#define CLR_DARK   0x444444

static void p(const char* s, uint32_t c) { Framebuffer::print(s, c); }

static void wait(uint32_t n) {
    volatile uint32_t i = n * 8000;
    while (i--);
}

static void loading_bar() {
    p("         [", CLR_BLUE);
    const char* steps[] = {
        "Kernel     ",
        "Memory     ",
        "Drivers    ",
        "Filesystem ",
        "Shell      "
    };
    uint32_t colors[] = { CLR_RED, CLR_YELLOW, CLR_GREEN, CLR_CYAN, CLR_BLUE };

    for (int i = 0; i < 5; i++) {
        wait(60000);
        p("####", colors[i]);
    }
    p("]  ", CLR_BLUE);
    p("READY\n", CLR_GREEN);
}

void Splash::show() {
    Framebuffer::clear(0);

    p("\n\n", CLR_WHITE);

    // Top border
    p("  ", CLR_WHITE);
    p("*---------------------------------------------------------*\n", CLR_BLUE);
    p("  ", CLR_WHITE);
    p("|                                                         |\n", CLR_BLUE);

    // Logo line 1
    p("  ", CLR_WHITE);
    p("|   ", CLR_BLUE);
    p(" _   _                   ___  ____  ", CLR_CYAN);
    p("                |\n", CLR_BLUE);

    // Logo line 2
    p("  ", CLR_WHITE);
    p("|   ", CLR_BLUE);
    p("| \\ | | _____   ____ _  / _ \\/ ___| ", CLR_CYAN);
    p("               |\n", CLR_BLUE);

    // Logo line 3
    p("  ", CLR_WHITE);
    p("|   ", CLR_BLUE);
    p("|  \\| |/ _ \\ \\ / / _` || | | \\___ \\ ", CLR_GREEN);
    p("               |\n", CLR_BLUE);

    // Logo line 4
    p("  ", CLR_WHITE);
    p("|   ", CLR_BLUE);
    p("| |\\  | (_) \\ V / (_| || |_| |___) |", CLR_GREEN);
    p("               |\n", CLR_BLUE);

    // Logo line 5
    p("  ", CLR_WHITE);
    p("|   ", CLR_BLUE);
    p("|_| \\_|\\___/ \\_/ \\__,_| \\___/|____/ ", CLR_YELLOW);
    p("               |\n", CLR_BLUE);

    p("  ", CLR_WHITE);
    p("|                                                         |\n", CLR_BLUE);

    // Tagline
    p("  ", CLR_WHITE);
    p("|        ", CLR_BLUE);
    p("Next-Generation OS for Future Computers", CLR_WHITE);
    p("        |\n", CLR_BLUE);

    // Version & build info
    p("  ", CLR_WHITE);
    p("|     ", CLR_BLUE);
    p("Version: ", CLR_GRAY);
    p("v0.1.0", CLR_YELLOW);
    p("   Kernel: ", CLR_GRAY);
    p("C/C++/ASM", CLR_GREEN);
    p("   GPU: ", CLR_GRAY);
    p("NVIDIA", CLR_GREEN);
    p("        |\n", CLR_BLUE);

    p("  ", CLR_WHITE);
    p("|                                                         |\n", CLR_BLUE);
    p("  ", CLR_WHITE);
    p("*---------------------------------------------------------*\n\n", CLR_BLUE);

    // Loading bar
    loading_bar();

    p("\n         ", CLR_WHITE);
    p("Press any key to enter NovaOS...", CLR_GRAY);
    p("\n", CLR_WHITE);

    Keyboard::getchar();
    Framebuffer::clear(0);
}
