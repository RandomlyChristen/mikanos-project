#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <cstdarg>

#include "graphics.hpp"
#include "font.hpp"
#include "console.hpp"
#include "error.hpp"
#include "pci.hpp"
#include "logger.hpp"
#include "usb/memory.hpp"
#include "usb/device.hpp"
#include "usb/classdriver/mouse.hpp"
#include "usb/xhci/xhci.hpp"
#include "usb/xhci/trb.hpp"
#include "mouse.hpp"

// void* operator new(size_t size, void* buf) { return buf; }
// void operator delete(void* obj) noexcept {}

const PixelColor kDesktopBGColor{45, 118, 237};
const PixelColor kDesktopFGColor{255, 255, 255};

char pixel_writer_buf[sizeof(RGBResv8BitPerColorPixelWriter)];
PixelWriter* pixel_writer;

char console_buf[sizeof(Console)];
Console* console;

char mouse_cursur_buf[sizeof(MouseCursor)];
MouseCursor* mouse_cursor;

void MouseObserver(int8_t displacement_x, int8_t displacement_y) {
    // Log(kError, "+%d, +%d\n", displacement_x, displacement_y);
    mouse_cursor->MoveRelative({displacement_x, displacement_y});
}

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

    const int kFrameWidth = frame_buffer_config.horizontal_resolution;
    const int kFrameHeight = frame_buffer_config.vertical_resolution;

    FillRectangle(*pixel_writer, {0, 0}, {kFrameWidth, kFrameHeight - 50}, kDesktopBGColor);
    FillRectangle(*pixel_writer, {0, kFrameHeight - 50}, {kFrameWidth, 50}, {1, 8, 17});
    FillRectangle(*pixel_writer, {0, kFrameHeight - 50}, {kFrameWidth / 5, 50}, {80, 80, 80});
    DrawRectangle(*pixel_writer, {10, kFrameHeight - 40}, {30, 30}, {160, 160, 160});

    mouse_cursor = new(mouse_cursur_buf) MouseCursor(pixel_writer, kDesktopBGColor, {kFrameWidth / 2, kFrameHeight / 2});

    console = new(console_buf) Console(*pixel_writer, kDesktopFGColor, kDesktopBGColor);
    printk("Resolution : %d x %d\n", kFrameWidth, kFrameHeight);
    SetLogLevel(kWarn);

    {
        auto err = pci::ScanAllBus();
        Log(kDebug, "ScanAllBus: %s\n", err.Name());
    }

    pci::Device* xhc_dev = nullptr;

    for (int i = 0; i < pci::num_device; ++i) {
        auto& dev = pci::devices[i];
        auto vendor_id = pci::ReadVendorId(dev.bus, dev.device, dev.function);

        Log(kDebug, "%d.%d.%d: vend 0x%04x, class 0x%02x 0x%02x 0x%02x, head 0x%02x\n", 
            dev.bus, dev.device, dev.function, vendor_id, dev.class_code.base, dev.class_code.sub, dev.class_code.interface, dev.header_type);

        if (dev.class_code.Match(0x0cu, 0x03u, 0x30u /* xHC class code */)) {
            xhc_dev = &dev;

            if (vendor_id == 0x8086) {
                break;
            }
        }
    }

    if (xhc_dev) {
        Log(kInfo, "xHC has been found: %d.%d.%d\n", xhc_dev->bus, xhc_dev->device, xhc_dev->function);
    }

    const WithError<uint64_t> xhc_bar = pci::ReadBar(*xhc_dev, 0);
    Log(kDebug, "ReadBar : %s\n", xhc_bar.error.Name());
    // xHC 64bit PCI BAR(0~1)
    const uint64_t xhc_mmio_base = xhc_bar.value & ~static_cast<uint64_t>(0xf);
    Log(kDebug, "xHC mmio_base = %08lx\n", xhc_mmio_base);
    
    usb::xhci::Controller xhc{xhc_mmio_base};
    if (0x8096 == pci::ReadVendorId(xhc_dev->bus, xhc_dev->device, xhc_dev->function)) {
        Log(kError, "NEED TO SWITCH EHCI TO XHCI\n");
    }
    {
        auto err = xhc.Initialize();
        Log(kDebug, "xhc.Initialize: %s\n", err.Name());
    }
    Log(kDebug, "xHC statring\n");
    xhc.Run();

    usb::HIDMouseDriver::default_observer = MouseObserver;

    for (int i = 1; i <= xhc.MaxPorts(); ++i) {
        auto port = xhc.PortAt(i);
        Log(kDebug, "Port %d: IsConnected=%d\n", i, port.IsConnected());

        if (port.IsConnected()) {
            if (auto err = ConfigurePort(xhc, port)) {
                Log(kError, "failed to configure port: %s at %s:%d\n", err.Name(), err.File(), err.Line());
                continue;
            }
        }
    }

    while (1) {
        if (auto err = ProcessEvent(xhc)) {
            Log(kError, "Error while ProcessEvent: %s at %s:%d\n", err.Name(), err.File(), err.Line());
        }
    }

    while (1) __asm__("hlt");
}

extern "C" void __cxa_pure_virtual() {
  while (1) __asm__("hlt");
}