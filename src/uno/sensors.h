#pragma once
#include <stdint.h>

struct Ranges {
  uint16_t left;    // mm, US-100 trái (lệch 45° sang trái)
  uint16_t center;  // mm, GY-530 chính giữa; = 0 khi lỗi (coi như bị chặn)
  uint16_t right;   // mm, US-100 phải
  bool fault;       // GY-530 không trả số liệu
};

void sensorsBegin();
void sensorsUpdate();  // gọi liên tục trong loop(), không chặn
const Ranges& sensorsRanges();
uint16_t batteryMv();
uint8_t batteryPct(uint16_t mv);
