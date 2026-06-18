#include "shell.h"
#include "../kernel/drivers/keyboard/keyboard.h"
#include "../kernel/drivers/video/framebuffer.h"
#include "../kernel/fs/ramfs.h"
#include "../kernel/proc/scheduler.h"
#include "../kernel/panic.h"
#include "../kernel/drivers/mouse/mouse.h"

static char input_buf[256];
static int  input_len = 0;

// Cosmic Pulse palette
#define CLR_BG      0x050520
#define CLR_WHITE   0xFFFFFF
#define CLR_CYAN    0x00E5FF
#define CLR_VIOLET  0x7B2FF7
#define CLR_PINK    0xFF0080
#define CLR_DIM     0x6666AA
#define CLR_GRAY    0x444466
#define CLR_GREEN   0x00C2A8
#define CLR_YELLOW  0xFFD700
#define CLR_RED     0xFF4444

// Layout constants
#define SIDEBAR_W   180
#define TOPBAR_H    36
#define PADDING     10

static bool streq(const char* a, const char* b) {
    while (*a && *b) { if (*a != *b) return false; a++; b++; }
    return *a == *b;
}

static bool startswith(const char* str, const char* prefix) {
    while (*prefix) { if (*str++ != *prefix++) return false; }
    return true;
}

static void p(const char* s, uint32_t c) { Framebuffer::print(s, c); }

static void print_num(uint32_t n) {
    if (n == 0) { p("0", CLR_WHITE); return; }
    char buf[12]; int i = 0;
    while (n > 0) { buf[i++] = '0' + (n % 10); n /= 10; }
    char out[12];
    for (int j = 0; j < i; j++) out[j] = buf[i-1-j];
    out[i] = '\0';
    p(out, CLR_WHITE);
}

extern uint32_t pit_ticks;

// Draw the topbar
static void draw_topbar() {
    Framebuffer::Info& fb = Framebuffer::get_info();
    uint32_t W = fb.width;

    // Topbar background
    Framebuffer::draw_rect(0, 0, W, TOPBAR_H, 0x0A0820);
    // Bottom border line — Cosmic Pulse gradient
    for (uint32_t x = 0; x < W; x++) {
        uint32_t t = x * 255 / W;
        uint32_t r = (0 * (255-t) + 123 * t) / 255;
        uint32_t g = (229 * (255-t) + 47 * t) / 255;
        uint32_t b = (255 * (255-t) + 247 * t) / 255;
        Framebuffer::put_pixel(x, TOPBAR_H-1, (r<<16)|(g<<8)|b);
    }

    // Nova Wave mini logo
    Framebuffer::draw_circle(16, 18, 5, CLR_VIOLET);
    Framebuffer::draw_circle(16, 18, 3, CLR_CYAN);
    Framebuffer::draw_circle(16, 18, 1, CLR_WHITE);

    // NovaOS wordmark
    Framebuffer::print_at("NovaOS", 28, 14, CLR_CYAN);

    // Nav items
    Framebuffer::print_at("Home", 110, 14, CLR_WHITE);
    Framebuffer::print_at("Files", 155, 14, CLR_DIM);
    Framebuffer::print_at("Apps", 205, 14, CLR_DIM);

    // Time / uptime on right
    uint32_t secs = pit_ticks / 1000u;
    char uptime[32] = "uptime: ";
    // simple num concat
    char ns[12]; int ni = 0;
    uint32_t tmp = secs;
    if (tmp == 0) { ns[ni++] = '0'; }
    while (tmp > 0) { ns[ni++] = '0' + (tmp % 10); tmp /= 10; }
    char ns2[12];
    for (int i = 0; i < ni; i++) ns2[i] = ns[ni-1-i];
    ns2[ni] = 's'; ns2[ni+1] = '\0';
    // copy to uptime string
    int ui = 8;
    for (int i = 0; ns2[i]; i++) uptime[ui++] = ns2[i];
    uptime[ui] = '\0';
    Framebuffer::print_at(uptime, (int)W - 120, 14, CLR_DIM);
}

