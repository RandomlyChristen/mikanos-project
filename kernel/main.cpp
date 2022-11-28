#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <cstdarg>
#include <array>

#include "memory_map.hpp"
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
#include "interrupt.hpp"
#include "asmfunc.h"
#include "queue.hpp"
#include "segment.hpp"
#include "paging.hpp"
#include "memory_manager.hpp"
#include "window.hpp"
#include "layer.hpp"
#include "timer.hpp"
#include "frame_buffer.hpp"


char pixel_writer_buf[sizeof(RGBResv8BitPerColorPixelWriter)];
PixelWriter* pixel_writer;

char console_buf[sizeof(Console)];
Console* console;

unsigned int mouse_layer_id;
Vector2D<int> screen_size;
Vector2D<int> mouse_position;

int printk(const char* format, ...) {
    va_list ap;
    int result;
    char s[1024];

    va_start(ap, format);
    result = vsprintf(s, format, ap);
    va_end(ap);

    StartLAPICTimer();
    console->PutString(s);
    auto elapsed = LAPICTimerElapsed();
    StopLAPICTimer();

    sprintf(s, "[%9d]", elapsed);
    console->PutString(s);

    return result;
}

void MouseObserver(uint8_t buttons, int8_t displacement_x, int8_t displacement_y) {
    static unsigned int mouse_drag_layer_id = 0;
    static uint8_t previous_buttons = 0;

    const auto oldpos = mouse_position;
    auto newpos = mouse_position + Vector2D<int>{displacement_x, displacement_y};
    newpos = ElementMin(newpos, screen_size + Vector2D<int>{-1, -1});
    mouse_position = ElementMax(newpos, {0, 0});

    const auto posdiff = mouse_position - oldpos;

    layer_manager->Move(mouse_layer_id, mouse_position);

    const bool previous_left_pressed = (previous_buttons & 0x01);
    const bool left_pressed = (buttons & 0x01);
    if (!previous_left_pressed && left_pressed) {
        auto layer = layer_manager->FindLayerByPosition(mouse_position, mouse_layer_id);
        if (layer && layer->IsDraggable()) {
            mouse_drag_layer_id = layer->ID();
            Log(kInfo, "Mouse Clicked on layer %d\n", mouse_drag_layer_id);
        }
    } else if (previous_left_pressed && left_pressed) {
        if (mouse_drag_layer_id > 0) {
            layer_manager->MoveRelative(mouse_drag_layer_id, posdiff);
            Log(kInfo, "Mouse drag moved layer %d\n", mouse_drag_layer_id);
        }
    } else if (previous_left_pressed && !left_pressed) {
        mouse_drag_layer_id = 0;
        Log(kInfo, "Mouse drag ended\n");
    }

    previous_buttons = buttons;
}

char memory_manager_buf[sizeof(BitmapMemoryManager)];
BitmapMemoryManager* memory_manager;

usb::xhci::Controller* xhc;

void SwitchEhci2Xhci(const pci::Device& xhc_dev) {
    bool intel_ehc_exist = false;
    for (int i = 0; i < pci::num_device; ++i) {
        if (pci::devices[i].class_code.Match(0x0cu, 0x03u, 0x20u) /* EHCI */ &&
            0x8086 == pci::ReadVendorId(pci::devices[i].bus, pci::devices[i].device, pci::devices[i].function)) {
            intel_ehc_exist = true;
            break;
        }
    }
    if (!intel_ehc_exist)
        return;
    uint32_t superspeed_ports = pci::ReadConfReg(xhc_dev, 0xdc); // USB3PRM
    pci::WriteConfReg(xhc_dev, 0xd8, superspeed_ports); // USB3_PSSEN
    uint32_t ehci2xhci_ports = pci::ReadConfReg(xhc_dev, 0xd4); // XUSB2PRM
    pci::WriteConfReg(xhc_dev, 0xd0, ehci2xhci_ports); // XUSB2PR
    Log(kDebug, "SwitchEhci2Xhci: SS = %02, xHCI = %02x\n",
    superspeed_ports, ehci2xhci_ports);
}

struct Message {
    enum Type {
        kInterruptXHCI,
    } type;
};

/**
 * @brief 외부 인터럽트 발생에 따른 Message객체를 저장하는 FIFO
 */
ArrayQueue<Message>* main_queue;

__attribute__((interrupt)) 
void IntHandlerXHCI(InterruptFrame* frame) {
    auto err = main_queue->Push(Message{Message::kInterruptXHCI});
    if (err) {
        Log(kInfo, "Interrupt dismissed %s at %s:%d\n", err.Name(), err.File(), err.Line());
    }
    
    NotifyEndOfInterrupt();
}

/**
 * @brief 커널 콜에 사용될 스택
 */
alignas(16) uint8_t kernel_main_stack[1024 * 1024];

