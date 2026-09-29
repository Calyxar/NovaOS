#pragma once

#include <stdint.h>

namespace Cursor {
    void init();

    void move_to(int x, int y);

    void restore();
    void draw();

    int get_x();
    int get_y();
}