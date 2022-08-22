#pragma once

#include <array>

class Error {
public:
    enum Code {
        kSuccess,
        kFull,
        kEmpty,
        kNoEnoughMemory,
        kIndexOutOfRange,
        kHostControllerNotHalted,
        kInvalidSlotID,
        kPortNotConnected,
        kInvalidEndpointNumber,
        kTransferRingNotSet,
        kAlreadyAllocated,
        kNotImplemented,
        kInvalidDescriptor,
        kBufferTooSmall,
        kUnknownDevice,
        kNoCorrespondingSetupStage,
        kTransferFailed,
        kInvalidPhase,
        kUnknownXHCISpeedID,
        kNoWaiter,
        kLastOfCode,
    };

    Error(Code code, const char* file, int line) : code_{code}, file_{file}, line_{line} {}

    operator bool () const {
        return this->code_ != kSuccess;
    }

    const char* Name() const {
        return code_names_[static_cast<int>(this->code_)];
    }
    const char* File() const {
        return this->file_;
    }
    int Line() const {
        return this->line_;
    }

private:
    static constexpr std::array<const char*, Error::Code::kLastOfCode> code_names_{
        "kSuccess",
        "kFull",
        "kEmpty",
        "kNoEnoughMemory",
        "kIndexOutOfRange",
        "kHostControllerNotHalted",
        "kInvalidSlotID",
        "kPortNotConnected",
        "kInvalidEndpointNumber",
        "kTransferRingNotSet",
        "kAlreadyAllocated",
        "kNotImplemented",
        "kInvalidDescriptor",
        "kBufferTooSmall",
        "kUnknownDevice",
        "kNoCorrespondingSetupStage",
        "kTransferFailed",
        "kInvalidPhase",
        "kUnknownXHCISpeedID",
        "kNoWaiter",
    };

    Code code_;
    const char* file_;
    int line_;
};

/**
 * @brief 파일과 라인 정보를 담는 에러생성 메크로 함수
 */
#define MAKE_ERROR(code) Error((code), __FILE__, __LINE__)

template <class T>
struct WithError {
    T value;
    Error error;
};