// Draw the sidebar
static void draw_sidebar() {
    Framebuffer::Info& fb = Framebuffer::get_info();
    uint32_t H = fb.height;

    // Sidebar background
    Framebuffer::draw_rect(0, TOPBAR_H, SIDEBAR_W, H - TOPBAR_H, 0x08031E);
    // Right border
    Framebuffer::draw_rect(SIDEBAR_W-1, TOPBAR_H, 1, H - TOPBAR_H, 0x1A0855);

    int y = TOPBAR_H + 16;

    // Section label
    Framebuffer::print_at("PINNED", 12, y, CLR_GRAY);
    y += 16;

    // Nav items
    struct NavItem { const char* label; const char* sub; uint32_t color; };
    NavItem items[] = {
        { "Dashboard",    "Home screen",    CLR_CYAN   },
        { "Nova Browser", "Browse the web", CLR_CYAN   },
        { "Nova Docs",    "Office suite",   CLR_PINK   },
        { "Game Mode",    "Launch games",   CLR_PINK   },
        { "Terminal",     "Nova Shell",     CLR_VIOLET },
    };

    for (int i = 0; i < 5; i++) {
        // Active highlight for Terminal
        if (i == 4) {
            Framebuffer::draw_rect(4, y-2, SIDEBAR_W-8, 22, 0x1A0844);
            Framebuffer::draw_rect(4, y-2, 2, 22, CLR_VIOLET);
        }
        // Color dot
        Framebuffer::draw_circle(18, y+7, 4, items[i].color);
        Framebuffer::print_at(items[i].label, 30, y,   CLR_WHITE);
        Framebuffer::print_at(items[i].sub,   30, y+10, CLR_GRAY);
        y += 28;
    }

    // Divider
    Framebuffer::draw_rect(12, y, SIDEBAR_W-24, 1, 0x1A0855);
    y += 10;
    Framebuffer::print_at("SYSTEM", 12, y, CLR_GRAY);
    y += 16;

    NavItem sys[] = {
        { "Nova Files",  "NovaFS",     CLR_DIM },
        { "Nova Store",  "Apps",       CLR_DIM },
        { "Settings",    "CPU/GPU/RAM", CLR_DIM },
    };
    for (int i = 0; i < 3; i++) {
        Framebuffer::draw_circle(18, y+7, 4, sys[i].color);
        Framebuffer::print_at(sys[i].label, 30, y,    CLR_DIM);
        Framebuffer::print_at(sys[i].sub,   30, y+10, CLR_GRAY);
        y += 28;
    }
}

// Main content area cursor
static int content_x = SIDEBAR_W + PADDING;
static int content_y = TOPBAR_H + PADDING;

static void content_print(const char* str, uint32_t color) {
    Framebuffer::Info& fb = Framebuffer::get_info();
    int px = content_x;
    for (int i = 0; str[i]; i++) {
        char c = str[i];
        if (c == '\n') {
            content_x = SIDEBAR_W + PADDING;
            content_y += 10;
            if (content_y > (int)fb.height - 20) {
                // Scroll content area
                content_y -= 10;
            }
            px = content_x;
            continue;
        }
        if (c == '\b') {
            if (px > SIDEBAR_W + PADDING) px -= 8;
            Framebuffer::draw_rect(px, content_y, 8, 10, CLR_BG);
            content_x = px;
            continue;
        }
        Framebuffer::draw_char(c, px, content_y, color);
        px += 8;
        if (px + 8 > (int)fb.width - PADDING) {
            px = SIDEBAR_W + PADDING;
            content_y += 10;
        }
    }
    content_x = px;
}

static void content_println(const char* str, uint32_t color) {
    content_print(str, color);
    content_print("\n", color);
}