/**
 * @brief 커널의 엔트리포인트 
 * @param frame_buffer_config FrameBufferConfig 타입, BIOS로부터 받아온 프레임 버퍼와 그 정보 
 * @param memory_map MemoryMap 타입, UEFI로부터 받아온 메모리 맵 정보
 */
extern "C" void KernelMainNewStack(const FrameBufferConfig& frame_buffer_config_ref, const MemoryMap& memory_map_ref) {
    FrameBufferConfig frame_buffer_config{frame_buffer_config_ref};
    MemoryMap memory_map{memory_map_ref};
    
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

    DrawDesktop(*pixel_writer);

    console = new(console_buf) Console(kDesktopFGColor, kDesktopBGColor);
    console->SetWriter(pixel_writer);
    printk("Resolution : %d x %d\n", pixel_writer->Width(), pixel_writer->Height());
    SetLogLevel(kInfo);

    InitializeLAPICTimer();

    SetupSegments();
    const uint16_t kernel_cs = 1 << 3;
    const uint16_t kernel_ss = 2 << 3;
    SetDSAll(0);
    SetCSSS(kernel_cs, kernel_ss);

    SetupIdentityPageTable();

    ::memory_manager = new(memory_manager_buf) BitmapMemoryManager;

    const auto memory_map_base = reinterpret_cast<uintptr_t>(memory_map.buffer);
    uintptr_t available_end = 0;
    for (uintptr_t iter = memory_map_base;
            iter < memory_map_base + memory_map.map_size;
            iter += memory_map.descriptor_size) {
        auto desc = reinterpret_cast<const MemoryDescriptor*>(iter);
        if (available_end < desc->physical_start) {
            memory_manager->MarkAllocated(
                FrameID{available_end / kBytesPerFrame},
                (desc->physical_start - available_end) / kBytesPerFrame);
            Log(kInfo, "Page [N/A] : 0x%08x (%d pages)\n", desc->physical_start, desc->number_of_pages);
        }

        const auto physical_end =
            desc->physical_start + desc->number_of_pages * kUEFIPageSize;
        if (IsAvailable(static_cast<MemoryType>(desc->type))) {
            available_end = physical_end;
            Log(kInfo, "Page [Available] : 0x%08x (%d pages)\n", desc->physical_start, desc->number_of_pages);
        } else {
            memory_manager->MarkAllocated(
                FrameID{desc->physical_start / kBytesPerFrame},
                desc->number_of_pages * kUEFIPageSize / kBytesPerFrame);
            Log(kInfo, "Page [Reserved] : 0x%08x (%d pages)\n", desc->physical_start, desc->number_of_pages);
        }
    }
    memory_manager->SetMemoryRange(FrameID{1}, FrameID{available_end / kBytesPerFrame});

    FrameID frame_id_1 = memory_manager->Allocate(10).value;
    Log(kInfo, "Allocated : 0x%08x (%d pages)\n", frame_id_1.ID() * 4_KiB, 10);
    FrameID frame_id_2 = memory_manager->Allocate(10).value;
    Log(kInfo, "Allocated : 0x%08x (%d pages)\n", frame_id_2.ID() * 4_KiB, 10);

    if (auto err = InitializeHeap(*memory_manager)) {
        Log(kError, "failed to allocate pages: %s at %s:%d\n",
            err.Name(), err.File(), err.Line());
        exit(1);
    }

    // while (true) {
    //     __asm__("hlt");
    // }

    {
        auto err = pci::ScanAllBus();
        Log(kInfo, "ScanAllBus: %s\n", err.Name());
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

    const uint16_t cs = GetCS();
    SetIDTEntry(idt[InterruptVector::kXHCI], MakeIDTAttr(DescriptorType::kInterruptGate, 0),
        reinterpret_cast<uint64_t>(IntHandlerXHCI), cs);
    Log(kInfo, "Vector : %d, Interrupt descriptor with cs 0x%02x\n", InterruptVector::kXHCI, cs);
    
    LoadIDT(sizeof(idt) - 1, reinterpret_cast<uint64_t>(&(idt[0])));
    Log(kInfo, "IDT : 0x%08x Loaded\n", reinterpret_cast<uintptr_t>(&(idt[0])));

    const uint8_t bsp_local_apic_id =
        (*reinterpret_cast<const uint32_t*>(0xfee00020)) >> 24;
    Log(kInfo, "XHCI Interrupt registered for Core apic id - #%d\n", bsp_local_apic_id);
    auto err = pci::ConfigureMSIFixedDestination(
        *xhc_dev, bsp_local_apic_id,
        pci::MSITriggerMode::kLevel, pci::MSIDeliveryMode::kFixed,
        InterruptVector::kXHCI, 0
    );
    if (err) {
        Log(kError, "failed to config msi: %s at %s:%d\n", err.Name(), err.File(), err.Line());
    }

    const WithError<uint64_t> xhc_bar = pci::ReadBar(*xhc_dev, 0);
    Log(kInfo, "ReadBar : %s\n", xhc_bar.error.Name());
    // xHC 64bit PCI BAR(0~1)
    const uint64_t xhc_mmio_base = xhc_bar.value & ~static_cast<uint64_t>(0xf);
    Log(kInfo, "xHC mmio_base = %08lx\n", xhc_mmio_base);
    
    usb::xhci::Controller xhc{xhc_mmio_base};
    ::xhc = &xhc;

    if (0x8096 == pci::ReadVendorId(xhc_dev->bus, xhc_dev->device, xhc_dev->function)) {
        SwitchEhci2Xhci(*xhc_dev);
    }
    {
        auto err = xhc.Initialize();
        Log(kInfo, "xhc.Initialize: %s\n", err.Name());
    }
    Log(kInfo, "xHC start\n");
    xhc.Run();

    usb::HIDMouseDriver::default_observer = MouseObserver;

    for (int i = 1; i <= xhc.MaxPorts(); ++i) {
        auto port = xhc.PortAt(i);
        Log(kInfo, "Port %d: IsConnected=%d\n", i, port.IsConnected());

        if (port.IsConnected()) {
            if (auto err = ConfigurePort(xhc, port)) {
                Log(kError, "failed to configure port: %s at %s:%d\n", err.Name(), err.File(), err.Line());
                continue;
            }
        }
    }

    screen_size.x = frame_buffer_config.horizontal_resolution;
    screen_size.y = frame_buffer_config.vertical_resolution;
    mouse_position = {screen_size.x / 2, screen_size.y / 2};

    auto bgwindow = std::make_shared<Window>(screen_size.x, screen_size.y, frame_buffer_config.pixel_format);
    auto bgwriter = bgwindow->Writer();

    DrawDesktop(*bgwriter);

    auto mouse_window = std::make_shared<Window>(
        kMouseCursorWidth, kMouseCursorHeight, frame_buffer_config.pixel_format);
    mouse_window->SetTransparentColor(kMouseTransparentColor);
    DrawMouseCursor(mouse_window->Writer(), {0, 0});

    auto main_window = std::make_shared<Window>(
        160, 52, frame_buffer_config.pixel_format
    );
    DrawWindow(*(main_window->Writer()), "Hello Window");

    FrameBuffer screen;
    if (auto err = screen.Initialize(frame_buffer_config)) {
        Log(kError, "failed to initialize frame buffer: %s at %s:%d\n",
            err.Name(), err.File(), err.Line());
    }

    auto console_window = std::make_shared<Window>(
        Console::kColumns * 8, Console::kRows * 16, frame_buffer_config.pixel_format
    );
    console->SetWindow(console_window);

    layer_manager = new LayerManager;
    layer_manager->SetWriter(&screen);

    auto bglayer_id = layer_manager->NewLayer()
        .SetWindow(bgwindow)
        .Move({0, 0})
        .ID();
    auto main_window_layer_id = layer_manager->NewLayer()
        .SetWindow(main_window)
        .SetDraggable(true)
        .Move({300, 100})
        .ID();
    mouse_layer_id = layer_manager->NewLayer()
        .SetWindow(mouse_window)
        .Move(mouse_position)
        .ID();
    auto console_layer_id = layer_manager->NewLayer()
        .SetWindow(console_window)
        .Move({0, 0})
        .ID();
    
    console->SetLayerID(console_layer_id);

    layer_manager->UpDown(bglayer_id, 0);
    layer_manager->UpDown(console_layer_id, 1);
    layer_manager->UpDown(main_window_layer_id, 2);
    layer_manager->UpDown(mouse_layer_id, 3);
    layer_manager->Draw({{0, 0}, screen_size});

    std::array<Message, 32> main_queue_data;
    ArrayQueue<Message> main_queue{main_queue_data};
    ::main_queue = &main_queue;

    char str[128];
    uint32_t elapsed = 0;

    __asm__("sti");
    /**
     * @brief 외부 인터럽트 이벤트 루프
     */
    while (true) {
        StartLAPICTimer();

        sprintf(str, "%010u", elapsed);
        FillRectangle(*(main_window->Writer()), {24, 28}, {8 * 10, 16}, {0xc6, 0xc6, 0xc6});
        WriteString(*(main_window->Writer()), {24, 28}, str, {0, 0, 0});
        layer_manager->Draw(main_window_layer_id);

        elapsed = LAPICTimerElapsed();
        StopLAPICTimer();

        __asm__("cli");
        if (main_queue.Count() == 0) {
            // __asm__("sti\n\thlt");
            __asm__("sti");
            continue;
        }

        Message msg = main_queue.Front();
        main_queue.Pop();
        __asm__("sti");

        switch (msg.type) {
            case Message::kInterruptXHCI:
                if (auto err = ProcessEvent(xhc)) {
                    Log(kError, "Error while ProcessEvent: %s at %s:%d\n",
                        err.Name(), err.File(), err.Line());
                }
                break;
        }
    }
}

extern "C" void __cxa_pure_virtual() {
  while (1) __asm__("hlt");
}