# MikanOS project
## Day 9D~E, 스크롤처리 고속화
<br>

### 주요 개발 사항
1. `printk()`의 스크롤 시간 측정
2. `Window::Move()`를 통해 스크롤에서 발생하는 `PixelWriter::Write()`억제

<br>

### 핵심 동작 원리
1. `printk()`에 LAPIC 타이머를 활용한 시간측정을 적용
   - `MouseObserver`때와 마찬가지
   - 책과는 다르게 이 레포에서는 `logger`에도 적용하도록 함
     - 실제로 `Log()`는 `printk()`와 마찬가지로 `Console::PutString()`을 사용
     - `printk`를 `extern`으로 받아서 사용할 수 있도록 함

```cpp
// main.cpp
int printk(const char* format, ...) {
  va_list ap;
  int result;
  char s[1024];

  va_start(ap, format);
  result = vsprintf(s, format, ap);
  va_end(ap);

  StartLAPICTimer();
  console->PutString(s);
  auto elapsed = LAPICTimerElapsed();
  StopLAPICTimer();

  sprintf(s, "[%9d]", elapsed);
  console->PutString(s);

  return result;
}

// logger.cpp
extern int printk(const char* format, ...);

int Log(LogLevel level, const char* format, ...) {
  if (level > log_level) {
    return 0;
  }

  va_list ap;
  int result;
  char s[1024];

  va_start(ap, format);
  result = vsprintf(s, format, ap);
  va_end(ap);

  printk(s);

  // console->PutString(s);
  return result;
}
```

![핵심동작원리 1-1](img/9D~E-1.gif)

<br>

2. `Console::Newline()`에서 콘솔이 위치한 윈도우의 프레임 버퍼를 한 줄 크기만큼 위로 이동시킴
   - 기존의 `Newline`은 배경에 해당하는 영역을 `PixelWriter::Write()`로 칠한다음 `Console::buffer_`의 문자열 내용을 덮어씌움
     - 이 작업에서 가장 높은 라인은 사라지고 2번째 라인부터 덮어씌워짐
   - 하지만 위 작업은 윈도우가 적용된 이상 문자열을 다시 렌더링 할 필요가 없음
     - 2번째 줄에 해당하는 영역부터 마지막 줄의 영역에 해당하는 `FrameBuffer(Window::shadow_buffer_)`를 한 줄 크기만큼 위로 이동(`Move`) 시키면 됨
   - 이를 위해 `FrameBuffer::Move()`를 설계하고 `Window::Move()`가 자신의 `shadow_buffer_`에 대해 사용하게 함

```cpp
// frame_buffer.cpp
void FrameBuffer::Move(Vector2D<int> dst_pos, const Rectangle<int> &src) {
  const auto bytes_per_pixel = BytesPerPixel(config_.pixel_format);
  const auto bytes_per_scan_line = BytesPerScanLine(config_);

  if (dst_pos.y < src.pos.y) { // move up
    uint8_t *dst_buf = FrameAddrAt(dst_pos, config_);
    const uint8_t *src_buf = FrameAddrAt(src.pos, config_);
    for (int y = 0; y < src.size.y; ++y) {
      memcpy(dst_buf, src_buf, bytes_per_pixel * src.size.x);
      dst_buf += bytes_per_scan_line;
      src_buf += bytes_per_scan_line;
    }
  } else { // move down
    uint8_t *dst_buf =
      FrameAddrAt(dst_pos + Vector2D<int>{0, src.size.y - 1}, config_);
    const uint8_t *src_buf =
      FrameAddrAt(src.pos + Vector2D<int>{0, src.size.y - 1}, config_);
    for (int y = 0; y < src.size.y; ++y) {
      memcpy(dst_buf, src_buf, bytes_per_pixel * src.size.x);
      dst_buf -= bytes_per_scan_line;
      src_buf -= bytes_per_scan_line;
    }
  }
}

void Console::Newline() {
  cursor_column_ = 0;
  if (cursor_row_ < kRows - 1) {
    ++cursor_row_;
    return;
  }

  if (window_) {
    // SetWindow 이후 동작
    Rectangle<int> move_src{{0, 16}, {8 * kColumns, 16 * (kRows - 1)}};
    window_->Move({0, 0}, move_src);
    FillRectangle(*writer_, {0, 16 * (kRows - 1)}, {8 * kColumns, 16}, bg_color_);
  } else {
    // SetWindow 이전 동작
    FillRectangle(*writer_, {0, 0}, {8 * kColumns, 16 * kRows}, bg_color_);
    for (int row = 0; row < kRows - 1; ++row) {
      memcpy(buffer_[row], buffer_[row + 1], kColumns + 1);
      WriteString(*writer_, Vector2D<int>{0, 16 * row}, buffer_[row], fg_color_);
    }
    memset(buffer_[kRows - 1], 0, kColumns + 1);
  }
}
```

![핵심동작원리 2-1](img/9D~E-2.gif)

<br>

### 추가
`Console::SetWindow()`이전에 `Console::SetWriter()`를 통해 적절한 `PixelWriter`를 지정해주지 않으면 로그 출력 시에 처리되지 않는 오류가 발생할 수 있다.