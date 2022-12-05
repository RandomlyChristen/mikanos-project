#pragma once

#include <cstddef>

/** @brief 정적으로 예약할 페이지 디렉토리 수
 * 이 상수는 SetupIdentityPageMap에서 사용됩니다.
 * 한 페이지 디렉토리에 512 개의 2MiB 페이지를 설정할 수 있으므로,
 * kPageDirectoryCount x 1GiB의 가상 주소가 매핑됩니다.
 */
const size_t kPageDirectoryCount = 64;

/** @brief 가상 주소 = 물리적 주소가 되도록 페이지 테이블을 설정합니다.
 * 궁극적으로 CR3 레지스터가 올바르게 설정된 페이지 테이블을 가리킵니다.
 */
void SetupIdentityPageTable();

void InitializePaging();