static void draw_prompt() {
    content_print("> ", CLR_CYAN);
}

static uint32_t rng_state = 12345;
static uint32_t rng_next() {
    rng_state ^= rng_state << 13;
    rng_state ^= rng_state >> 17;
    rng_state ^= rng_state << 5;
    return rng_state;
}

static void draw_wallpaper() {
    Framebuffer::Info& fb = Framebuffer::get_info();
    uint32_t W = fb.width;
    uint32_t H = fb.height;

    // Deep space base
    Framebuffer::clear(0x010610);

    // Nebula glows — large soft radial gradients
    struct Nebula { int x, y, r; uint32_t r_col, g_col, b_col; uint32_t alpha; };
    Nebula nebulae[] = {
        { (int)(W*7/10), (int)(H*2/10), (int)(H*6/10), 80,  0,   220, 20 },
        { (int)(W*1/10), (int)(H*7/10), (int)(H*5/10), 0,   180, 255, 16 },
        { (int)(W*9/10), (int)(H*8/10), (int)(H*5/10), 200, 0,   120, 14 },
        { (int)(W*3/10), (int)(H*1/10), (int)(H*4/10), 0,   200, 240, 10 },
    };
    for (int n = 0; n < 4; n++) {
        Nebula& nb = nebulae[n];
        int r2 = nb.r * nb.r;
        for (int dy = -nb.r; dy <= nb.r; dy += 2) {
            for (int dx = -nb.r; dx <= nb.r; dx += 2) {
                int d = dx*dx + dy*dy;
                if (d > r2) continue;
                int px = nb.x + dx, py = nb.y + dy;
                if (px < 0 || py < 0 || px >= (int)W || py >= (int)H) continue;
                // Fade with distance
                // Integer sqrt approximation
                uint32_t dd = (uint32_t)d;
                uint32_t isqrt = nb.r;
                for (uint32_t s = 1; s * s <= dd; s++) isqrt = s;
                uint32_t fade = (uint32_t)(nb.r - isqrt) * nb.alpha / nb.r;
                if (fade == 0) continue;
                // Blend with existing pixel
                uint32_t existing = 0x010610;
                uint32_t nr = ((existing >> 16) & 0xFF) + (nb.r_col * fade / 255);
                uint32_t ng = ((existing >> 8)  & 0xFF) + (nb.g_col * fade / 255);
                uint32_t nbl = (existing & 0xFF)        + (nb.b_col * fade / 255);
                if (nr > 255) nr = 255;
                if (ng > 255) ng = 255;
                if (nbl > 255) nbl = 255;
                uint32_t col = (nr<<16)|(ng<<8)|nbl;
                Framebuffer::put_pixel(px,   py,   col);
                Framebuffer::put_pixel(px+1, py,   col);
                Framebuffer::put_pixel(px,   py+1, col);
                Framebuffer::put_pixel(px+1, py+1, col);
            }
        }
    }

    // Background stars — tiny and faint
    rng_state = 42;
    for (int i = 0; i < 200; i++) {
        int x = rng_next() % W;
        int y = rng_next() % H;
        uint32_t b = 20 + rng_next() % 40;
        Framebuffer::put_pixel(x, y, (b<<16)|(b<<8)|b);
    }

    // Mid stars
    for (int i = 0; i < 80; i++) {
        int x = rng_next() % W;
        int y = rng_next() % H;
        uint32_t b = 60 + rng_next() % 80;
        uint32_t type = rng_next() % 3;
        uint32_t col = type == 0 ? (b<<16)|(b<<8)|b :
                       type == 1 ? ((b/2)<<16)|((b/2)<<8)|b :
                                   (b<<16)|((b/2)<<8)|(b/2);
        Framebuffer::put_pixel(x, y, col);
        if (b > 100) Framebuffer::put_pixel(x+1, y, col);
    }

    // Bright stars with glow
    for (int i = 0; i < 12; i++) {
        int x = 20 + rng_next() % (W-40);
        int y = 20 + rng_next() % (H-40);
        // Glow
        uint32_t gc2 = rng_next() % 3;
        for (int r = 6; r >= 1; r--) {
            uint32_t b2 = 255 / (r+1);
            uint32_t gc3 = gc2 == 0 ? (0|(0<<8)|(b2)) :
                          gc2 == 1 ? (b2|(0<<8)|(b2/2)) :
                                     (0|(b2/2<<8)|(b2));
            for (int dy2 = -r; dy2 <= r; dy2++)
                for (int dx2 = -r; dx2 <= r; dx2++)
                    if (dx2*dx2+dy2*dy2 <= r*r)
                        Framebuffer::put_pixel(x+dx2, y+dy2, gc3);
        }
        // Core
        Framebuffer::put_pixel(x, y, 0xFFFFFF);
    }

    // Aero orbs — soft glowing bubbles
    struct Orb { int x, y, r; uint32_t rc, gc4, bc; };
    Orb orbs[] = {
        { (int)(W*7/100),  (int)(H*16/100), 22, 0,   229, 255 },
        { (int)(W*93/100), (int)(H*32/100), 14, 244, 114, 182 },
        { (int)(W*12/100), (int)(H*84/100), 18, 123, 47,  247 },
        { (int)(W*85/100), (int)(H*88/100), 10, 0,   229, 255 },
    };
    for (int o = 0; o < 4; o++) {
        Orb& orb = orbs[o];
        int maxR = orb.r * 4;
        // Wide outer glow
        for (int dy2 = -maxR; dy2 <= maxR; dy2++) {
            for (int dx2 = -maxR; dx2 <= maxR; dx2++) {
                int d2 = dx2*dx2 + dy2*dy2;
                if (d2 > maxR*maxR) continue;
                int px2 = orb.x+dx2, py2 = orb.y+dy2;
                if (px2<0||py2<0||px2>=(int)W||py2>=(int)H) continue;
                // Distance-based fade — bright core, soft edge
                uint32_t dist2 = 1;
                for (uint32_t s = 1; s*s <= (uint32_t)d2; s++) dist2 = s;
                uint32_t fade2 = 0;
                if (dist2 < (uint32_t)maxR)
                    fade2 = ((uint32_t)maxR - dist2) * 80 / (uint32_t)maxR;
                if (fade2 == 0) continue;
                // Blend additively with existing pixel
                uint32_t nr2 = orb.rc  * fade2 / 255;
                uint32_t ng2 = orb.gc4 * fade2 / 255;
                uint32_t nb2 = orb.bc  * fade2 / 255;
                // Read existing and add
                // (simple additive — just write, looks great on dark bg)
                if (nr2 > 255) nr2 = 255;
                if (ng2 > 255) ng2 = 255;
                if (nb2 > 255) nb2 = 255;
                Framebuffer::put_pixel(px2, py2,
                    ((nr2&0xFF)<<16)|((ng2&0xFF)<<8)|(nb2&0xFF));
            }
        }
        // Bright core
        for (int dy2 = -orb.r/2; dy2 <= orb.r/2; dy2++)
            for (int dx2 = -orb.r/2; dx2 <= orb.r/2; dx2++)
                if (dx2*dx2+dy2*dy2 <= (orb.r/2)*(orb.r/2)) {
                    int px2 = orb.x+dx2, py2 = orb.y+dy2;
                    if (px2<0||py2<0||px2>=(int)W||py2>=(int)H) continue;
                    uint32_t bright = 180;
                    uint32_t nr2 = orb.rc  * bright / 255;
                    uint32_t ng2 = orb.gc4 * bright / 255;
                    uint32_t nb2 = orb.bc  * bright / 255;
                    Framebuffer::put_pixel(px2, py2,
                        ((nr2&0xFF)<<16)|((ng2&0xFF)<<8)|(nb2&0xFF));
                }
    }
}

