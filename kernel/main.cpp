#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <cstdarg>

#include "graphics.hpp"
#include "font.hpp"
#include "console.hpp"

void* operator new(size_t size, void* buf) { return buf; }
void operator delete(void* obj) noexcept {}

char pixel_writer_buf[sizeof(RGBResv8BitPerColorPixelWriter)];
PixelWriter* pixel_writer;

char console_buf[sizeof(Console)];
Console* console;

/**
 * @brief 콘솔에 서식 문자열을 표시하는 printk 함수
 * @param format 문자열 서식
 * @param ... 서식지정 치환 값의 가변인자 리스트
 * @return int vsprintf의 결과 값
 */
int printk(const char* format, ...) {
    va_list ap;
    int result;
    char s[1024];

    va_start(ap, format);
    result = vsprintf(s, format, ap);
    va_end(ap);

    console->PutString(s);
    return result;
}

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
            pixel_writer->Write(x, y, {255, 255, 255});
        }
    }

    console = new(console_buf) Console(*pixel_writer, {128, 128, 128}, {0, 0, 0});

    // console->PutString("\0\0\0\0\0a");

    char buf[128];
    for (int i = 0; i < 27; ++i) {
        sprintf(buf, "line %d\n", i);
        console->PutString(buf);
    }

    for (int i = 0; i < 10; ++i) {
        printk("printk no line feed 1: %d ", i);
        printk("printk no line feed 2: %d ", i);
        printk("printk line feed: %d\n", i);
    }

    while (1) __asm__("hlt");
}