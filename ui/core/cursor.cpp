#include "cursor.h"
#include "../../kernel/drivers/video/framebuffer.h"

namespace {

    constexpr int CURSOR_W = 10;
    constexpr int CURSOR_H = 16;

    int cursor_x = 0;
    int cursor_y = 0;

    bool background_saved = false;

    uint32_t saved_pixels[CURSOR_W * CURSOR_H];

    // Simple arrow cursor mask.
    // 1 = white pixel
    // 2 = dark outline pixel
    // 0 = transparent
    const uint8_t cursor_mask[CURSOR_H][CURSOR_W] = {
        {2,0,0,0,0,0,0,0,0,0},
        {2,1,0,0,0,0,0,0,0,0},
        {2,1,1,0,0,0,0,0,0,0},
        {2,1,1,1,0,0,0,0,0,0},
        {2,1,1,1,1,0,0,0,0,0},
        {2,1,1,1,1,1,0,0,0,0},
        {2,1,1,1,1,1,1,0,0,0},
        {2,1,1,1,1,1,1,1,0,0},
        {2,1,1,1,1,1,1,1,1,0},
        {2,1,1,1,2,2,2,2,2,2},
        {2,1,1,2,0,0,0,0,0,0},
        {2,1,2,1,0,0,0,0,0,0},
        {2,2,0,2,1,0,0,0,0,0},
        {2,0,0,2,1,0,0,0,0,0},
        {0,0,0,0,2,1,0,0,0,0},
        {0,0,0,0,2,2,0,0,0,0}
    };

    void save_background() {
        Framebuffer::Info& info = Framebuffer::get_info();

        for (int y = 0; y < CURSOR_H; ++y) {
            for (int x = 0; x < CURSOR_W; ++x) {

                int px = cursor_x + x;
                int py = cursor_y + y;

                uint32_t& slot =
                    saved_pixels[y * CURSOR_W + x];

                if (
                    px >= 0 &&
                    py >= 0 &&
                    px < (int)info.width &&
                    py < (int)info.height
                ) {
                    slot = Framebuffer::get_pixel(px, py);
                } else {
                    slot = 0;
                }
            }
        }

        background_saved = true;
    }

}


void Cursor::init() {
    Framebuffer::Info& info =
        Framebuffer::get_info();

    cursor_x = (int)info.width / 2;
    cursor_y = (int)info.height / 2;

    background_saved = false;
}


void Cursor::move_to(int x, int y) {
    Framebuffer::Info& info =
        Framebuffer::get_info();

    if (x < 0)
        x = 0;

    if (y < 0)
        y = 0;

    if (x >= (int)info.width)
        x = (int)info.width - 1;

    if (y >= (int)info.height)
        y = (int)info.height - 1;

    cursor_x = x;
    cursor_y = y;
}


void Cursor::restore() {
    if (!background_saved)
        return;

    Framebuffer::Info& info =
        Framebuffer::get_info();

    for (int y = 0; y < CURSOR_H; ++y) {
        for (int x = 0; x < CURSOR_W; ++x) {

            int px = cursor_x + x;
            int py = cursor_y + y;

            if (
                px >= 0 &&
                py >= 0 &&
                px < (int)info.width &&
                py < (int)info.height
            ) {
                Framebuffer::put_pixel(
                    px,
                    py,
                    saved_pixels[y * CURSOR_W + x]
                );
            }
        }
    }

    background_saved = false;
}


void Cursor::draw() {
    save_background();

    for (int y = 0; y < CURSOR_H; ++y) {
        for (int x = 0; x < CURSOR_W; ++x) {

            uint8_t value =
                cursor_mask[y][x];

            if (value == 0)
                continue;

            uint32_t color =
                value == 1
                    ? 0xFFFFFF
                    : 0x101010;

            Framebuffer::put_pixel(
                cursor_x + x,
                cursor_y + y,
                color
            );
        }
    }
}


int Cursor::get_x() {
    return cursor_x;
}


int Cursor::get_y() {
    return cursor_y;
}