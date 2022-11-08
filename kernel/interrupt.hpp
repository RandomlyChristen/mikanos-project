#pragma once

#include <array>
#include <cstdint>

#include "x86_descriptor.hpp"

/**
 * @brief 인터럽트 기술자 속성, type으로 인터럽트와 트랩을 구분함. DPL은 인터럽트가 핸들링 되는 권한레벨
 */
union InterruptDescriptorAttribute {
    uint16_t data;
    struct {
        uint16_t interrupt_stack_table : 3;
        uint16_t : 5;
        DescriptorType type : 4;
        uint16_t : 1;
        uint16_t descriptor_privilege_level : 2;
        uint16_t present : 1;
    } __attribute__((packed)) bits;
} __attribute__((packed));

/**
 * @brief 인터럽트 기술자, 각 크기에 해당하는 비트필드로 인터럽트 속성과 핸들러 분기주소를 표현하는 16바이트 자료형
 */
struct InterruptDescriptor {
    uint16_t offset_low;
    uint16_t segment_selector;
    InterruptDescriptorAttribute attr;
    uint16_t offset_middle;
    uint32_t offset_high;
    uint32_t reserved;
} __attribute__((packed));

/**
 * @brief 인터럽트 기술자 테이블(IDT)
 */
extern std::array<InterruptDescriptor, 256> idt;

/**
 * @brief 주어진 정보를 이용해 비트필드를 설정하여 인터럽트 기술자 속성을 리턴
 * @param type 인터럽트 기술자가 인터럽트인지 트랩인지 등
 * @param descriptor_privilege_level 인터럽트가 핸들링되는 권한레벨 (0, kernel ~ 3, user)
 * @param present 
 * @param interrupt_stack_table 
 * @return constexpr InterruptDescriptorAttribute 
 */
constexpr InterruptDescriptorAttribute MakeIDTAttr(
    DescriptorType type,
    uint8_t descriptor_privilege_level,
    bool present = true,
    uint8_t interrupt_stack_table = 0
) {
    InterruptDescriptorAttribute attr{};
    attr.bits.interrupt_stack_table = interrupt_stack_table;
    attr.bits.type = type;
    attr.bits.descriptor_privilege_level = descriptor_privilege_level;
    attr.bits.present = present;
    return attr;
}

/**
 * @brief desc가 레퍼런스하고 있는 IDT 위치에 인터럽트 기술자를 등록
 * @param desc IDT에 인터럽트 벡터를 이용해 접근한 기술자 레퍼런스
 * @param attr InterruptDescriptorAttribute
 * @param offset 인터럽트 핸들러 분기 주소
 * @param segment_selector 세그먼트 셀렉터
 */
void SetIDTEntry(
    InterruptDescriptor& desc, InterruptDescriptorAttribute attr, 
    uint64_t offset, uint16_t segment_selector);

class InterruptVector {
public:
    enum Number {
        kXHCI = 0x40,
    };
};

struct InterruptFrame {
    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
    uint64_t rsp;
    uint64_t ss;
};

void NotifyEndOfInterrupt();
