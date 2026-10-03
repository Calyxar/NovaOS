/**
 * NovaOS — PS/2 Keyboard Driver
 */

#pragma once

#include <stdint.h>

namespace Keyboard {

    void init();

    // Existing keyboard interfaces
    char getchar();
    bool try_getchar(char* out_char);
    bool has_char();

    bool key_pressed(uint8_t scancode);

    // Keyboard interrupt handler
    void handle_irq();
}

// Existing special keys
extern bool f1_pressed;
extern bool f2_pressed;

extern bool up_pressed;
extern bool down_pressed;
extern bool esc_pressed;

// New editor/navigation keys
extern bool left_pressed;
extern bool right_pressed;

extern bool home_pressed;
extern bool end_pressed;

extern bool delete_pressed;

// Ctrl+S shortcut
extern bool save_pressed;