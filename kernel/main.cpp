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


char pixel_writer_buf[sizeof(RGBResv8BitPerColorPixelWriter)];
PixelWriter* pixel_writer;

char console_buf[sizeof(Console)];
Console* console;

unsigned int mouse_layer_id;

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

void MouseObserver(int8_t displacement_x, int8_t displacement_y) {
    layer_manager->MoveRelative(mouse_layer_id, {displacement_x, displacement_y});
    StartLAPICTimer();
    layer_manager->Draw();
    auto elapsed = LAPICTimerElapsed();
    StopLAPICTimer();
    printk("MouseObserver: elapsed = %u\n", elapsed);
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

    console = new(console_buf) Console(pixel_writer, kDesktopFGColor, kDesktopBGColor);
    printk("Resolution : %d x %d\n", pixel_writer->Width(), pixel_writer->Height());
    SetLogLevel(kError);

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

    const int kFrameWidth = frame_buffer_config.horizontal_resolution;
    const int kFrameHeight = frame_buffer_config.vertical_resolution;

    auto bgwindow = std::make_shared<Window>(kFrameWidth, kFrameHeight);
    auto bgwriter = bgwindow->Writer();

    DrawDesktop(*bgwriter);
    console->SetWriter(bgwriter);

    auto mouse_window = std::make_shared<Window>(
        kMouseCursorWidth, kMouseCursorHeight);
    mouse_window->SetTransparentColor(kMouseTransparentColor);
    DrawMouseCursor(mouse_window->Writer(), {0, 0});

    layer_manager = new LayerManager;
    layer_manager->SetWriter(pixel_writer);

    auto bglayer_id = layer_manager->NewLayer()
        .SetWindow(bgwindow)
        .Move({0, 0})
        .ID();
    mouse_layer_id = layer_manager->NewLayer()
        .SetWindow(mouse_window)
        .Move({kFrameWidth / 2, kFrameHeight / 2})
        .ID();

    layer_manager->UpDown(bglayer_id, 0);
    layer_manager->UpDown(mouse_layer_id, 1);
    layer_manager->Draw();

    std::array<Message, 32> main_queue_data;
    ArrayQueue<Message> main_queue{main_queue_data};
    ::main_queue = &main_queue;

    __asm__("sti");
    /**
     * @brief 외부 인터럽트 이벤트 루프
     */
    while (true) {
        __asm__("cli");
        if (main_queue.Count() == 0) {
            __asm__("sti\n\thlt");
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