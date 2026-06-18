/**
 * NovaOS — VESA Pixel Framebuffer
 * 800x600 32-bit color pixel graphics.
 */
#pragma once
#include <stdint.h>

namespace Framebuffer {
    struct Info {
        uint32_t* addr;
        uint32_t  width;
        uint32_t  height;
        uint32_t  pitch;
        uint8_t   bpp;
    };

    void     init();
    void     init_vesa(uint32_t* addr, uint32_t w, uint32_t h, uint32_t pitch, uint8_t bpp);
    void     clear(uint32_t color);
    void     put_pixel(int x, int y, uint32_t color);
    void     draw_rect(int x, int y, int w, int h, uint32_t color);
    void     draw_circle(int cx, int cy, int r, uint32_t color);
    void     draw_line(int x0, int y0, int x1, int y1, uint32_t color);
    void     print(const char* str, uint32_t color = 0xFFFFFF);
    void     print_at(const char* str, int x, int y, uint32_t color);
    void     draw_char(char c, int x, int y, uint32_t color);
    void     scroll();
    void     set_cursor(int x, int y);
    Info&    get_info();
    int      get_cursor_x();
    int      get_cursor_y();
}
