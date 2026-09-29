#pragma once

#include "../core/rect.h"

class Button {
public:
    Button();

    void set_bounds(int x, int y, int width, int height);
    void set_label(const char* text);

    void set_selected(bool selected);
    void set_hovered(bool hovered);

    bool contains(int x, int y) const;

    void draw();

private:
    Rect bounds;
    const char* label;

    bool selected;
    bool hovered;
};