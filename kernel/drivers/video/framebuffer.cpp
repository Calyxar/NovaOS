#include "framebuffer.h"
#include <stdint.h>

static uint16_t* vga     = (uint16_t*)0xB8000;
static int cursor_x      = 0;
static int cursor_y      = 0;
static Framebuffer::Info fb_info;

// VGA color codes
#define VGA_BLACK        0
#define VGA_BLUE         1
#define VGA_GREEN        2
#define VGA_CYAN         3
#define VGA_RED          4
#define VGA_MAGENTA      5
#define VGA_BROWN        6
#define VGA_WHITE        7
#define VGA_GRAY         8
#define VGA_LIGHT_BLUE   9
#define VGA_LIGHT_GREEN  10
#define VGA_LIGHT_CYAN   11
#define VGA_LIGHT_RED    12
#define VGA_PINK         13
#define VGA_YELLOW       14
#define VGA_BRIGHT_WHITE 15

static uint8_t current_fg = VGA_BRIGHT_WHITE;
static uint8_t current_bg = VGA_BLACK;

static uint16_t make_entry(char c, uint8_t fg, uint8_t bg) {
    return (uint16_t)c | ((uint16_t)((bg << 4) | fg) << 8);
}

static void scroll() {
    for (int y = 0; y < 24; y++)
        for (int x = 0; x < 80; x++)
            vga[y * 80 + x] = vga[(y + 1) * 80 + x];
    for (int x = 0; x < 80; x++)
        vga[24 * 80 + x] = make_entry(' ', current_fg, current_bg);
    cursor_y = 24;
}

void Framebuffer::init() {
    cursor_x = cursor_y = 0;
    current_fg = VGA_BRIGHT_WHITE;
    current_bg = VGA_BLACK;
}

void Framebuffer::clear(uint32_t color) {
    (void)color;
    for (int i = 0; i < 80 * 25; i++)
        vga[i] = make_entry(' ', VGA_BRIGHT_WHITE, VGA_BLACK);
    cursor_x = cursor_y = 0;
}

// Map our hex colors to VGA color codes
static uint8_t map_color(uint32_t color) {
    if (color == 0x00C2A8) return VGA_LIGHT_GREEN;
    if (color == 0x6C63FF) return VGA_LIGHT_BLUE;
    if (color == 0xFF6B6B) return VGA_LIGHT_RED;
    if (color == 0xFFFF00) return VGA_YELLOW;
    if (color == 0xF5A623) return VGA_YELLOW;
    if (color == 0x888888) return VGA_GRAY;
    if (color == 0x00FFFF) return VGA_LIGHT_CYAN;
    return VGA_BRIGHT_WHITE;
}

void Framebuffer::print(const char* str, uint32_t color) {
    uint8_t fg = map_color(color);
    for (int i = 0; str[i]; i++) {
        char c = str[i];
        if (c == '\n') {
            cursor_x = 0;
            cursor_y++;
            if (cursor_y >= 25) scroll();
            continue;
        }
        if (c == '\b') {
            if (cursor_x > 0) {
                cursor_x--;
                vga[cursor_y * 80 + cursor_x] = make_entry(' ', fg, VGA_BLACK);
            }
            continue;
        }
        vga[cursor_y * 80 + cursor_x] = make_entry(c, fg, VGA_BLACK);
        if (++cursor_x >= 80) { cursor_x = 0; cursor_y++; }
        if (cursor_y >= 25) scroll();
    }
}

void Framebuffer::put_pixel(int x, int y, uint32_t color) { (void)x;(void)y;(void)color; }
void Framebuffer::draw_rect(int x, int y, int w, int h, uint32_t color) { (void)x;(void)y;(void)w;(void)h;(void)color; }
Framebuffer::Info& Framebuffer::get_info() { return fb_info; }
