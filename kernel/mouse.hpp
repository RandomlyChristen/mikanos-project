#pragma once

#include "graphics.hpp"

class MouseCursor {
public:
    MouseCursor(PixelWriter* writer, PixelColor erase_color, Vector2D<int> initial_pos);
    void MoveRelative(Vector2D<int> displacement);

private:
    PixelWriter* pixel_writer_ = nullptr;
    PixelColor erase_color_;
    Vector2D<int> position_;
};