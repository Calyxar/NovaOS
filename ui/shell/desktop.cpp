#include "desktop.h"
#include "sidebar.h"

#include "../core/cursor.h"
#include "../pages/home.h"
#include "../pages/apps.h"
#include "../pages/files.h"
#include "../theme/colors.h"

#include "../../kernel/drivers/video/framebuffer.h"
#include "../../kernel/drivers/mouse/mouse.h"


static DesktopPage currentPage =
    DesktopPage::Home;


void Desktop::init() {
    Sidebar::init();
    FilesPage::init();
    Cursor::init();

    draw();

    Mouse::State& mouse =
        Mouse::get_state();

    Cursor::move_to(
        mouse.x,
        mouse.y
    );

    Cursor::draw();
}


void Desktop::set_page(
    DesktopPage page
) {
    if (currentPage == page) {
        return;
    }

    currentPage = page;

    draw();
}


DesktopPage Desktop::get_page() {
    return currentPage;
}


void Desktop::run() {
    bool previousLeft =
        false;

    int previousX =
        -1;

    int previousY =
        -1;


    for (;;) {

        // -----------------------------------------------------
        // Poll mouse
        // -----------------------------------------------------

        Mouse::poll();

        Mouse::State& mouse =
            Mouse::get_state();


        // -----------------------------------------------------
        // Process keyboard/page updates EVERY loop
        // -----------------------------------------------------

        bool pageChanged =
            false;


        if (
            currentPage ==
            DesktopPage::Files
        ) {
            pageChanged =
                FilesPage::update();
        }


        // -----------------------------------------------------
        // Mouse movement
        // -----------------------------------------------------

        bool moved =
            mouse.x != previousX ||
            mouse.y != previousY;


        if (moved) {
            Cursor::restore();


            Sidebar::handle_hover(
                mouse.x,
                mouse.y
            );


            if (
                currentPage ==
                DesktopPage::Files
            ) {
                FilesPage::handle_hover(
                    mouse.x,
                    mouse.y
                );
            }


            draw();


            Cursor::move_to(
                mouse.x,
                mouse.y
            );


            Cursor::draw();


            previousX =
                mouse.x;

            previousY =
                mouse.y;


            pageChanged =
                false;
        }


        // -----------------------------------------------------
        // Mouse click
        // -----------------------------------------------------

        bool clicked =
            mouse.left &&
            !previousLeft;


        if (clicked) {
            Cursor::restore();


            Sidebar::handle_click(
                mouse.x,
                mouse.y
            );


            if (
                currentPage ==
                DesktopPage::Files
            ) {
                FilesPage::handle_click(
                    mouse.x,
                    mouse.y
                );
            }


            draw();


            Cursor::move_to(
                mouse.x,
                mouse.y
            );


            Cursor::draw();


            pageChanged =
                false;
        }


        // -----------------------------------------------------
        // Keyboard changed the editor
        // -----------------------------------------------------

        if (pageChanged) {
            Cursor::restore();


            draw();


            Cursor::move_to(
                mouse.x,
                mouse.y
            );


            Cursor::draw();
        }


        previousLeft =
            mouse.left;
    }
}


void Desktop::draw() {
    Framebuffer::Info& info =
        Framebuffer::get_info();

    // Main desktop background
    Framebuffer::clear(
        NovaColors::Desktop
    );

    // --------------------------------
    // Top bar
    // --------------------------------

    Framebuffer::draw_rect(
        0,
        0,
        info.width,
        40,
        NovaColors::SurfaceRaised
    );

    Framebuffer::print_at(
        "NovaOS",
        16,
        12,
        NovaColors::TextPrimary
    );

    // --------------------------------
    // Sidebar
    // --------------------------------

    Sidebar::draw();

    // --------------------------------
    // Current page
    // --------------------------------

    switch (currentPage) {

        case DesktopPage::Home:
            HomePage::draw();
            break;

        case DesktopPage::Apps:
            AppsPage::draw();
            break;

            Framebuffer::print_at(
                "Your applications will appear here.",
                190,
                100,
                NovaColors::TextSecondary
            );
            break;

        case DesktopPage::Files:
                FilesPage::draw();
                break;

            Framebuffer::print_at(
                "Browse NovaFS",
                190,
                100,
                NovaColors::TextSecondary
            );
            break;

        case DesktopPage::Terminal:
            Framebuffer::print_at(
                "Terminal",
                190,
                72,
                NovaColors::TextPrimary
            );

            Framebuffer::print_at(
                "Nova Shell",
                190,
                100,
                NovaColors::TextSecondary
            );
            break;

        case DesktopPage::Settings:
            Framebuffer::print_at(
                "Settings",
                190,
                72,
                NovaColors::TextPrimary
            );

            Framebuffer::print_at(
                "System preferences",
                190,
                100,
                NovaColors::TextSecondary
            );
            break;
    }
}