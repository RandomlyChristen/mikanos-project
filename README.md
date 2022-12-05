# MikanOS project
## Day 11A 리팩토링
<br>

### 주요 개발 사항
1. main 함수가 커널의 각 컴포넌트를 초기화하는 함수를 호출하게끔 정리

<br>

```cpp
extern "C" void KernelMainNewStack(const FrameBufferConfig& frame_buffer_config_ref, const MemoryMap& memory_map_ref) {
  InitializeGraphics(frame_buffer_config_ref);
  InitializeConsole();
  
  printk("Welcome to MikanOS!\n");
  SetLogLevel(kInfo);
  
  InitializeSegmentation();
  InitializePaging();
  
  InitializeMemoryManager(memory_map_ref);
  
  ::main_queue = new std::deque<Message>(32);
  InitializeInterrupt(main_queue);
  
  InitializePCI();
  usb::xhci::Initialize();
  
  InitializeLayer();
  InitializeMainWindow();
  InitializeMouse();
  
  layer_manager->Draw({{0, 0}, ScreenSize()});
  
  char str[128];
  uint32_t elapsed = 0;

  __asm__("sti");
  /**
   * @brief 외부 인터럽트 이벤트 루프
   */
  while (true) {
    StartLAPICTimer();
    ...
  }
}
```

자세한 내용은 **Compare changes** 확인