static void draw_header() {
    Framebuffer::Info& fb = Framebuffer::get_info();
    uint32_t W = fb.width;
    uint32_t H = fb.height;

    // Paint Deep Space wallpaper first
    draw_wallpaper();

    draw_topbar();
    draw_sidebar();

    // Welcome card in content area
    int cx = SIDEBAR_W + PADDING;
    int cy = TOPBAR_H + PADDING;
    int cw = (int)W - SIDEBAR_W - PADDING*2;

    // Card background
    Framebuffer::draw_rect(cx, cy, cw, 50, 0x0D0530);
    // Gradient top border
    for (int x = cx; x < cx+cw; x++) {
        uint32_t t = (x-cx) * 255 / cw;
        uint32_t r = (0*(255-t) + 255*t)/255;
        uint32_t g = (229*(255-t) + 0*t)/255;
        uint32_t bv = (255*(255-t) + 128*t)/255;
        Framebuffer::put_pixel(x, cy, (r<<16)|(g<<8)|bv);
    }

    Framebuffer::print_at("Welcome to NovaOS", cx+10, cy+10, CLR_WHITE);
    Framebuffer::print_at("Productivity  |  Gaming  |  Open Source",
                          cx+10, cy+26, CLR_DIM);

    // Divider below card
    Framebuffer::draw_rect(cx, cy+52, cw, 1, 0x1A0855);

    // Position cursor below card
    content_x = cx;
    content_y = cy + 62;

    content_println("  Nova Shell v0.2.0 — pixel mode", CLR_CYAN);
    content_println("  Type 'help' for commands.", CLR_DIM);
    content_print("\n", CLR_WHITE);
}

