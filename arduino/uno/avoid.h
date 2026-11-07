#pragma once
#include <stdint.h>

#include "sensors.h"

struct Motion {
  int16_t v;      // %, dương = tiến
  int16_t steer;  // % góc lái, dương = phải, ±100 = hết cỡ
};

void avoidReset();
// steerReady: góc lái thật (ước lượng) đã tới góc lệnh lần trước; dùng để chờ bẻ lái xong mới chạy
Motion avoidStep(const Ranges& r, uint32_t nowMs, bool steerReady);
bool avoidStuck();
