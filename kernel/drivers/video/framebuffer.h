/**
 * novaOS — Early Framebuffer Driver
 * Writes directly to VESA framebuffer during kernel boot.
 * NOT the compositor — just for kernel debug output.
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
    void init();
    void clear(uint32_t color);
    void put_pixel(int x, int y, uint32_t color);
    void print(const char* str, uint32_t color = 0xE8E8F0);
    void draw_rect(int x, int y, int w, int h, uint32_t color);
    Info& get_info();
}
