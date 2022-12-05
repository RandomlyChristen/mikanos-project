#include "interrupt.hpp"

#include <array>

#include "asmfunc.h"
#include "segment.hpp"
#include "logger.hpp"

std::array<InterruptDescriptor, 256> idt;

void SetIDTEntry(
    InterruptDescriptor& desc,
    InterruptDescriptorAttribute attr,
    uint64_t offset,
    uint16_t segment_selector
) {
    desc.attr = attr;
    desc.offset_low = offset & 0xffffu;
    desc.offset_middle = (offset >> 16) & 0xffffu;
    desc.offset_high = offset >> 32;
    desc.segment_selector = segment_selector;
}


void NotifyEndOfInterrupt() {
    volatile auto end_of_interrupt = reinterpret_cast<uint32_t*>(0xfee000b0);
    *end_of_interrupt = 0;
}

namespace {
    std::deque<Message>* msg_queue;

    __attribute__((interrupt))
    void IntHandlerXHCI(InterruptFrame* frame) {
        msg_queue->push_back(Message{Message::kInterruptXHCI});
        NotifyEndOfInterrupt();
    }
}

void InitializeInterrupt(std::deque<Message>* msg_queue_) {
    ::msg_queue = msg_queue_;

    SetIDTEntry(idt[InterruptVector::kXHCI],
                MakeIDTAttr(DescriptorType::kInterruptGate, 0),
                reinterpret_cast<uint64_t>(IntHandlerXHCI),
                kKernelCS);
    Log(kInfo, "Vector : %d, Interrupt descriptor with cs 0x%02x\n", InterruptVector::kXHCI, kKernelCS);

    LoadIDT(sizeof(idt) - 1, reinterpret_cast<uintptr_t>(&idt[0]));
    Log(kInfo, "IDT : 0x%08x Loaded\n", reinterpret_cast<uintptr_t>(&(idt[0])));
}