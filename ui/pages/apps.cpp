#include "apps.h"

#include "../theme/colors.h"
#include "../../kernel/drivers/video/framebuffer.h"

namespace {

    void draw_app_tile(
        int x,
        int y,
        const char* title,
        const char* subtitle
    ) {
        Framebuffer::draw_rounded_rect(
            x,
            y,
            150,
            110,
            12,
            NovaColors::SurfaceRaised
        );

        Framebuffer::print_at(
            title,
            x + 16,
            y + 18,
            NovaColors::TextPrimary
        );

        Framebuffer::print_at(
            subtitle,
            x + 16,
            y + 52,
            NovaColors::TextSecondary
        );
    }

}

void AppsPage::draw() {
    Framebuffer::print_at(
        "Apps",
        190,
        72,
        NovaColors::TextPrimary
    );

    Framebuffer::print_at(
        "Your NovaOS applications",
        190,
        98,
        NovaColors::TextSecondary
    );

    draw_app_tile(
        190,
        140,
        "Terminal",
        "Nova Shell"
    );

    draw_app_tile(
        360,
        140,
        "Files",
        "Browse NovaFS"
    );

    draw_app_tile(
        530,
        140,
        "Settings",
        "System preferences"
    );

    draw_app_tile(
        190,
        270,
        "Browser",
        "Coming soon"
    );

    draw_app_tile(
        360,
        270,
        "Docs",
        "Coming soon"
    );

    draw_app_tile(
        530,
        270,
        "Store",
        "Coming soon"
    );
}