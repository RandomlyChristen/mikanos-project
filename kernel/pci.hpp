#pragma once

#include <cstdint>
#include <array>

#include "error.hpp"

namespace pci {
    const uint16_t kConfigAddress = 0x0cf8u;
    const uint16_t kConfigData = 0x0cfcu;

    struct ClassCode {
        uint8_t base, sub, interface;

        bool Match(uint8_t base_) const { 
            return base_ == base; 
        }
        bool Match(uint8_t base_, uint8_t sub_) const { 
            return Match(base_) && sub_ == sub; 
        }
        bool Match(uint8_t base_, uint8_t sub_, uint8_t interface_) const {
            return Match(base_, sub_) && interface_ == interface;
        }
    };

    /**
     * @brief PCI 디바이스 설정정보
     */
    struct Device {
        uint8_t bus, device, function, header_type;
        ClassCode class_code;
    };

    inline std::array<Device, 32> devices;
    inline int num_device;

    /**
     * @brief PCI Configuration Space - Address Port에 값을 쓰는 함수
     * @param address Address Port에 쓸 주소값
     */
    void WriteAddress(uint32_t address);

    /**
     * @brief PCI Configuration Space - Data Port에 값을 쓰는 함수
     * @param value Data Port에 쓸 값
     */
    void WriteData(uint32_t value);

    /**
     * @brief PCI Configuration Space - Data Port에서 값을 읽는 함수
     * @return uint32_t Data Port로부터 읽은 값
     */
    uint32_t ReadData();

    /**
     * @brief 주어진 PCI Configuration으로부터 vendor id를 반환하는 함수
     * @param bus PCI 버스 번호
     * @param device PCI 디바이스 번호
     * @param function PCI 펑션 번호
     * @return uint16_t vendor id
     */
    uint16_t ReadVendorId(uint8_t bus, uint8_t device, uint8_t function);

    /**
     * @brief 주어진 PCI Configuration으로부터 device id를 반환하는 함수
     * @param bus PCI 버스 번호
     * @param device PCI 디바이스 번호
     * @param function PCI 펑션 번호
     * @return uint16_t device id
     */
    uint16_t ReadDeviceId(uint8_t bus, uint8_t device, uint8_t function);

    /**
     * @brief 주어진 PCI Configuration으로부터 헤더타입을 반환하는 함수
     * @param bus PCI 버스 번호
     * @param device PCI 디바이스 번호
     * @param function PCI 펑션 번호
     * @return uint8_t PCI 헤더타입
     */
    uint8_t ReadHeaderType(uint8_t bus, uint8_t device, uint8_t function);

    /**
     * @brief 주어진 PCI Configuration으로부터 클래스 코드를 반환하는 함수
     * @param bus PCI 버스 번호
     * @param device PCI 디바이스 번호
     * @param function PCI 펑션 번호
     * @return ClassCode 클래스 코드
     */
    ClassCode ReadClassCode(uint8_t bus, uint8_t device, uint8_t function);

    /**
     * @brief 주어진 PCI Configuration으로부터 인접한 버스 번호들을 반환하는 함수
     * @param bus PCI 버스 번호 (primary)
     * @param device PCI 디바이스 번호
     * @param function PCI 펑션 번호
     * @return uint32_t 버스 번호들
     * - 23:16 : subordinate bus number
     * - 15:8 : sub bus number
     * - 7:0 : 리비전 번호
     */
    uint32_t ReadBusNumbers(uint8_t bus, uint8_t device, uint8_t function);

    bool IsSingleFunctionDevice(uint8_t header_type);

    /**
     * @brief 모든 버스를 재귀적으로 탐색하고 유효한 장치를 devices에 추가하는 함수
     * @return Error 오류여부
     */
    Error ScanAllBus();

    /**
     * @brief 주어진 인덱스에 해당하는 베이스 주소 레지스터(BAR)의 상대 주소(PCI 설정공간으로부터)를 계산
     * @param bar_index 계산할 BAR 인덱스
     * @return constexpr uint8_t BAR 주소
     */
    constexpr uint8_t CalcBarAddress(unsigned int bar_index) {
        return 0x10 + 4 * bar_index;
    }

    uint32_t ReadConfReg(const Device& dev, uint8_t reg_addr);
    void WriteConfReg(const Device& dev, uint8_t reg_addr, uint32_t value);

    /**
     * @brief 주어진 장치와 인덱스에 해당하는 베이스 주소 레지스터(BAR)의 값을 64비트로 읽는 함수
     * @param device pci::Device PCI 장치
     * @param bar_index 읽을 BAR 인덱스
     * @return WithError<uint64_t> 읽은 값과 에러(성공여부)
     */
    WithError<uint64_t> ReadBar(Device& device, unsigned int bar_index);

    union CapabilityHeader {
        uint32_t data;
        struct {
            uint32_t cap_id : 8;
            uint32_t next_ptr : 8;
            uint32_t cap : 16;
        } __attribute__((packed)) bits;
    } __attribute__((packed));

    // https://marz.utk.edu/my-courses/cosc562/pcie/#interrupts
    const uint8_t kCapabilityMSI = 0x05;
    const uint8_t kCapabilityMSIX = 0x11;

    CapabilityHeader ReadCapabilityHeader(const Device& dev, uint8_t addr);

    /**
     * @brief MSI Message Address Structure
     * 32 bit MAS : addr_64_capable=0, per_vector_mask_capable=0
     * 64 bit MAS : addr_64_capable=1, per_vector_mask_capable=0
     * 32 bit per-vec MAS : addr_64_capable=0, per_vector_mask_capable=1
     * 64 bit per-vec MAS : addr_64_capable=1, per_vector_mask_capable=1
     */
    struct MSICapability {
        union {
            uint32_t data;
            struct {
                uint32_t cap_id : 8;
                uint32_t next_ptr : 8;
                uint32_t msi_enable : 1;
                uint32_t multi_msg_capable : 3;
                uint32_t multi_msg_enable : 3;
                uint32_t addr_64_capable : 1;
                uint32_t per_vector_mask_capable : 1;
                uint32_t : 7;
            } __attribute__((packed)) bits;
        } __attribute__((packed)) header ;

        uint32_t msg_addr;
        uint32_t msg_upper_addr;
        uint32_t msg_data;
        uint32_t mask_bits;
        uint32_t pending_bits;
    } __attribute__((packed));

    Error ConfigureMSI(
        const Device& dev, uint32_t msg_addr, uint32_t msg_data,
        unsigned int num_vector_exponent);

    enum class MSITriggerMode {
        kEdge = 0,
        kLevel = 1
    };

    enum class MSIDeliveryMode {
        kFixed          = 0b000,
        kLowestPriority = 0b001,
        kSMI            = 0b010,
        kNMI            = 0b100,
        kINIT           = 0b101,
        kExtINT         = 0b111,
    };

    Error ConfigureMSIFixedDestination(
        const Device& dev, uint8_t apic_id,
        MSITriggerMode trigger_mode, MSIDeliveryMode delivery_mode,
        uint8_t vector, unsigned int num_vector_exponent);
}