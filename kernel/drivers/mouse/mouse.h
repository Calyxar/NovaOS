#pragma once
#include <stdint.h>

namespace Mouse {
    struct State {
        int32_t x, y;
        bool left, right, middle;
    };

    void   init();
    void   handle_irq();
    void   poll();
    State& get_state();
    void   set_bounds(int32_t max_x, int32_t max_y);
}

extern bool f1_pressed;
