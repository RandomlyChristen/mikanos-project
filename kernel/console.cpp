#include "console.hpp"

#include <cstring>
#include "font.hpp"
#include "layer.hpp"

Console::Console(PixelWriter* writer, 
    const PixelColor& fg_color, const PixelColor& bg_color)
    : writer_(writer), fg_color_(fg_color), bg_color_(bg_color),
    buffer_(), cursor_row_(0), cursor_columns_(0) {}

void Console::PutString(const char* s) {
    while (*s) {
        if (*s == '\n') {
            Newline();
        }
        else if (cursor_columns_ < kColumns - 1) {
            WriteAscii(*writer_, Vector2D<int>{8 * cursor_columns_, 16 * cursor_row_}, *s, fg_color_);
            buffer_[cursor_row_][cursor_columns_] = *s;
            ++cursor_columns_;
        }
        ++s;
    }
    if (layer_manager) {
        layer_manager->Draw();
    }
}

void Console::Refresh() {
    for (int row = 0; row < kRows; ++row) {
        WriteString(*writer_, Vector2D<int>{0, 16 * row}, buffer_[row], fg_color_);
    }
}

void Console::SetWriter(PixelWriter* writer) {
    if (writer == writer_) {
        return;
    }
    writer_ = writer;
    Refresh();
}

void Console::Newline() {
    cursor_columns_ = 0;
    if (cursor_row_ < kRows - 1) {
        ++cursor_row_;
    }
    else {
        for (int y = 0; y < 16 * kRows; ++y) {
            for (int x = 0; x < 8 * kColumns; ++x) {
                writer_->Write(Vector2D<int>{x, y}, bg_color_);
            }
        }
        for (int row = 0; row < kRows - 1; ++row) {
            memcpy(buffer_[row], buffer_[row + 1], kColumns + 1);
            WriteString(*writer_, Vector2D<int>{0, 16 * row}, buffer_[row], fg_color_);
        }
        memset(buffer_[kRows - 1], 0, kColumns + 1);
    }
}