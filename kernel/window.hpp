#pragma once

#include <optional>
#include <vector>

#include "frame_buffer.hpp"
#include "graphics.hpp"

/**
 * @brief Window 클래스는 그래픽의 표시 영역을 나타낸다.
 *
 * 타이틀이나 메뉴가 있는 윈도우 뿐만이 아니라, 마우스 커서의 표시 영역등도
 * 대상으로 한다.
 */
class Window {
  public:
    /** @brief WindowWriter는 Window와 관련된 PixelWriter를 제공한다. */
    class WindowWriter : public PixelWriter {
      public:
        WindowWriter(Window &window) : window_{window} {}
        /** @brief 지정된 위치에 지정된 색 그리기 */
        virtual void Write(Vector2D<int> pos, const PixelColor& c) override {
            window_.Write(pos, c);
        }
        /** @brief Width 는 Window 의 가로폭을 픽셀 단위로 돌려준다. */
        virtual int Width() const override { return window_.Width(); }
        /** @brief Height 는 Window 의 높이를 픽셀 단위로 돌려준다. */
        virtual int Height() const override { return window_.Height(); }

      private:
        Window &window_;
    };

    /** @brief 지정된 픽셀 수의 평면 그리기 영역을 만듭니다. */
    Window(int width, int height, PixelFormat shadow_format);
    ~Window() = default;
    Window(const Window &rhs) = delete;
    Window &operator=(const Window &rhs) = delete;

    /** @brief 지정된 FrameBuffer 에 이 윈도우의 표시 영역을 렌더링 한다.
     *
     * @param dst 그리기
     * @param position writer의 좌상을 기준으로 한 드로잉 위치
     */
    void DrawTo(FrameBuffer &dst, Vector2D<int> position);

    /** @brief 투명 색상을 설정합니다. */
    void SetTransparentColor(std::optional<PixelColor> c);

    /** @brief 이 인스턴스에 붙은 WindowWriter 를 취득한다. */
    WindowWriter *Writer();

    /** @brief 指定した位置にピクセルを書き込む。 */
    void Write(Vector2D<int> pos, PixelColor c);

    /** @brief 지정된 위치의 픽셀을 반환합니다. */
    const PixelColor& At(Vector2D<int> pos) const;

    /** @brief 평면 묘화 영역의 가로폭을 픽셀 단위로 돌려준다. */
    int Width() const;

    /** @brief 평면 드로잉 영역의 높이를 픽셀 단위로 반환합니다. */
    int Height() const;

  private:
    int width_, height_;
    std::vector<std::vector<PixelColor>> data_{};
    WindowWriter writer_{*this};
    std::optional<PixelColor> transparent_color_{std::nullopt};

    FrameBuffer shadow_buffer_{};
};