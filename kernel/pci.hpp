#pragma once

#include <cstdint>
#include <array>

#include "error.hpp"

namespace pci {
    const uint16_t kConfigAddress = 0x0cf8u;
    const uint16_t kConfigData = 0x0cfcu;

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
     * @return uint32_t 클래스 코드
     * - 31:24 : 베이스 클래스
     * - 23:16 : 서브 클래스
     * - 15:8 : 인터페이스
     * - 7:0 : 리비전 ID
     */
    uint32_t ReadClassCode(uint8_t bus, uint8_t device, uint8_t function);

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
     * @brief PCI 디바이스 설정정보
     */
    struct Device {
        uint8_t bus, device, function, header_type;
    };

    inline std::array<Device, 32> devices;
    inline int num_device;

    /**
     * @brief 모든 버스를 재귀적으로 탐색하고 유효한 장치를 devices에 추가하는 함수
     * @return Error 오류여부
     */
    Error ScanAllBus();
}