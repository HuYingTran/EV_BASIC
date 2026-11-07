#pragma once
#include <stdint.h>

void driveBegin();
// v: -100..100 % (dương = tiến), steer: -100..100 % góc lái (dương = phải, ±100 = hết cỡ).
// brake = true thì hãm động cơ kéo ngay và dừng động cơ lái tại chỗ. Gọi mỗi CONTROL_MS.
void driveUpdate(int16_t v, int16_t steer, bool brake);
int16_t driveSpeedMmps();  // tốc độ ước lượng, có dấu
int8_t drivePwmPct();      // % thực xuất ra driver kéo
int8_t steerPct();         // góc lái ước lượng, -100..100
bool steerReady();         // góc ước lượng đã tới góc lệnh và đã hiệu chuẩn xong
