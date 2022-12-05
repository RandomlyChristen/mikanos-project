#pragma once

#include <array>
#include <cstdint>

#include "x86_descriptor.hpp"


union SegmentDescriptor {
    uint64_t data;
    struct {
        uint64_t limit_low : 16;
        uint64_t base_low : 16;
        uint64_t base_middle : 8;
        DescriptorType type : 4;
        uint64_t system_segment : 1;
        uint64_t descriptor_privilege_level : 2;
        uint64_t present : 1;
        uint64_t limit_high : 4;
        uint64_t available : 1;
        uint64_t long_mode : 1;
        uint64_t default_operation_size : 1;
        uint64_t granularity : 1;
        uint64_t base_high : 8;
    } __attribute__((packed)) bits;
} __attribute__((packed));

/**
 * @brief 주어진 입력으로부터 64-bit code segment 기술자를 생성
 * @param desc SegmentDescriptor
 * @param type DescriptorType
 * @param descriptor_privilege_level 0(특권)~3(유저)
 * @param base 세그먼트 베이스(32bit)
 * @param limit 세그먼트 리미트(20bit)
 */
void SetCodeSegment(
    SegmentDescriptor& desc,
    DescriptorType type,
    unsigned int descriptor_privilege_level,
    uint32_t base,
    uint32_t limit
);

const uint16_t kKernelCS = 1 << 3;
const uint16_t kKernelSS = 2 << 3;
const uint16_t kKernelDS = 0;

/**
 * @brief 주어진 입력으로부터 data segment 기술자를 생성
 * @param desc SegmentDescriptor
 * @param type DescriptorType
 * @param descriptor_privilege_level 0(특권)~3(유저)
 * @param base 세그먼트 베이스(32bit)
 * @param limit 세그먼트 리미트(20bit)
 */
void SetDataSegment(
    SegmentDescriptor& desc,
    DescriptorType type,
    unsigned int descriptor_privilege_level,
    uint32_t base,
    uint32_t limit
);

/**
 * @brief 설정된 GDT 정보를 바탕으로 LGDT를 실행
 */
void SetupSegments();

void InitializeSegmentation();