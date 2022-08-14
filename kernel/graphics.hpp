#pragma once

#include <cstdint>

#include "frame_buffer_config.hpp"

struct PixelColor {
    uint8_t r, g, b;
};

class PixelWriter {
public:
    /**
     * @brief PixelWriter 오브젝트의 생성자
     * @param config FrameBufferConfig의 레퍼런스 타입, pixel_format이 enum PixelFormat에 정의되어 있어야 함
     */
    PixelWriter(const FrameBufferConfig& config) : config_{config} {}
    virtual ~PixelWriter() = default;

    virtual void Write(int x, int y, const PixelColor& c) = 0;

protected:
    /**
     * @brief 1개의 픽셀에 대한 프레임 버퍼 상의 주소를 가져옴
     * @param x, y 값을 읽을 픽셀의 열과 행
     * @return uint8_t* 프레임 버퍼 상의 주소
     */
    uint8_t* PixelAt(int x, int y);

private:
    const FrameBufferConfig& config_;
};

class RGBResv8BitPerColorPixelWriter : public PixelWriter {
public:
    using PixelWriter::PixelWriter;

    /**
     * @brief 1개의 픽셀을 RGB값으로 설정
     * @param x, y 값을 쓸 픽셀의 열과 행
     * @param c PixelColor 픽셀의 R,G,B 값
     */
    virtual void Write(int x, int y, const PixelColor& c) override;
};

class BGRResv8BitPerColorPixelWriter : public PixelWriter {
public:
    using PixelWriter::PixelWriter;

    /**
     * @brief 1개의 픽셀을 RGB값으로 설정
     * @param x, y 값을 쓸 픽셀의 열과 행
     * @param c PixelColor 픽셀의 R,G,B 값
     */
    virtual void Write(int x, int y, const PixelColor& c) override;
};