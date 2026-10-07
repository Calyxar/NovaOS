#include "apps.h"

#include "../theme/colors.h"
#include "../../kernel/drivers/video/framebuffer.h"
#include "../../kernel/drivers/video/font_renderer.h"

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

        // App title — Inter 16px
        FontRenderer::draw_text(
            title,
            x + 16,
            y + 16,
            16,
            NovaColors::TextPrimary
        );

        // App description — Inter 14px
        FontRenderer::draw_text(
            subtitle,
            x + 16,
            y + 50,
            14,
            NovaColors::TextSecondary
        );
    }

}

void AppsPage::draw() {

    // Page title — Inter 24px
    FontRenderer::draw_text(
        "Apps",
        190,
        70,
        24,
        NovaColors::TextPrimary
    );

    // Page description — Inter 14px
    FontRenderer::draw_text(
        "Your NovaOS applications",
        190,
        103,
        14,
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