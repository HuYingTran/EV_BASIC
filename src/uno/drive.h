#pragma once
#include <stdint.h>

void driveBegin();
// left/right: -100..100 %. brake = true thì hãm ngay (IN = 0, EN = 1: hãm động năng).
// Gọi mỗi CONTROL_MS.
void driveUpdate(int16_t left, int16_t right, bool brake);
int16_t driveSpeedMmps();          // tốc độ ước lượng, trung bình hai bên, có dấu
int8_t drivePwmPct(uint8_t side);  // 0 = trái, 1 = phải
