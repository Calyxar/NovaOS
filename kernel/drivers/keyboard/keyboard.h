/**
 * novaOS — PS/2 Keyboard Driver
 */
#pragma once
#include <stdint.h>

namespace Keyboard {
    void init();
    char getchar();
    bool key_pressed(uint8_t scancode);
    void handle_irq();
}

// F1 key press flag — set when F1 is pressed, cleared after reading
extern bool f1_pressed;
extern bool f2_pressed;
extern bool up_pressed;
extern bool down_pressed;
extern bool esc_pressed;

