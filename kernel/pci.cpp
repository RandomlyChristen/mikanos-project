#include "pci.hpp"
#include "logger.hpp"
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

    Error AddDevice(const Device& device) {
        if (num_device == devices.size()) {
            return MAKE_ERROR(Error::kFull);
        }

        devices[num_device] = device;
        ++num_device;
        return MAKE_ERROR(Error::kSuccess);
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
        auto class_code = ReadClassCode(bus, device, function);
        auto header_type = ReadHeaderType(bus, device, function);
        Device dev{bus, device, function, header_type, class_code};

        if (auto error = AddDevice(dev)) {
            return error;
        }

        if (class_code.Match(0x06u, 0x04u)) {
            auto bus_numbers = ReadBusNumbers(bus, device, function);
            uint8_t secondary_bus = (bus_numbers >> 8) & 0xffu;
            return ScanBus(secondary_bus);
        }

        return MAKE_ERROR(Error::kSuccess);
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
            return MAKE_ERROR(Error::kSuccess);
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

        return MAKE_ERROR(Error::kSuccess);
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
        return MAKE_ERROR(Error::kSuccess);
    }

    MSICapability ReadMSICapability(const Device& dev, uint8_t cap_addr) {
        MSICapability msi_cap{};

        msi_cap.header.data = ReadConfReg(dev, cap_addr);
        msi_cap.msg_addr = ReadConfReg(dev, cap_addr + 4);

        uint8_t msg_data_addr = cap_addr + 8;
        if (msi_cap.header.bits.addr_64_capable) {
            msi_cap.msg_upper_addr = ReadConfReg(dev, cap_addr + 8);
            msg_data_addr = cap_addr + 12;
        }

        msi_cap.msg_data = ReadConfReg(dev, msg_data_addr);

        if (msi_cap.header.bits.per_vector_mask_capable) {
            msi_cap.mask_bits = ReadConfReg(dev, msg_data_addr + 4);
            msi_cap.pending_bits = ReadConfReg(dev, msg_data_addr + 8);
        }

        return msi_cap;
    }

    void WriteMSICapability(
        const Device& dev, uint8_t cap_addr, const MSICapability& msi_cap
    ) {
        WriteConfReg(dev, cap_addr, msi_cap.header.data);
        WriteConfReg(dev, cap_addr + 4, msi_cap.msg_addr);

        uint8_t msg_data_addr = cap_addr + 8;
        if (msi_cap.header.bits.addr_64_capable) {
            WriteConfReg(dev, cap_addr + 8, msi_cap.msg_upper_addr);
            msg_data_addr = cap_addr + 12;
        }

        WriteConfReg(dev, msg_data_addr, msi_cap.msg_data);

        if (msi_cap.header.bits.per_vector_mask_capable) {
            WriteConfReg(dev, msg_data_addr + 4, msi_cap.mask_bits);
            WriteConfReg(dev, msg_data_addr + 8, msi_cap.pending_bits);
        }
    }

    Error ConfigureMSIRegister(
        const Device& dev, uint8_t cap_addr, uint32_t msg_addr, uint32_t msg_data,
        unsigned int num_vector_exponent
    ) {
        auto msi_cap = ReadMSICapability(dev, cap_addr);

        if (msi_cap.header.bits.multi_msg_capable <= num_vector_exponent) {
            msi_cap.header.bits.multi_msg_enable = msi_cap.header.bits.multi_msg_capable;
        } 
        else {
            msi_cap.header.bits.multi_msg_enable = num_vector_exponent;
        }

        msi_cap.header.bits.msi_enable = 1;
        msi_cap.msg_addr = msg_addr;
        Log(kInfo, "Prev MSI(0x%04x) Cap.MsgData 0x%08x\n", cap_addr, msi_cap.msg_data);
        msi_cap.msg_data &= (0xffffu << 16);
        msi_cap.msg_data |= (msg_data & 0xffffu);
        Log(kInfo, "Curr MSI(0x%04x) Cap.MsgData 0x%08x\n", cap_addr, msi_cap.msg_data);

        WriteMSICapability(dev, cap_addr, msi_cap);
        // validation
        auto msi_cap_after = ReadMSICapability(dev, cap_addr);
        Log(kInfo, "MSI(0x%04x) for PCI Device(%d.%d.%d) Setting : msi_enabled(%u)\n", 
            cap_addr, dev.bus, dev.device, dev.function, msi_cap_after.header.bits.msi_enable);
        Log(kInfo, "    multi_msg_enable = %d, multi_msg_capable = %d\n", 
            msi_cap_after.header.bits.multi_msg_enable, msi_cap_after.header.bits.multi_msg_capable);
        Log(kInfo, "    msg_addr = 0x%08x, msg_data = 0x%08x\n", 
            msi_cap_after.msg_addr, msi_cap_after.msg_data);
        return MAKE_ERROR(Error::kSuccess);
    }

    Error ConfigureMSIXRegister(
        const Device& dev, uint8_t cap_addr, uint32_t msg_addr, uint32_t msg_data,
        unsigned int num_vector_exponent
    ) {
        return MAKE_ERROR(Error::kNotImplemented);
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

    ClassCode ReadClassCode(uint8_t bus, uint8_t device, uint8_t function) {
        WriteAddress(MakeAddress(bus, device, function, 0x08u));
        auto reg = ReadData();
        ClassCode cc;
        cc.base = (reg >> 24) & 0xffu,
        cc.sub = (reg >> 16) & 0xffu,
        cc.interface = (reg >> 8) & 0xffu;
        return cc;
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
        
        return MAKE_ERROR(Error::kSuccess);
    }

    uint32_t ReadConfReg(const Device& dev, uint8_t reg_addr) {
        WriteAddress(MakeAddress(dev.bus, dev.device, dev.function, reg_addr));
        return ReadData();
    }

    void WriteConfReg(const Device& dev, uint8_t reg_addr, uint32_t value) {
        WriteAddress(MakeAddress(dev.bus, dev.device, dev.function, reg_addr));
        WriteData(value);
    }

    WithError<uint64_t> ReadBar(Device& device, unsigned int bar_index) {
        if (bar_index >= 6) {
            return {0, MAKE_ERROR(Error::kIndexOutOfRange)};
        }

        const auto addr = CalcBarAddress(bar_index);
        const auto bar = ReadConfReg(device, addr);

        // 32비트 주소값인 경우
        if ((bar & 4u) == 0) {
            return {bar, MAKE_ERROR(Error::kSuccess)};
        }

        if (bar_index >= 5) {
            return {0, MAKE_ERROR(Error::kIndexOutOfRange)};
        }

        const auto bar_upper = ReadConfReg(device, addr + 4);
        return {
            bar | static_cast<uint64_t>(bar_upper) << 32,
            MAKE_ERROR(Error::kSuccess)
        };
    }

    CapabilityHeader ReadCapabilityHeader(const Device& dev, uint8_t addr) {
        CapabilityHeader header;
        header.data = pci::ReadConfReg(dev, addr);
        return header;
    }

    Error ConfigureMSI(
        const Device& dev, uint32_t msg_addr, uint32_t msg_data,
        unsigned int num_vector_exponent
    ) {
        uint8_t cap_addr = ReadConfReg(dev, 0x34) & 0xffu;
        uint8_t msi_cap_addr = 0, msix_cap_addr = 0;
        while (cap_addr != 0) {
            auto header = ReadCapabilityHeader(dev, cap_addr);
            if (header.bits.cap_id == kCapabilityMSI) {
                Log(kInfo, "MSI Capability founded at XHC PCI Cap.Pointer : 0x%04x\n", cap_addr);
                msi_cap_addr = cap_addr;
            } 
            else if (header.bits.cap_id == kCapabilityMSIX) {
                Log(kInfo, "MSIX Capability founded at XHC PCI Cap.Pointer : 0x%04x\n", cap_addr);
                msix_cap_addr = cap_addr;
            }
            cap_addr = header.bits.next_ptr;
        }

        if (msi_cap_addr) {
            return ConfigureMSIRegister(dev, msi_cap_addr, msg_addr, msg_data, num_vector_exponent);
        } else if (msix_cap_addr) {
            return ConfigureMSIXRegister(dev, msix_cap_addr, msg_addr, msg_data, num_vector_exponent);
        }
        return MAKE_ERROR(Error::kNoPCIMSI);
    }

    Error ConfigureMSIFixedDestination(
        const Device& dev, uint8_t apic_id,
        MSITriggerMode trigger_mode, MSIDeliveryMode delivery_mode,
        uint8_t vector, unsigned int num_vector_exponent
    ) {
        uint32_t msg_addr = 0xfee00000u | (apic_id << 12);
        uint32_t msg_data = (static_cast<uint32_t>(delivery_mode) << 8) | vector;
        if (trigger_mode == MSITriggerMode::kLevel) {
            msg_data |= 0xc000;
        }
        Log(kInfo, "msg_addr : 0x%08x, msg_data : 0x%08x\n", msg_addr, msg_data);
        return ConfigureMSI(dev, msg_addr, msg_data, num_vector_exponent);
    }
}