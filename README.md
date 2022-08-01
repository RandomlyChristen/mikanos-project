# MikanOS project
## Day 3D, 부트로더 예외처리
<br>

### 주요 개발 사항
1. 부트로더에서 `Halt`되는 예외처리

<br>

### 핵심 동작 원리
1. `EFI_STATUS`와 `EFI_ERROR` API를 활용해 예외처리

```c
void Halt(void) {
    while (1) __asm__("hlt");
}

EFI_STATUS status = provider->Service(provider, ...);
if (EFI_ERROR(status)) {
  Print(L"Error occured!\n");
  Halt();
}
```