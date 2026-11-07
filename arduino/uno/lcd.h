// LCD 16x2 (HD44780) qua module I2C PCF8574, dùng chung bus I2C với GY-530 (A4/A5).
// Ghi vào bộ đệm bằng lcdText(); lcdUpdate() gọi liên tục trong loop() sẽ đẩy dần các ký tự
// thay đổi ra màn hình (mỗi lần gọi tối đa 1 ký tự, ~1 ms), nên không làm trễ vòng điều khiển.
#pragma once
#include <stdint.h>

constexpr uint8_t LCD_COLS = 16;
constexpr uint8_t LCD_ROWS = 2;

bool lcdBegin();  // gọi sau Wire.begin(); trả về false nếu không thấy module (xe vẫn chạy bình thường)
bool lcdOk();
// Ghi chuỗi vào bộ đệm dòng `row` (0 hoặc 1) từ cột 0, phần còn lại của dòng được xóa trắng.
void lcdText(uint8_t row, const char* s);
void lcdUpdate();
