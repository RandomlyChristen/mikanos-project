#include <cstdint>
#include <cstdio>
#include <cstdarg>
#include <array>

#include "memory_map.hpp"
#include "graphics.hpp"
#include "font.hpp"
#include "console.hpp"
#include "pci.hpp"
#include "logger.hpp"
#include "usb/xhci/xhci.hpp"
#include "mouse.hpp"
#include "interrupt.hpp"
#include "segment.hpp"
#include "paging.hpp"
#include "memory_manager.hpp"
#include "window.hpp"
#include "layer.hpp"
#include "timer.hpp"


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

std::shared_ptr<Window> main_window;
unsigned int main_window_layer_id;
void InitializeMainWindow() {
    main_window = std::make_shared<Window>(
            160, 52, screen_config.pixel_format);
    DrawWindow(*main_window->Writer(), "Hello Window");

    main_window_layer_id = layer_manager->NewLayer()
            .SetWindow(main_window)
            .SetDraggable(true)
            .Move({300, 100})
            .ID();

    layer_manager->UpDown(main_window_layer_id, std::numeric_limits<int>::max());
}

/**
 * @brief 커널 콜에 사용될 스택
 */
alignas(16) uint8_t kernel_main_stack[1024 * 1024];

/**
 * @brief 외부 인터럽트 발생에 따른 Message객체를 저장하는 FIFO
 */
std::deque<Message>* main_queue;

/**
 * @brief 커널의 엔트리포인트 
 * @param frame_buffer_config FrameBufferConfig 타입, BIOS로부터 받아온 프레임 버퍼와 그 정보 
 * @param memory_map MemoryMap 타입, UEFI로부터 받아온 메모리 맵 정보
 */
extern "C" void KernelMainNewStack(const FrameBufferConfig& frame_buffer_config_ref, const MemoryMap& memory_map_ref) {
    InitializeGraphics(frame_buffer_config_ref);
    InitializeConsole();

    printk("Welcome to MikanOS!\n");
    SetLogLevel(kInfo);

    InitializeSegmentation();
    InitializePaging();

    InitializeMemoryManager(memory_map_ref);

    ::main_queue = new std::deque<Message>(32);
    InitializeInterrupt(main_queue);

    InitializePCI();
    usb::xhci::Initialize();

    InitializeLayer();
    InitializeMainWindow();
    InitializeMouse();

    InitializeLAPICTimer(*main_queue);

    timer_manager->AddTimer(Timer(100, 1));
    timer_manager->AddTimer(Timer(500, -1));

    layer_manager->Draw({{0, 0}, ScreenSize()});

    char str[128];
    uint32_t count = 0;

    __asm__("sti");
    /**
     * @brief 외부 인터럽트 이벤트 루프
     */
    while (true) {
        sprintf(str, "%010u", count);
        FillRectangle(*(main_window->Writer()), {24, 28}, {8 * 10, 16}, {0xc6, 0xc6, 0xc6});
        WriteString(*(main_window->Writer()), {24, 28}, str, {0, 0, 0});
        layer_manager->Draw(main_window_layer_id);

        __asm__("cli");     // critical section start
        count = timer_manager->CurrentTick();

        if (main_queue->empty()) {
             __asm__("sti\n\thlt");
            continue;
        }

        Message msg = main_queue->front();
        main_queue->pop_front();
        __asm__("sti");     // critical section end

        switch (msg.type) {
            case Message::kNull:
                break;
            case Message::kInterruptXHCI:
                usb::xhci::ProcessEvents();
                break;
            case Message::kTimerTimeout:
                printk("Timer: timeout = %lu, value = %d\n",
                       msg.arg.timer.timeout, msg.arg.timer.value);
                if (msg.arg.timer.value > 0) {
                    timer_manager->AddTimer(
                            Timer(msg.arg.timer.timeout + 100, msg.arg.timer.value + 1));
                }
                break;
            default:
                Log(kError, "Unknown message type: %d\n", msg.type);
        }
    }
}

extern "C" void __cxa_pure_virtual() {
  while (1) __asm__("hlt");
}