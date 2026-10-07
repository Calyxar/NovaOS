/** NovaOS — Login Screen */

#include "login.h"

#include "../core/cursor.h"
#include "../theme/colors.h"

#include "../../kernel/user/user.h"
#include "../../kernel/user/session.h"

#include "../../kernel/drivers/video/framebuffer.h"
#include "../../kernel/drivers/mouse/mouse.h"


namespace {

// ------------------------------------------------------------
// Layout
// ------------------------------------------------------------

constexpr int CARD_X = 250;
constexpr int CARD_Y = 170;
constexpr int CARD_W = 300;
constexpr int CARD_H = 230;

constexpr int USER_X = 290;
constexpr int USER_Y = 270;
constexpr int USER_W = 220;
constexpr int USER_H = 44;


// ------------------------------------------------------------
// State
// ------------------------------------------------------------

bool userHovered = false;


// ------------------------------------------------------------
// Hit testing
// ------------------------------------------------------------

bool point_inside(
    int px,
    int py,
    int x,
    int y,
    int w,
    int h
) {
    return
        px >= x &&
        py >= y &&
        px < x + w &&
        py < y + h;
}


// ------------------------------------------------------------
// User selection
// ------------------------------------------------------------

User::Account* first_user() {
   return User::get(0);
}

}

// ============================================================
// Initialization
// ============================================================

void LoginPage::init() {
    userHovered = false;

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


// ============================================================
// Drawing
// ============================================================

void LoginPage::draw() {
    Framebuffer::Info& info =
        Framebuffer::get_info();

    Framebuffer::clear(
        NovaColors::Desktop
    );


    // --------------------------------------------------------
    // Top bar
    // --------------------------------------------------------

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


    // --------------------------------------------------------
    // Login card
    // --------------------------------------------------------

    Framebuffer::draw_rounded_rect(
        CARD_X,
        CARD_Y,
        CARD_W,
        CARD_H,
        14,
        NovaColors::SurfaceRaised
    );

    Framebuffer::print_at(
        "Welcome to NovaOS",
        CARD_X + 70,
        CARD_Y + 30,
        NovaColors::TextPrimary
    );

    Framebuffer::print_at(
        "Choose your account",
        CARD_X + 74,
        CARD_Y + 60,
        NovaColors::TextSecondary
    );


    // --------------------------------------------------------
    // User
    // --------------------------------------------------------

    User::Account* account =
        first_user();

    if (!account) {
        Framebuffer::print_at(
            "No user accounts found",
            CARD_X + 62,
            CARD_Y + 120,
            NovaColors::TextSecondary
        );

        return;
    }


    uint32_t userColor =
        userHovered
            ? NovaColors::SurfaceHover
            : NovaColors::Desktop;


    Framebuffer::draw_rounded_rect(
        USER_X,
        USER_Y,
        USER_W,
        USER_H,
        8,
        userColor
    );


    // Simple account icon
    Framebuffer::draw_circle(
        USER_X + 24,
        USER_Y + 22,
        10,
        NovaColors::Cyan
    );


    Framebuffer::print_at(
        account->username,
        USER_X + 48,
        USER_Y + 15,
        NovaColors::TextPrimary
    );


    Framebuffer::print_at(
        "Click account to sign in",
        CARD_X + 58,
        CARD_Y + 170,
        NovaColors::TextSecondary
    );
}


// ============================================================
// Main login loop
// ============================================================

bool LoginPage::run() {
    bool previousLeft = false;

    int previousX = -1;
    int previousY = -1;


    for (;;) {

        Mouse::poll();

        Mouse::State& mouse =
            Mouse::get_state();


        // ----------------------------------------------------
        // Mouse movement
        // ----------------------------------------------------

        bool moved =
            mouse.x != previousX ||
            mouse.y != previousY;


        if (moved) {

            Cursor::restore();


            bool newHover =
                point_inside(
                    mouse.x,
                    mouse.y,
                    USER_X,
                    USER_Y,
                    USER_W,
                    USER_H
                );


            if (newHover != userHovered) {
                userHovered =
                    newHover;
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
        }


        // ----------------------------------------------------
        // Click
        // ----------------------------------------------------

        bool clicked =
            mouse.left &&
            !previousLeft;


        if (clicked) {

            if (
                point_inside(
                    mouse.x,
                    mouse.y,
                    USER_X,
                    USER_Y,
                    USER_W,
                    USER_H
                )
            ) {

                User::Account* account =
                    first_user();


                if (
                    account &&
                    Session::login(
                        account->username
                    )
                ) {

                    Cursor::restore();

                    return true;
                }
            }
        }


        previousLeft =
            mouse.left;
    }
}