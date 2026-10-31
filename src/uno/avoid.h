#pragma once
#include <stdint.h>

#include "sensors.h"

struct Motion {
  int16_t v;     // %, dương = tiến
  int16_t turn;  // %, dương = rẽ phải
};

void avoidReset();
Motion avoidStep(const Ranges& r, uint32_t nowMs);
bool avoidStuck();