static void ls_callback(const char* name, uint32_t size) {
    content_print("  ", CLR_WHITE);
    content_print(name, CLR_CYAN);
    content_print("  (", CLR_GRAY);
    print_num(size);
    content_print(" bytes)\n", CLR_GRAY);
}

static void proc_hello() {
    content_println("[hello] Hello from a NovaOS process!", CLR_GREEN);
    Scheduler::exit(0);
}

static void proc_counter() {
    content_print("[counter] ", CLR_YELLOW);
    for (int i = 1; i <= 10; i++) {
        char s[4]; s[0] = '0' + (i/10); s[1] = '0' + (i%10); s[2] = ' '; s[3] = '\0';
        if (i < 10) { s[0] = '0' + i; s[1] = ' '; s[2] = '\0'; }
        content_print(s, CLR_YELLOW);
    }
    content_println("\n[counter] Done!", CLR_YELLOW);
    Scheduler::exit(0);
}

static void proc_sysinfo() {
    content_println("[sysinfo] NovaOS v0.2.0 — pixel GUI", CLR_CYAN);
    content_println("[sysinfo] Kernel: C/C++/ASM", CLR_CYAN);
    content_println("[sysinfo] Display: VESA 800x600x32", CLR_CYAN);
    Scheduler::exit(0);
}

