#pragma once

#include <stdint.h>

namespace Mouse {

    struct State {
        int32_t x;
        int32_t y;

        bool left;
        bool right;
        bool middle;
    };

    void init();

    void handle_irq();
    void poll();

    State& get_state();

    void set_bounds(
        int32_t width,
        int32_t height
    );
}

extern bool f1_pressed;