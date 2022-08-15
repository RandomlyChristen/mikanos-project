#pragma once

#include "graphics.hpp"

class Console {
public:
    static const int kRows = 25, kColumns = 80;

    /**
     * @brief 콘솔 클래스의 생성자
     * @param writer PixelWriter 레퍼런스 타입, 커널 진입시 초기화 된 값
     * @param fg_color 전경(폰트) 색상
     * @param bg_color 배경 색상
     */
    Console(PixelWriter& writer, 
        const PixelColor& fg_color, const PixelColor& bg_color);
    void PutString(const char* s);

private:
    void Newline();

    PixelWriter& writer_;
    const PixelColor fg_color_, bg_color_;
    char buffer_[kRows][kColumns + 1];
    int cursor_row_, cursor_columns_;
};