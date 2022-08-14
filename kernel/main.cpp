#include <cstdint>
#include <cstddef>
#include <cstdio>

#include "graphics.hpp"
#include "font.hpp"

void* operator new(size_t size, void* buf) { return buf; }
void operator delete(void* obj) noexcept {}

char pixel_writer_buf[sizeof(RGBResv8BitPerColorPixelWriter)];
PixelWriter* pixel_writer;

/**
 * @brief 커널의 엔트리포인트 
 * @param frame_buffer_config FrameBufferConfig 타입, BIOS로부터 받아온 프레임 버퍼와 그 정보 
 */
extern "C" void KernelMain(const FrameBufferConfig& frame_buffer_config) {
    switch (frame_buffer_config.pixel_format) {
        case kPixelBGRResv8BitPerColor:
            pixel_writer = new(pixel_writer_buf)
                BGRResv8BitPerColorPixelWriter(frame_buffer_config);
            break;
        case kPixelRGBResv8BitPerColor:
            pixel_writer = new(pixel_writer_buf)
                RGBResv8BitPerColorPixelWriter(frame_buffer_config);
            break;
    }

    for (int x = 0; x < frame_buffer_config.horizontal_resolution; ++x) {
        for (int y = 0; y < frame_buffer_config.vertical_resolution; ++y) {
            pixel_writer->Write(x, y, {0, 0, 0});
        }
    }
    for (int x = 400; x < 600; ++x) {
        for (int y = 400; y < 500; ++y) {
            pixel_writer->Write(x, y, {0, 255, 0});
        }
    }

    int i;
    char c;
    for (i = 0, c = '!'; c <= '~'; ++c, ++i) {
        WriteAscii(*pixel_writer, 8 * i, 50, c, {128, 128, 128});
    }
    WriteString(*pixel_writer, 0, 66, "Hello, World!", {255, 255, 255});

    char buf[128];
    sprintf(buf, "1 + 2 = %d", 1 + 2);
    WriteString(*pixel_writer, 0, 82, buf, {128, 128, 128});

    while (1) __asm__("hlt");
}