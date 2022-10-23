# MikanOS project
## Day 7B, 이벤트 루프를 활용한 인터럽트 핸들링 고속화
<br>

### 주요 개발 사항
1. 인터럽트 이벤트와 대응되는 메시지 구조체와 큐(FIFO) 구현
2. 인터럽트 핸들러가 큐에 메시지를 삽입하도록 변경
3. 메인에서 이벤트 큐에서 메시지를 추출하여 핸들링하는 루프 구현

<br>

### 핵심 동작 원리
1. 이벤트 큐의 구현과 인터럽트 핸들러 재구성
   - 이벤트 큐는 인터럽트가 발생할 때 마다 메세지를 받는 역할을 함
     - `circular queue`형태의 FIFO 자료구조 구현
     - 고정 크기 배열을 활용하므로 `Push`연산에서 오버플로우가 발생할 수 있음
       - 이 오버플로우는 실제 이벤트 루프에서 1회 이벤트를 처리하는 동안, 정해진 크기를 넘어서는 인터럽트 핸들러가 호출되었음을 의미할 것임(?)
   - 인터럽트 핸들러로 진입하면, 해당 인터럽트의 처리동안 다른 인터럽트를 무시하게 됨
     - 인터럽트 핸들러는 최대한 빠르게 끝나야 함
     - 인터럽트 핸들러는 벡터에 해당하는 메세지를 큐에 삽입하는 동작만 하도록 구현할 수 있음

```cpp
struct Message {
  enum Type {
    kInterruptXHCI,
  } type;
};

/**
 * @brief 외부 인터럽트 발생에 따른 Message객체를 저장하는 FIFO
 */
ArrayQueue<Message>* main_queue;

__attribute__((interrupt)) 
void IntHandlerXHCI(InterruptFrame* frame) {
  auto err = main_queue->Push(Message{Message::kInterruptXHCI});  // <-- 메세지를 큐에 삽입하는 동작만 하도록 구현
  if (err) {
    Log(kInfo, "Interrupt dismissed %s at %s:%d\n", err.Name(), err.File(), err.Line());
  }
  
  NotifyEndOfInterrupt();
}

std::array<Message, 32> main_queue_data;  // 인터럽트 메시지가 이벤트 루프에 의해 처리되기 전까지 최대 32개만큼 적재가능
ArrayQueue<Message> main_queue{main_queue_data};
::main_queue = &main_queue;

```

<br>

2. 이벤트 루프의 구현
   - 인터럽트의 발생을 이벤트 큐로부터 폴링(Polling)하는 루프 구현
     - `HLT`명령은 "인터럽트가 발생할 때 까지" 코어를 멈춤
       - 즉 인터럽트가 발생하게 되면 다음 명령의 수행이 시작됨
       - 이를 이용하여 효율적인 폴링 루프를 구현할 수 있음
   - 인터럽트 핸들러로의 분기는 어떤 프로시저에 있어서 정상적인 루틴이 아님
     - 이는 `main_queue`라는 공유자원에 대한 "data race"가 발생할 수 있음을 의미함
     - 즉, 어느 쪽이든 `main_queue`가 점유되고 있으면 대기해야함
       - 이벤트 루프 쪽은 인터럽트 핸들러가 리턴될 때까지 실행될 일이 없음
       - 하지만 인터럽트 핸들러는 이벤트 루프 진행 중 어디에서도 호출 될 수 있음
       - 따라서, 루프가 `main_queue`에 대한 작업을 하는 동안은 인터럽트가 비활성화 되어야 함(`IF=0`)

```cpp
__asm__("sti");
/**
 * @brief 외부 인터럽트 이벤트 루프
 */
while (true) {
  __asm__("cli");  // <------------ Lock : 외부 인터럽트 비활성화
  if (main_queue.Count() == 0) {  // critical
    __asm__("sti\n\thlt");  // <--- Unlock : 외부 인터럽트 활성화
    continue;
  }

  Message msg = main_queue.Front(); // critical
  main_queue.Pop(); // critical
  __asm__("sti");  // <------------ Unlock : 외부 인터럽트 활성화

  switch (msg.type) {
    case Message::kInterruptXHCI:
      if (auto err = ProcessEvent(xhc)) {
        Log(kError, "Error while ProcessEvent: %s at %s:%d\n",
          err.Name(), err.File(), err.Line());
      }
      break;
  }
}
```

<br>

### 주요 동작
실제로 인터럽트 이벤트 루프 내의 처리에 강제 딜레이를 주어도 마우스 이벤트가 생략되지는 않음.
그 이유는 `ProcessEvent`에서 찾을 수 있는데, 구현된 XHC 드라이버가 발생된 이벤트를 링에 저장하고 있고, 
`ProcessEvent`는 링에서 현 시점까지 발생한 모든 이벤트를 처리하기 때문임.

```cpp
Error ProcessEvent(Controller& xhc) {
  if (!xhc.PrimaryEventRing()->HasFront()) {
    return MAKE_ERROR(Error::kSuccess);
  }

  Error err = MAKE_ERROR(Error::kNotImplemented);
  auto event_trb = xhc.PrimaryEventRing()->Front();
  if (auto trb = TRBDynamicCast<TransferEventTRB>(event_trb)) {
    err = OnEvent(xhc, *trb);
  } else if (auto trb = TRBDynamicCast<PortStatusChangeEventTRB>(event_trb)) {
    err = OnEvent(xhc, *trb);
  } else if (auto trb = TRBDynamicCast<CommandCompletionEventTRB>(event_trb)) {
    err = OnEvent(xhc, *trb);
  }
  xhc.PrimaryEventRing()->Pop();

  return err;
}
```

동작 내용은 크게 바뀐 부분이 없음.