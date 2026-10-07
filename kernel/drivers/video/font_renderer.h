/**
 * NovaOS — Modern Font Renderer
 */

#pragma once

#include <stdint.h>
#include <stddef.h>


namespace FontRenderer {


// ============================================================
// Font information
// ============================================================

struct Font {
    uint32_t pixel_size;

    uint32_t ascent;
    uint32_t descent;
    uint32_t line_height;
};


// ============================================================
// Initialization
// ============================================================

void init();


// ============================================================
// Font information
// ============================================================

const Font& default_font();


// ============================================================
// Color blending
// ============================================================

uint32_t blend_color(
    uint32_t background,
    uint32_t foreground,
    uint8_t alpha
);


// ============================================================
// Modern text rendering
// ============================================================

void draw_text(
    const char* text,
    int x,
    int y,
    uint32_t pixel_size,
    uint32_t color
);


// Default-size overload.
//
// This lets us write:
//
// FontRenderer::draw_text(
//     "Hello",
//     100,
//     100,
//     0xFFFFFF
// );
//
void draw_text(
    const char* text,
    int x,
    int y,
    uint32_t color
);


// ============================================================
// Text measurement
// ============================================================

int measure_text(
    const char* text,
    uint32_t pixel_size
);


int measure_text(
    const char* text
);


} // namespace FontRenderer