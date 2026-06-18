/**
 * NovaOS — PS/2 Mouse Driver
 * Tracks cursor position and button state.
 */
#pragma once
#include <stdint.h>

namespace Mouse {
    struct State {
        int32_t  x, y;
        bool     left, right, middle;
    };

    void   init();
    void   handle_irq();
    State& get_state();
    void   poll();
    void   set_bounds(int32_t max_x, int32_t max_y);
}
