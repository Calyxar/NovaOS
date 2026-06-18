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
