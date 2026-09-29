#include "home.h"

#include "../theme/colors.h"
#include "../../kernel/drivers/video/framebuffer.h"

void HomePage::draw() {
    Framebuffer::print_at(
        "Welcome to NovaOS",
        190,
        72,
        NovaColors::TextPrimary
    );

    Framebuffer::print_at(
        "Your personal computing environment",
        190,
        96,
        NovaColors::TextSecondary
    );

    // Recent apps card
    Framebuffer::draw_rounded_rect(
        190,
        135,
        250,
        130,
        12,
        NovaColors::SurfaceRaised
    );

    Framebuffer::print_at(
        "Recent Apps",
        210,
        155,
        NovaColors::TextPrimary
    );

    Framebuffer::print_at(
        "Terminal",
        210,
        190,
        NovaColors::TextSecondary
    );

    Framebuffer::print_at(
        "Files",
        210,
        218,
        NovaColors::TextSecondary
    );

    // System card
    Framebuffer::draw_rounded_rect(
        460,
        135,
        280,
        130,
        12,
        NovaColors::SurfaceRaised
    );

    Framebuffer::print_at(
        "System",
        480,
        155,
        NovaColors::TextPrimary
    );

    Framebuffer::print_at(
        "NovaOS v0.1.0",
        480,
        190,
        NovaColors::TextSecondary
    );

    Framebuffer::print_at(
        "Framebuffer: VESA",
        480,
        218,
        NovaColors::TextSecondary
    );
}