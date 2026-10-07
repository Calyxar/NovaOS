/** NovaOS — First Boot Account Setup */

#include "setup.h"

#include "../core/cursor.h"
#include "../theme/colors.h"

#include "../../kernel/user/user.h"
#include "../../kernel/user/session.h"

#include "../../kernel/drivers/video/framebuffer.h"
#include "../../kernel/drivers/video/font_renderer.h"
#include "../../kernel/drivers/mouse/mouse.h"
#include "../../kernel/drivers/keyboard/keyboard.h"


namespace {

// ------------------------------------------------------------
// Layout
// ------------------------------------------------------------

constexpr int CARD_X = 225;
constexpr int CARD_Y = 125;
constexpr int CARD_W = 350;
constexpr int CARD_H = 340;

constexpr int INPUT_X = 275;
constexpr int INPUT_Y = 275;
constexpr int INPUT_W = 250;
constexpr int INPUT_H = 42;

constexpr int BUTTON_X = 300;
constexpr int BUTTON_Y = 345;
constexpr int BUTTON_W = 200;
constexpr int BUTTON_H = 44;


// ------------------------------------------------------------
// State
// ------------------------------------------------------------

char username[NOVA_USERNAME_MAX + 1];

size_t usernameLength = 0;

bool inputFocused = true;
bool buttonHovered = false;

const char* errorMessage = nullptr;


// ------------------------------------------------------------
// Helpers
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


bool valid_username_char(char c) {
    return
        (c >= 'a' && c <= 'z') ||
        (c >= 'A' && c <= 'Z') ||
        (c >= '0' && c <= '9') ||
        c == '_' ||
        c == '-';
}


void clear_username() {
    for (size_t i = 0;
         i < NOVA_USERNAME_MAX + 1;
         ++i) {

        username[i] = '\0';
    }

    usernameLength = 0;
}


bool create_account() {

    if (usernameLength == 0) {
        errorMessage =
            "Please enter a username";

        return false;
    }

    if (!User::create(username)) {
        errorMessage =
            "Could not create account";

        return false;
    }

    if (!Session::login(username)) {
        errorMessage =
            "Account created, login failed";

        return false;
    }

    errorMessage = nullptr;

    return true;
}

} // namespace


// ============================================================
// Initialization
// ============================================================

void SetupPage::init() {

    clear_username();

    inputFocused = true;
    buttonHovered = false;
    errorMessage = nullptr;

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

void SetupPage::draw() {

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
        "Welcome toNovaOS",
        16,
        12,
        NovaColors::TextPrimary
    );


    // --------------------------------------------------------
    // Setup card
    // --------------------------------------------------------

    Framebuffer::draw_rounded_rect(
        CARD_X,
        CARD_Y,
        CARD_W,
        CARD_H,
        14,
        NovaColors::SurfaceRaised
    );


    FontRenderer::draw_text(
    "Welcome to NovaOS",
    CARD_X + 75,
    CARD_Y + 30,
    24,
    NovaColors::TextPrimary
);

    Framebuffer::print_at(
        "Create your first account",
        CARD_X + 75,
        CARD_Y + 70,
        NovaColors::TextSecondary
    );


    // --------------------------------------------------------
    // Username label
    // --------------------------------------------------------

    Framebuffer::print_at(
        "Username",
        INPUT_X,
        INPUT_Y - 25,
        NovaColors::TextSecondary
    );


    // --------------------------------------------------------
    // Username input
    // --------------------------------------------------------

    uint32_t inputColor =
        inputFocused
            ? NovaColors::SurfaceHover
            : NovaColors::Surface;


    Framebuffer::draw_rounded_rect(
        INPUT_X,
        INPUT_Y,
        INPUT_W,
        INPUT_H,
        8,
        inputColor
    );


    if (usernameLength > 0) {

        Framebuffer::print_at(
            username,
            INPUT_X + 12,
            INPUT_Y + 13,
            NovaColors::TextPrimary
        );

    } else {

        Framebuffer::print_at(
            "Enter username",
            INPUT_X + 12,
            INPUT_Y + 13,
            NovaColors::TextMuted
        );
    }


    // --------------------------------------------------------
    // Create button
    // --------------------------------------------------------

    uint32_t buttonColor =
        buttonHovered
            ? NovaColors::SurfaceHover
            : NovaColors::Surface;


    Framebuffer::draw_rounded_rect(
        BUTTON_X,
        BUTTON_Y,
        BUTTON_W,
        BUTTON_H,
        8,
        buttonColor
    );


    Framebuffer::print_at(
        "Create Account",
        BUTTON_X + 48,
        BUTTON_Y + 15,
        NovaColors::Cyan
    );


    // --------------------------------------------------------
    // Error
    // --------------------------------------------------------

    if (errorMessage) {

        Framebuffer::print_at(
            errorMessage,
            CARD_X + 70,
            BUTTON_Y + 70,
            NovaColors::TextSecondary
        );
    }


    Framebuffer::print_at(
        "Letters, numbers, _ and -",
        CARD_X + 78,
        CARD_Y + 295,
        NovaColors::TextMuted
    );
}


// ============================================================
// Setup loop
// ============================================================

bool SetupPage::run() {

    bool previousLeft = false;

    int previousX = -1;
    int previousY = -1;


    for (;;) {

        // ----------------------------------------------------
        // Keyboard input
        // ----------------------------------------------------

        bool changed = false;

        char c;

        while (
            inputFocused &&
            Keyboard::try_getchar(&c)
        ) {

            // Backspace
            if (c == '\b') {

                if (usernameLength > 0) {

                    --usernameLength;

                    username[
                        usernameLength
                    ] = '\0';

                    errorMessage = nullptr;

                    changed = true;
                }

                continue;
            }


            // Enter
            if (c == '\n') {

                if (create_account()) {
                    Cursor::restore();
                    return true;
                }

                changed = true;

                continue;
            }


            // Valid username character
            if (
                valid_username_char(c) &&
                usernameLength <
                    NOVA_USERNAME_MAX
            ) {

                username[
                    usernameLength++
                ] = c;

                username[
                    usernameLength
                ] = '\0';

                errorMessage = nullptr;

                changed = true;
            }
        }


        // ----------------------------------------------------
        // Mouse
        // ----------------------------------------------------

        Mouse::poll();

        Mouse::State& mouse =
            Mouse::get_state();


        bool moved =
            mouse.x != previousX ||
            mouse.y != previousY;


        if (moved) {

            Cursor::restore();


            buttonHovered =
                point_inside(
                    mouse.x,
                    mouse.y,
                    BUTTON_X,
                    BUTTON_Y,
                    BUTTON_W,
                    BUTTON_H
                );


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


            changed = false;
        }


        // ----------------------------------------------------
        // Click
        // ----------------------------------------------------

        bool clicked =
            mouse.left &&
            !previousLeft;


        if (clicked) {

            // Username field
            if (
                point_inside(
                    mouse.x,
                    mouse.y,
                    INPUT_X,
                    INPUT_Y,
                    INPUT_W,
                    INPUT_H
                )
            ) {

                inputFocused = true;
                errorMessage = nullptr;
                changed = true;
            }


            // Create Account button
            else if (
                point_inside(
                    mouse.x,
                    mouse.y,
                    BUTTON_X,
                    BUTTON_Y,
                    BUTTON_W,
                    BUTTON_H
                )
            ) {

                inputFocused = true;

                if (create_account()) {
                    Cursor::restore();
                    return true;
                }

                changed = true;
            }
        }


        // ----------------------------------------------------
        // Redraw after keyboard/click changes
        // ----------------------------------------------------

        if (changed) {

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