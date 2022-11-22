# MikanOS project
## Day 9A 마우스 개량
<br>

### 주요 개발 사항
1. 마우스가 화면 밖에서 렌더링 되는 문제의 개선

<br>

### 핵심 동작 원리
1. 프레임 버퍼 밖으로 나간 윈도우에 대해 `PixelWriter::Write`를 호출하지 않도록 개선
   - 이전까지 마우스는 `Layer::MoveRelative`를 통해 무제한 2차원 좌표로 이동이 가능했음
     - 이동된 레이어의 윈도우가 `Window::DrawTo`를 호출하는 경우
       - `transparent_color_`가 없으면 `FrameBuffer::Copy`에서 ~~렌더링 범위를 재조정하고 있기때문에 괜찮음~~
       - `transperent_color_`가 있으면 범위 밖에 값을 `PixelWriter::Write`로 직접 쓰기 때문에 위험
         - 만약 이 범위가 프레임 버퍼 안으로 재구성 가능하다면 화면 반대편에 마우스가 그려짐
   - `Window::DrawTo()`에서 좌표를 계산할 때 `writer`가 표현할 수 있는 크기를 고려하는 것으로 범위 밖에 쓰는 것을 수정
   - 

```cpp
// window.cpp
void Window::DrawTo(FrameBuffer &dst, Vector2D<int> position) {
  if (!transparent_color_) {
    dst.Copy(position, shadow_buffer_);
    return;
  }

  const auto tc = transparent_color_.value();
  auto &writer = dst.Writer();
  for (int y = std::max(0, 0 - position.y);
      y < std::min(Height(), writer.Height() - position.y); ++y) {
    for (int x = std::max(0, 0 - position.x);
        x < std::min(Width(), writer.Width() - position.x); ++x) {
      const auto c = At(Vector2D<int>{x, y});
      if (c != tc) {
        writer.Write(position + Vector2D<int>{x, y}, c);
      }
    }
  }
}

// frane_buffer.cpp
Error FrameBuffer::Copy(Vector2D<int> dst_pos, const FrameBuffer &src) {
  ...
  for (int y = dst_start.y; y < dst_end.y; ++y) {
    if ((dst_end.x - dst_start.x) < 0)  // 이 값이 음수가 되면 memcpy는 매우 큰 값을 size로 받음
      continue;
    memcpy(dst_buf, src_buf, bytes_per_pixel * (dst_end.x - dst_start.x));
    dst_buf += BytesPerScanLine(config_);
    src_buf += BytesPerScanLine(src.config_);
  }

  return MAKE_ERROR(Error::kSuccess);
}

// main.cpp
void MouseObserver(int8_t displacement_x, int8_t displacement_y) {
  // 마우스의 위치가 화면 밖으로 정의되지 않도록 함
  auto newpos = mouse_position + Vector2D<int>{displacement_x, displacement_y};
  newpos = ElementMin(newpos, screen_size + Vector2D<int>{-1, -1});
  mouse_position = ElementMax(newpos, {0, 0});

  layer_manager->Move(mouse_layer_id, mouse_position);
  layer_manager->Draw();
}
```