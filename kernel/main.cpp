#include <cstdint>
#include <cstddef>

#include "frame_buffer_config.hpp"

// #@@range_begin(write_pixel)
struct PixelColor {
    uint8_t r, g, b;
};

/**
 * @brief 1개의 픽셀을 RGB값으로 설정
 * @param config FrameBufferConfig의 레퍼런스 타입, pixel_format이 enum PixelFormat에 정의되어 있어야 함
 * @param x,y 값을 쓸 픽셀의 열과 행
 * @retval 성공적으로 값을 쓰면 0 그렇지 않으면 0이 아님
 */
int WritePixel(const FrameBufferConfig& config, int x, int y, const PixelColor& c) {
    const int pixel_position = config.pixels_per_scan_line * y + x;
    if (config.pixel_format == kPixelRGBResv8BitPerColor) {
        uint8_t* p = &(config.frame_buffer[4 * pixel_position]);
        p[0] = c.r;
        p[1] = c.g;
        p[2] = c.b;
    }
    else if (config.pixel_format == kPixelBGRResv8BitPerColor) {
        uint8_t* p = &(config.frame_buffer[4 * pixel_position]);
        p[0] = c.b;
        p[1] = c.g;
        p[2] = c.r;
    }
    else {
        return -1;
    }
    return 0;
}
// #@@range_end(write_pixel)

// #@@range_begin(call_write_pixel)
/**
 * @brief 커널의 엔트리포인트 
 * @param frame_buffer_config FrameBufferConfig 타입, BIOS로부터 받아온 프레임 버퍼와 그 정보 
 */
extern "C" void KernelMain(const FrameBufferConfig& frame_buffer_config) {
    for (int y = 0; y < frame_buffer_config.vertical_resolution; ++y) {
        for (int x = 0; x < frame_buffer_config.horizontal_resolution; ++x) {
            WritePixel(frame_buffer_config, x, y, {255, 255, 255});
        }
    }

    for (int y = 0; y < 100; ++y) {
        for (int x = 0; x < 200; ++x) {
            WritePixel(frame_buffer_config, 100 + x, 100 + y, {0, 255, 0});
        }
    }

    while (1) __asm__("hlt");
}
// #@@range_end(call_write_pixel)