static void handle_command(const char* cmd) {
    if (streq(cmd, "help")) {
        content_println("\n  Commands:", CLR_CYAN);
        const char* cmds[] = {
            "help","version","about","clear","cpu","mem","uptime",
            "echo","ls","cat","touch","write","rm","ps","run","panic","color"
        };
        for (int i = 0; i < 17; i++) {
            content_print("  ", CLR_WHITE);
            content_println(cmds[i], CLR_GREEN);
        }
        content_print("\n", CLR_WHITE);
    }
    else if (streq(cmd, "version")) {
        content_println("\n  NovaOS v0.2.0", CLR_CYAN);
        content_println("  VESA pixel GUI — built from scratch\n", CLR_DIM);
    }
    else if (streq(cmd, "about")) {
        content_println("\n  NovaOS — Next-Gen OS", CLR_CYAN);
        content_println("  Kernel:   C/C++/ASM", CLR_WHITE);
        content_println("  Display:  VESA 800x600 32bpp", CLR_WHITE);
        content_println("  GPU:      NVIDIA (roadmap)", CLR_GREEN);
        content_println("  Compositor: Nova (roadmap)\n", CLR_GREEN);
    }
    else if (streq(cmd, "clear")) {
        draw_header();
    }
    else if (streq(cmd, "mem")) {
        content_println("\n  Memory", CLR_CYAN);
        content_println("  Kernel: 0x100000", CLR_YELLOW);
        content_println("  FB:     0xFD000000", CLR_YELLOW);
        content_println("  RAM:    ~30MB\n", CLR_YELLOW);
    }
    else if (streq(cmd, "cpu")) {
        content_println("\n  CPU: x86 i686 32-bit", CLR_CYAN);
        content_println("  Mode: Protected Ring 0", CLR_WHITE);
        content_println("  Next: x86-64 + NVIDIA\n", CLR_GREEN);
    }
    else if (streq(cmd, "uptime")) {
        content_print("\n  Uptime: ", CLR_CYAN);
        print_num(pit_ticks / 1000u);
        content_println("s\n", CLR_WHITE);
    }
    else if (startswith(cmd, "echo ")) {
        content_print("\n  ", CLR_WHITE);
        content_println(cmd + 5, CLR_CYAN);
        content_print("\n", CLR_WHITE);
    }
    else if (streq(cmd, "ls")) {
        content_println("\n  NovaFS Files", CLR_CYAN);
        RamFS::list(ls_callback);
        content_print("\n", CLR_WHITE);
    }
    else if (startswith(cmd, "cat ")) {
        RamFile* f = RamFS::find(cmd+4);
        if (f) { content_print("\n  ", CLR_WHITE); content_println(f->data, CLR_WHITE); content_print("\n", CLR_WHITE); }
        else   { content_print("  Not found: ", CLR_RED); content_println(cmd+4, CLR_WHITE); }
    }
    else if (startswith(cmd, "touch ")) {
        if (RamFS::create(cmd+6)) { content_print("  Created: ", CLR_GREEN); content_println(cmd+6, CLR_WHITE); }
        else { content_println("  Error creating file", CLR_RED); }
    }
    else if (startswith(cmd, "write ")) {
        const char* rest = cmd+6;
        int i = 0; while (rest[i] && rest[i] != ' ') i++;
        if (rest[i] == ' ') {
            char fname[32];
            for (int j = 0; j < i && j < 31; j++) fname[j] = rest[j];
            fname[i] = '\0';
            const char* content = rest+i+1;
            uint32_t len = 0; while (content[len]) len++;
            if (RamFS::write(fname, content, len)) {
                content_print("  Written: ", CLR_GREEN); content_println(fname, CLR_WHITE);
            } else { content_println("  File not found", CLR_RED); }
        } else { content_println("  Usage: write <file> <text>", CLR_YELLOW); }
    }
    else if (startswith(cmd, "rm ")) {
        if (RamFS::remove(cmd+3)) { content_print("  Deleted: ", CLR_PINK); content_println(cmd+3, CLR_WHITE); }
        else { content_println("  Not found", CLR_RED); }
    }
    else if (streq(cmd, "ps")) {
        content_println("\n  Processes", CLR_CYAN);
        content_println("  PID 1  nova-shell  RUNNING", CLR_GREEN);
        content_print("\n", CLR_WHITE);
    }
    else if (startswith(cmd, "run ")) {
        const char* prog = cmd+4;
        if (streq(prog, "hello"))   { Scheduler::spawn("hello",   proc_hello);   proc_hello(); }
        else if (streq(prog, "counter")) { Scheduler::spawn("counter", proc_counter); proc_counter(); }
        else if (streq(prog, "sysinfo")) { Scheduler::spawn("sysinfo", proc_sysinfo); proc_sysinfo(); }
        else { content_print("  Unknown: ", CLR_RED); content_println(prog, CLR_WHITE); }
    }
    else if (streq(cmd, "color")) {
        content_println("\n  Cosmic Pulse palette:", CLR_WHITE);
        content_println("  CYAN    ", CLR_CYAN);
        content_println("  VIOLET  ", CLR_VIOLET);
        content_println("  PINK    ", CLR_PINK);
        content_println("  GREEN   ", CLR_GREEN);
        content_println("  YELLOW  ", CLR_YELLOW);
        content_print("\n", CLR_WHITE);
    }
    else if (streq(cmd, "panic")) {
        PANIC("Manual panic from pixel shell");
    }
    else if (cmd[0] == '\0') {}
    else {
        content_print("  Unknown: ", CLR_RED);
        content_println(cmd, CLR_WHITE);
        content_println("  Type 'help' for commands.", CLR_DIM);
    }
}


