#pragma once

#include <array>
#include <limits>

#include "error.hpp"

namespace {
    constexpr unsigned long long operator""_KiB(unsigned long long kib) {
        return kib * 1024;
    }

    constexpr unsigned long long operator""_MiB(unsigned long long mib) {
        return mib * 1024_KiB;
    }

    constexpr unsigned long long operator""_GiB(unsigned long long gib) {
        return gib * 1024_MiB;
    }
}

/** @brief 물리적 메모리 프레임 1개의 크기(바이트) */
static const auto kBytesPerFrame{4_KiB};

class FrameID {
public:
    explicit FrameID(size_t id) : id_{id} {}
    size_t ID() const { return id_; }
    void* Frame() const { return reinterpret_cast<void*>(id_ * kBytesPerFrame); }

    private:
    size_t id_;
};

static const FrameID kNullFrame{std::numeric_limits<size_t>::max()};

/** @brief 비트 맵 배열을 사용하여 프레임 단위로 메모리를 관리하는 클래스.
 * 1 비트를 1 프레임에 대응시켜, 비트 맵에 의해 빈 프레임을 관리한다.
 * 배열 alloc_map의 각 비트는 프레임에 해당하며 0이면 비어 있고 1이면 사용 중입니다.
 * alloc_map[n] 의 m 비트째가 대응하는 물리 어드레스는 다음 식으로 구해진다:
 * kFrameBytes * (n * kBitsPerMapLine + m)
 */
class BitmapMemoryManager {
public:
    /** @brief 이 메모리 관리 클래스에서 취급할 수 있는 최대의 물리적 메모리량(바이트) */
    static const auto kMaxPhysicalMemoryBytes{128_GiB};
    /** @brief kMaxPhysicalMemoryBytes까지의 실제 메모리를 처리하는 데 필요한 프레임 수 */
    static const auto kFrameCount{kMaxPhysicalMemoryBytes / kBytesPerFrame};

    /** @brief 비트맵 배열의 요소형 */
    using MapLineType = unsigned long;
    /** @brief 비트맵 배열의 한 요소의 비트 수 == 프레임 수 */
    static const size_t kBitsPerMapLine{8 * sizeof(MapLineType)};

    /** @brief 인스턴스를 초기화합니다. */
    BitmapMemoryManager();

    /** @brief 요청한 프레임 수의 공간을 확보하고 첫 번째 프레임 ID를 반환합니다. */
    WithError<FrameID> Allocate(size_t num_frames);
    Error Free(FrameID start_frame, size_t num_frames);
    void MarkAllocated(FrameID start_frame, size_t num_frames);

    /** @brief 이 메모리 관리자가 처리 할 메모리 범위를 설정합니다.
     * 이 호출 이후 Allocate에 의한 메모리 할당은 설정된 범위 내에서만 수행됩니다.
     *
     * @param range_begin_ 메모리 범위의 시작점
     * @param range_end_ 메모리 범위의 끝. 최종 프레임의 다음 프레임.
    */
    void SetMemoryRange(FrameID range_begin, FrameID range_end);

private:
    std::array<MapLineType, kFrameCount / kBitsPerMapLine> alloc_map_;
    /** @brief 메모리 관리자가 처리하는 메모리 범위의 시작점. */
    FrameID range_begin_;
    /** @brief 메모리 관리자가 처리하는 메모리 범위의 끝점. 최종 프레임의 다음 프레임. */
    FrameID range_end_;

    bool GetBit(FrameID frame) const;
    void SetBit(FrameID frame, bool allocated);
};

Error InitializeHeap(BitmapMemoryManager& memory_manager);