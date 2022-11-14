
#pragma once

#include <memory>
#include <map>
#include <vector>

#include "graphics.hpp"
#include "window.hpp"

/** @brief Layer 는 1 개의 층을 나타낸다.
 *
 * 현재 상태에서는 하나의 창만 유지할 수 있는 설계이지만,
 * 미래에는 여러 개의 창을 가질 수 있습니다.
 */
class Layer {
public:
    /** @brief 지정된 ID를 가진 레이어를 생성합니다. */
    Layer(unsigned int id = 0);
    /** @brief 이 인스턴스의 ID를 반환합니다. */
    unsigned int ID() const;

    /** @brief 창을 설정합니다. 기존 창은 이 레이어에서 벗어난다. */
    Layer& SetWindow(const std::shared_ptr<Window>& window);
    /** @brief 설정된 창을 반환합니다. */
    std::shared_ptr<Window> GetWindow() const;

    /** @brief 레이어의 위치 정보를 지정된 절대 좌표로 업데이트합니다. 다시 그리지는 않습니다. */
    Layer& Move(Vector2D<int> pos);
    /** @brief 레이어의 위치 정보를 지정된 상대 좌표로 업데이트합니다. 다시 그리지는 않습니다. */
    Layer& MoveRelative(Vector2D<int> pos_diff);

    /** @brief writer 에 현재 설정되어 있는 윈도우의 내용을 렌더링 한다. */
    void DrawTo(PixelWriter& writer) const;

private:
    unsigned int id_;
    Vector2D<int> pos_;
    std::shared_ptr<Window> window_;
};

/** @brief LayerManager는 여러 레이어를 관리합니다. */
class LayerManager {
public:
    /** @brief Draw 메소드 등으로 묘화할 때의 writer를 설정한다. */
    void SetWriter(PixelWriter* writer);
    /** @brief 새 레이어를 생성하고 참조를 반환합니다.
     *
     * 새로 생성된 레이어의 실체는 LayerManager 내부의 컨테이너에 보관 유지된다.
     */
    Layer& NewLayer();

    /** @brief 현재 표시 상태에 있는 레이어를 그립니다. */
    void Draw() const;

    /** @brief 레이어의 위치 정보를 지정된 절대 좌표로 업데이트합니다. 다시 그리지는 않습니다. */
    void Move(unsigned int id, Vector2D<int> new_position);
    /** @brief 레이어의 위치 정보를 지정된 상대 좌표로 업데이트합니다. 다시 그리지는 않습니다. */
    void MoveRelative(unsigned int id, Vector2D<int> pos_diff);

    /** @brief 레이어의 높이 방향 위치를 지정된 위치로 이동합니다.
     *
     * new_height에 음의 높이를 지정하면 레이어가 숨겨집니다.
     * 0 이상을 지정하면 그 높이가 된다.
     * 현재의 레이어수 이상의 수치를 지정했을 경우는 최전면의 레이어가 된다.
     * */
    void UpDown(unsigned int id, int new_height);
    /** @brief 레이어를 숨깁니다. */
    void Hide(unsigned int id);

private:
    PixelWriter* writer_{nullptr};
    std::vector<std::unique_ptr<Layer>> layers_{};
    std::vector<Layer*> layer_stack_{};
    unsigned int latest_id_{0};

    Layer* FindLayer(unsigned int id);
};

extern LayerManager* layer_manager;