static int32_t last_cx = -1, last_cy = -1;

static void draw_cursor() {
    Mouse::State& ms = Mouse::get_state();
    if (last_cx >= 0)
        Framebuffer::draw_rect(last_cx, last_cy, 12, 18, CLR_BG);
    last_cx = ms.x;
    last_cy = ms.y;
    uint32_t col = ms.left ? CLR_CYAN : CLR_WHITE;
    Framebuffer::draw_line(ms.x,   ms.y,    ms.x,   ms.y+12, col);
    Framebuffer::draw_line(ms.x,   ms.y,    ms.x+8, ms.y+8,  col);
    Framebuffer::draw_line(ms.x+8, ms.y+8,  ms.x+4, ms.y+8,  col);
    Framebuffer::draw_line(ms.x+4, ms.y+8,  ms.x+6, ms.y+12, col);
}

void Shell::init() {
    input_len = 0;
    RamFS::init();
    Scheduler::init();
    draw_header();
}

void Shell::run() {
    while (true) {
        draw_topbar();
        // Demo: move cursor with keyboard arrows for now
        Mouse::poll();
        // Force cursor to center if not moving
        static uint32_t last_ticks = 0;
        if (pit_ticks - last_ticks > 500) {
            last_ticks = pit_ticks;
            Mouse::State& ms = Mouse::get_state();
            ms.x = (ms.x + 5) % 780;
            ms.y = (ms.y + 2) % 580;
        }
        draw_cursor();
        draw_prompt();
        input_len = 0;

        // Draw cursor
        int saved_x = content_x;
        int saved_y = content_y;
        bool cursor_on = true;

        while (true) {
            // Simple cursor blink using ticks
            bool should_be_on = (pit_ticks / 300) % 2 == 0;
            if (should_be_on != cursor_on) {
                Framebuffer::draw_rect(content_x, content_y, 6, 9,
                    cursor_on ? CLR_VIOLET : CLR_BG);
                cursor_on = should_be_on;
            }

            char c = Keyboard::getchar();

            // Clear cursor before processing
            Framebuffer::draw_rect(content_x, content_y, 6, 9, CLR_BG);

            if (c == '\n') {
                input_buf[input_len] = '\0';
                content_print("\n", CLR_WHITE);
                handle_command(input_buf);
                break;
            } else if (c == '\b') {
                if (input_len > 0) {
                    input_len--;
                    content_x -= 8;
                    Framebuffer::draw_rect(content_x, content_y, 8, 10, CLR_BG);
                }
            } else if (input_len < 255) {
                input_buf[input_len++] = c;
                Framebuffer::draw_char(c, content_x, content_y, CLR_WHITE);
                content_x += 8;
            }
        }
    }
}

void Shell::print(const char* str)   { content_print(str, CLR_WHITE); }
void Shell::println(const char* str) { content_println(str, CLR_WHITE); }

