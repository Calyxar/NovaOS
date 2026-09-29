#include "button.h"
#include "../../kernel/drivers/video/framebuffer.h"
#include "../theme/colors.h"

Button::Button()
    : bounds{0, 0, 0, 0},
      label(""),
      selected(false),
      hovered(false) {}

void Button::set_bounds(
    int x,
    int y,
    int width,
    int height
) {
    bounds.x = x;
    bounds.y = y;
    bounds.width = width;
    bounds.height = height;
}

void Button::set_label(const char* text) {
    label = text;
}

void Button::set_selected(bool value) {
    selected = value;
}

void Button::set_hovered(bool value) {
    hovered = value;
}

bool Button::contains(int x, int y) const {
    return bounds.contains(x, y);
}

void Button::draw() {
    uint32_t background = NovaColors::Surface;
    uint32_t textColor = NovaColors::TextSecondary;

    if (hovered) {
        background = NovaColors::SurfaceHover;
        textColor = NovaColors::TextPrimary;
    }

    if (selected) {
        background = NovaColors::SurfaceHover;
        textColor = NovaColors::Cyan;
    }

    Framebuffer::draw_rect(
        bounds.x,
        bounds.y,
        bounds.width,
        bounds.height,
        background
    );

    // Small accent strip for active item
    if (selected) {
        Framebuffer::draw_rect(
            bounds.x,
            bounds.y,
            3,
            bounds.height,
            NovaColors::Cyan
        );
    }

    Framebuffer::print_at(
        label,
        bounds.x + 14,
        bounds.y + 12,
        textColor
    );
}