/**
 * NovaOS — Modern Font Renderer
 */

#include "font_renderer.h"
#include "framebuffer.h"
#include "generated_font.h"


namespace {


// ============================================================
// Default font
// ============================================================

FontRenderer::Font systemFont = {
    16,
    0,
    0,
    0
};


// ============================================================
// Color helpers
// ============================================================

uint8_t channel_r(uint32_t color) {
    return (color >> 16) & 0xFF;
}


uint8_t channel_g(uint32_t color) {
    return (color >> 8) & 0xFF;
}


uint8_t channel_b(uint32_t color) {
    return color & 0xFF;
}


uint32_t make_rgb(
    uint8_t r,
    uint8_t g,
    uint8_t b
) {
    return
        ((uint32_t)r << 16) |
        ((uint32_t)g << 8) |
        (uint32_t)b;
}


// ============================================================
// Find generated font size
// ============================================================

const NovaFontData::FontSizeData*
find_font_size(
    uint32_t pixel_size
) {

    for (
        uint32_t i = 0;
        i < NovaFontData::font_size_count;
        ++i
    ) {

        const NovaFontData::FontSizeData* font =
            NovaFontData::font_sizes[i];


        if (font->pixel_size == pixel_size)
            return font;
    }


    return nullptr;
}


// ============================================================
// Find glyph
// ============================================================

const NovaFontData::GlyphData*
find_glyph(
    const NovaFontData::FontSizeData* font,
    unsigned char character
) {

    if (!font)
        return nullptr;


    if (
        character < NovaFontData::first_character ||
        character > NovaFontData::last_character
    ) {
        return nullptr;
    }


    uint32_t index =
        (uint32_t)character -
        NovaFontData::first_character;


    return &font->glyphs[index];
}


// ============================================================
// Draw generated glyph
// ============================================================

void draw_generated_glyph(
    const NovaFontData::GlyphData& glyph,
    int x,
    int y,
    uint32_t color
) {

    if (!glyph.bitmap)
        return;


    Framebuffer::Info& info =
        Framebuffer::get_info();


    for (
        uint32_t gy = 0;
        gy < glyph.height;
        ++gy
    ) {

        for (
            uint32_t gx = 0;
            gx < glyph.width;
            ++gx
        ) {

            uint8_t alpha =
                glyph.bitmap[
                    gy * glyph.width + gx
                ];


            if (alpha == 0)
                continue;


            int screenX =
                x +
                glyph.offset_x +
                (int)gx;


            int screenY =
                y +
                glyph.offset_y +
                (int)gy;


            // --------------------------------------------
            // Framebuffer clipping
            // --------------------------------------------

            if (
                screenX < 0 ||
                screenY < 0 ||
                screenX >= (int)info.width ||
                screenY >= (int)info.height
            ) {
                continue;
            }


            uint32_t background =
                Framebuffer::get_pixel(
                    screenX,
                    screenY
                );


            uint32_t blended =
                FontRenderer::blend_color(
                    background,
                    color,
                    alpha
                );


            Framebuffer::put_pixel(
                screenX,
                screenY,
                blended
            );
        }
    }
}


} // namespace


// ============================================================
// Initialization
// ============================================================

void FontRenderer::init() {

    const NovaFontData::FontSizeData* font =
        find_font_size(16);


    if (!font)
        return;


    systemFont.pixel_size =
        font->pixel_size;

    systemFont.ascent =
        font->ascent;

    systemFont.descent =
        font->descent;

    systemFont.line_height =
        font->line_height;
}


// ============================================================
// Default font
// ============================================================

const FontRenderer::Font&
FontRenderer::default_font() {
    return systemFont;
}


// ============================================================
// Alpha blending
// ============================================================

