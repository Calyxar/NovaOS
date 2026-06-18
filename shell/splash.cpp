#include "splash.h"
#include "../kernel/drivers/video/framebuffer.h"
#include "../kernel/drivers/keyboard/keyboard.h"

#define WHITE   0xFFFFFF
#define CYAN    0x00E5FF
#define VIOLET  0x7B2FF7
#define PINK    0xFF0080
#define DIM     0x6666AA
#define DARK    0x050520

static void wait(uint32_t n) {
    volatile uint32_t i = n * 8000;
    while (i--);
}

static void draw_ring(int cx, int cy, int r, uint32_t color, int thickness) {
    for (int j = -r-thickness; j <= r+thickness; j++) {
        for (int k = -r-thickness; k <= r+thickness; k++) {
            int d = j*j + k*k;
            int rmin = (r-thickness)*(r-thickness);
            int rmax = (r+thickness)*(r+thickness);
            if (d >= rmin && d <= rmax)
                Framebuffer::put_pixel(cx+k, cy+j, color);
        }
    }
}

void Splash::show() {
    Framebuffer::Info& fb = Framebuffer::get_info();
    uint32_t W = fb.width  ? fb.width  : 800;
    uint32_t H = fb.height ? fb.height : 600;

    Framebuffer::clear(DARK);

    // Radial background glow
    for (uint32_t y = 0; y < H; y++) {
        for (uint32_t x = 0; x < W; x++) {
            int dx = (int)x - (int)W/2;
            int dy = (int)y - (int)H/2;
            int dist = (dx*dx + dy*dy) / 1000;
            if (dist < 60) {
                uint32_t b = (60 - dist) * 3;
                if (b > 40) b = 40;
                Framebuffer::put_pixel(x, y, 0x050520 + (b << 8) + b);
            }
        }
    }

    int cx = (int)W / 2;
    int cy = (int)H / 2 - 70;

    // Nova Wave logo — bright thick rings
    // Outer glow rings (dim)
    draw_ring(cx, cy, 68, 0x1A0055, 4);
    draw_ring(cx, cy, 50, 0x220066, 4);
    draw_ring(cx, cy, 32, 0x2A0088, 4);

    // Main rings — bright Cosmic Pulse colors
    draw_ring(cx, cy, 66, VIOLET, 2);
    draw_ring(cx, cy, 48, CYAN,   2);
    draw_ring(cx, cy, 30, PINK,   2);

    // Inner fill
    Framebuffer::draw_circle(cx, cy, 14, 0x2200AA);
    Framebuffer::draw_circle(cx, cy, 10, 0x5500DD);
    Framebuffer::draw_circle(cx, cy, 6,  WHITE);

    // Title
    int tx = cx - 72;
    int ty = cy + 90;
    Framebuffer::print_at("N  O  V  A  O  S", tx, ty,      WHITE);
    Framebuffer::print_at("v 0 . 1 . 0",      cx-40, ty+16, VIOLET);
    Framebuffer::print_at("Next-Generation OS for Future Computers",
                          cx - 152, ty + 36, DIM);

    // Loading bar track
    int bx = (int)W/2 - 150;
    int by = ty + 62;
    Framebuffer::draw_rect(bx-2, by-2, 304, 10, 0x111133);
    Framebuffer::draw_rect(bx,   by,   300,  6,  0x0A0A22);

    // Animate bar in Cosmic Pulse gradient
    uint32_t colors[] = { 0x00C6FF, 0x4488FF, 0x7B2FF7, 0xCC00AA, 0xFF0080 };
    for (int i = 0; i < 5; i++) {
        wait(60000);
        Framebuffer::draw_rect(bx + i*60, by, 60, 6, colors[i]);
    }

    Framebuffer::print_at("Press any key to continue...",
                          cx - 112, by + 16, DIM);

    Keyboard::getchar();
    Framebuffer::clear(DARK);
}
