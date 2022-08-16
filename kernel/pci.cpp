#include "pci.hpp"

#include "asmfunc.h"

namespace {
    using namespace pci;

    /**
     * @brief PCI Configuration Address를 조합하여 32비트 주소값을 만드는 함수
     * @return uint32_t 주소값
     * - 31 : Enable(1)
     * - 30:24 : 예약 공간(0)
     * - 23:16 : 버스 번호(0~255)
     * - 15:11 : 디바이스 번호(0~31)
     * - 10:8 : 펑션 번호(0~31)
     * - 7:0 : 4바이트 단위 PCI 설정공간 레지스터 오프셋(0x00~0xFF)
     */
    uint32_t MakeAddress(uint8_t bus, uint8_t device, uint8_t function, uint8_t reg_addr) {
        auto shl = [] (uint32_t x, unsigned int bits) {
            return x << bits;
        };

        return shl(1, 31) | shl(bus, 16) | shl(device, 11) | shl(function, 8) | (reg_addr & 0xfcu);
    }

    Error AddDevice(uint8_t bus, uint8_t device, uint8_t function, uint8_t header_type) {
        if (num_device == devices.size()) {
            return Error::kFull;
        }

        devices[num_device] = Device{bus, device, function, header_type};
        ++num_device;
        return Error::kSuccess;
    }

    /**
     * @brief 주어진 버스로부터 각 디바이스를 찾는 함수
     * @param bus PCI 버스 번호, 호스트 브리지에 대해서 이 번호는 펑션 번호와 같음
     */
    Error ScanBus(uint8_t bus);

    /**
     * @brief Vendor ID가 이미 검증된(이 함수 이후로는 검증하지 않는다) 주어진 펑션 및 설정정보로부터 찾은 디바이스를 저장함,
     * 만약 PCI-PCI 브릿지를 찾은 경우에는 재귀적으로 다운스트림 방향 버스(서브 버스) 탐색함
     */
    Error ScanFunction(uint8_t bus, uint8_t device, uint8_t function) {
        auto header_type = ReadHeaderType(bus, device, function);
        if (auto error = AddDevice(bus, device, function, header_type)) {
            return error;
        }

        auto class_code = ReadClassCode(bus, device, function);
        uint8_t base = (class_code >> 24) & 0xffu;
        uint8_t sub = (class_code >> 16) & 0xffu;

        if (base == 0x06u && sub == 0x04u) {
            auto bus_numbers = ReadBusNumbers(bus, device, function);
            uint8_t secondary_bus = (bus_numbers >> 8) & 0xffu;
            return ScanBus(secondary_bus);
        }

        return Error::kSuccess;
    }

    /**
     * @brief 0번 펑션에 대해 이미 검증된(이 함수 이후로는 검증하지 않는다) 주어진 디바이스의 모든 펑션(0을 포함하여)을 탐색함,
     * 0번 펑션이 아닌 모든 펑션에 대해서는 Vendor ID를 검증함
     */
    Error ScanDevice(uint8_t bus, uint8_t device) {
        if (auto err = ScanFunction(bus, device, 0)) {
            return err;
        }
        if (IsSingleFunctionDevice(ReadHeaderType(bus, device, 0))) {
            return Error::kSuccess;
        }

        // TODO 이 부분이 책과 다름, 펑션 번호는 연속적이지 않고 31까지 값을 가질 수 있는데 왜 <8로 설정??
        for (uint8_t function = 1; function < 32; ++function) {  
            if (ReadVendorId(bus, device, function) == 0xffffu) {
                continue;
            }
            if (auto err = ScanFunction(bus, device, function)) {
                return err;
            }
        }

        return Error::kSuccess;
    }

    /**
     * @brief 주어진 버스의 모든 디바이스를 탐색하여(0~31번) Vendor ID 검증 후 해당 디바이스를 탐색함 
     */
    Error ScanBus(uint8_t bus) {
        for (uint8_t device = 0; device < 32; ++device) {
            if (ReadVendorId(bus, device, 0) == 0xffffu) {
                continue;
            }
            if (auto err = ScanDevice(bus, device)) {
                return err;
            }
        }
        return Error::kSuccess;
    }
}

namespace pci {
    void WriteAddress(uint32_t address) {
        IoOut32(kConfigAddress, address);
    }

    void WriteData(uint32_t value) {
        IoOut32(kConfigData, value);
    }

    uint32_t ReadData() {
        return IoIn32(kConfigData);
    }

    uint16_t ReadVendorId(uint8_t bus, uint8_t device, uint8_t function) {
        WriteAddress(MakeAddress(bus, device, function, 0x00u));
        return ReadData() & 0xffffu;
    }

    uint16_t ReadDeviceId(uint8_t bus, uint8_t device, uint8_t function) {
        WriteAddress(MakeAddress(bus, device, function, 0x00u));
        return (ReadData() >> 16) & 0xffffu;
    }

    uint8_t ReadHeaderType(uint8_t bus, uint8_t device, uint8_t function) {
        WriteAddress(MakeAddress(bus, device, function, 0x0cu));
        return (ReadData() >> 16) & 0xffu;
    }

    uint32_t ReadClassCode(uint8_t bus, uint8_t device, uint8_t function) {
        WriteAddress(MakeAddress(bus, device, function, 0x08u));
        return ReadData();
    }

    uint32_t ReadBusNumbers(uint8_t bus, uint8_t device, uint8_t function) {
        WriteAddress(MakeAddress(bus, device, function, 0x18u));
        return ReadData();
    }

    bool IsSingleFunctionDevice(uint8_t header_type) {
        return (header_type & 0x80u) == 0;
    }

    Error ScanAllBus() {
        num_device = 0;

        auto header_type = ReadHeaderType(0, 0, 0);
        if (IsSingleFunctionDevice(header_type)) {
            return ScanBus(0);
        }

        for (uint8_t function = 1; function < 32; ++function) {
            if (ReadVendorId(0, 0, function) == 0xffffu) {
                continue;
            }
            if (auto err = ScanBus(function)) {
                return err;
            }
        }
        
        return Error::kSuccess;
    }
}