uint32_t FontRenderer::blend_color(
    uint32_t background,
    uint32_t foreground,
    uint8_t alpha
) {

    if (alpha == 0)
        return background;


    if (alpha == 255)
        return foreground;


    uint32_t inverse =
        255 - alpha;


    uint32_t r =
        (
            channel_r(foreground) * alpha +
            channel_r(background) * inverse
        ) / 255;


    uint32_t g =
        (
            channel_g(foreground) * alpha +
            channel_g(background) * inverse
        ) / 255;


    uint32_t b =
        (
            channel_b(foreground) * alpha +
            channel_b(background) * inverse
        ) / 255;


    return make_rgb(
        (uint8_t)r,
        (uint8_t)g,
        (uint8_t)b
    );
}


// ============================================================
// Modern text rendering
// ============================================================

void FontRenderer::draw_text(
    const char* text,
    int x,
    int y,
    uint32_t pixel_size,
    uint32_t color
) {

    if (!text)
        return;


    const NovaFontData::FontSizeData* font =
        find_font_size(pixel_size);


    // --------------------------------------------------------
    // Unsupported size:
    // fall back to the 16px generated font.
    // --------------------------------------------------------

    if (!font)
        font = find_font_size(16);


    // --------------------------------------------------------
    // Emergency legacy fallback
    // --------------------------------------------------------

    if (!font) {

        Framebuffer::print_at(
            text,
            x,
            y,
            color
        );

        return;
    }


    int penX = x;
    int penY = y;


    for (
        size_t i = 0;
        text[i] != '\0';
        ++i
    ) {

        unsigned char character =
            (unsigned char)text[i];


        // ----------------------------------------------------
        // New line
        // ----------------------------------------------------

        if (character == '\n') {

            penX = x;

            penY +=
                font->line_height;

            continue;
        }


        // ----------------------------------------------------
        // Tab
        // ----------------------------------------------------

        if (character == '\t') {

            const NovaFontData::GlyphData* space =
                find_glyph(
                    font,
                    ' '
                );


            if (space)
                penX += space->advance * 4;


            continue;
        }


        // ----------------------------------------------------
        // Find glyph
        // ----------------------------------------------------

        const NovaFontData::GlyphData* glyph =
            find_glyph(
                font,
                character
            );


        // Unsupported character.
        if (!glyph)
            continue;


        // ----------------------------------------------------
        // Draw glyph
        // ----------------------------------------------------

        draw_generated_glyph(
            *glyph,
            penX,
            penY,
            color
        );


        // ----------------------------------------------------
        // Advance cursor
        // ----------------------------------------------------

        penX +=
            glyph->advance;
    }
}


// ============================================================
// Default 16px text
// ============================================================

void FontRenderer::draw_text(
    const char* text,
    int x,
    int y,
    uint32_t color
) {

    draw_text(
        text,
        x,
        y,
        16,
        color
    );
}


// ============================================================
// Text measurement
// ============================================================

int FontRenderer::measure_text(
    const char* text,
    uint32_t pixel_size
) {

    if (!text)
        return 0;


    const NovaFontData::FontSizeData* font =
        find_font_size(pixel_size);


    if (!font)
        font = find_font_size(16);


    if (!font)
        return 0;


    int width = 0;
    int currentLine = 0;


    for (
        size_t i = 0;
        text[i] != '\0';
        ++i
    ) {

        unsigned char character =
            (unsigned char)text[i];


        if (character == '\n') {

            if (currentLine > width)
                width = currentLine;


            currentLine = 0;

            continue;
        }


        if (character == '\t') {

            const NovaFontData::GlyphData* space =
                find_glyph(
                    font,
                    ' '
                );


            if (space)
                currentLine +=
                    space->advance * 4;


            continue;
        }


        const NovaFontData::GlyphData* glyph =
            find_glyph(
                font,
                character
            );


        if (!glyph)
            continue;


        currentLine +=
            glyph->advance;
    }


    if (currentLine > width)
        width = currentLine;


    return width;
}


int FontRenderer::measure_text(
    const char* text
) {

    return measure_text(
        text,
        16
    );
}