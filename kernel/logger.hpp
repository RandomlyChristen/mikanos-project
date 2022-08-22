#pragma once

enum LogLevel {
    kError = 3,
    kWarn = 4,
    kInfo = 6,
    kDebug = 7,
};

/**
 * @brief 전역 로그레벨을 설정하는 함수
 * @param level 설정할 로그레벨
 */
void SetLogLevel(LogLevel level);

/**
 * @brief 주어진 로그레벨의 로그를 출력하는 함수
 * 초기 전역 로그레벨은 kWarn이며, 전역 로그레벨보다 높은 수준의 레벨의 로그만 실제 로깅됨
 * @param level 로깅을 시도할 로그레벨
 * @param format 서식 문자열
 * @param ... 문자열을 서식할 가변인자 리스트
 * @return int 성공 여부
 * - 0 : 로깅하지 않음
 * - >0 : 로깅된 문자열의 크기
 * - <0 : vsprintf 에러
 */
int Log(LogLevel level, const char* format, ...);