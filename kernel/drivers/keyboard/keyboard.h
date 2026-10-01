/**
 * NovaOS — PS/2 Keyboard Driver
 */

#pragma once

#include <stdint.h>


namespace Keyboard {

    // Initialize keyboard state.
    void init();


    // Blocking character read.
    //
    // Waits until a character is available.
    char getchar();


    // Non-blocking character read.
    //
    // Returns true when a character was read.
    // Returns false when the keyboard buffer is empty.
    bool try_getchar(
        char* out_char
    );


    // Returns true if at least one buffered
    // character is waiting.
    bool has_char();


    bool key_pressed(
        uint8_t scancode
    );


    // Called from the keyboard IRQ handler.
    void handle_irq();
}


// Special-key flags.
//
// These are set by the keyboard driver.
// The consumer should clear them after handling.

extern bool f1_pressed;
extern bool f2_pressed;
extern bool up_pressed;
extern bool down_pressed;
extern bool esc_pressed;