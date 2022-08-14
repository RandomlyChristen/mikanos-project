#pragma once

#include "graphics.hpp"

/**
 * @brief 화면에 하나의 아스키 문자를 표시하는 함수
 * @param writer PixelWriter의 레퍼런스, 커널 main에서 초기화한 전역 포인터를 참조
 * @param x , y 폰트를 쓸 좌상단 위치
 * @param c 표시할 아스키 문자
 * @param color 문자를 표시할 색상
 */
void WriteAscii(PixelWriter& writer, int x, int y, char c, const PixelColor& color);

/**
 * @brief 화면에 아스키 문자열을 표시하는 함수
 * @param writer PixelWriter의 레퍼런스, 커널 main에서 초기화한 전역 포인터를 참조
 * @param x , y 폰트를 쓸 좌상단 위치
 * @param s 표시할 아스키 문자열의 주소
 * @param color 문자열을 표시할 색상
 */
void WriteString(PixelWriter& writer, int x, int y, const char* s, const PixelColor& color);