#pragma once

struct Rect {
    int x;
    int y;
    int width;
    int height;

    bool contains(int px, int py) const {
        return px >= x &&
               py >= y &&
               px < x + width &&
               py < y + height;
    }
};