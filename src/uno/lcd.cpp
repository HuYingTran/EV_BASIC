// Driver HD44780 chế độ 4 bit qua PCF8574 (sơ đồ chân phổ biến của module LCD I2C):
//   P0 = RS, P1 = RW, P2 = EN, P3 = đèn nền, P4..P7 = D4..D7
// Mỗi byte gửi tới LCD = 2 nửa byte, mỗi nửa có một xung EN; gói cả 4 lần ghi PCF8574 vào MỘT
// giao dịch I2C (~0,5 ms ở 100 kHz) thay vì 6 giao dịch như thư viện LiquidCrystal_I2C.
// PCF8574 chỉ chịu 100 kHz nên hạ xung nhịp khi nói với LCD rồi trả lại 400 kHz cho VL53L0X.
#include "lcd.h"

#include <Arduino.h>
#include <Wire.h>

#include "config.h"

namespace {

constexpr uint8_t BIT_RS = 0x01;
constexpr uint8_t BIT_EN = 0x04;
constexpr uint8_t BIT_BL = 0x08;
constexpr uint8_t NO_CURSOR = 0xFF;

uint8_t addr = 0;
char want[LCD_ROWS][LCD_COLS];   // nội dung cần hiển thị
char shown[LCD_ROWS][LCD_COLS];  // nội dung đang có trên LCD
uint8_t cursor = NO_CURSOR;      // vị trí con trỏ DDRAM (row * 16 + col), NO_CURSOR = chưa biết
uint8_t scan = 0;                // vị trí bắt đầu tìm ký tự khác lần sau

bool probe(uint8_t a) {
  Wire.beginTransmission(a);
  return Wire.endTransmission() == 0;
}

// Gửi 1 byte (lệnh hoặc dữ liệu) trong một giao dịch I2C
bool sendByte(uint8_t value, uint8_t rs) {
  const uint8_t hi = (value & 0xF0) | BIT_BL | rs;
  const uint8_t lo = (uint8_t)(value << 4) | BIT_BL | rs;
  Wire.setClock(100000);
  Wire.beginTransmission(addr);
  Wire.write(hi | BIT_EN);
  Wire.write(hi);  // sườn xuống EN: LCD chốt nửa byte cao
  Wire.write(lo | BIT_EN);
  Wire.write(lo);
  const bool ok = Wire.endTransmission() == 0;
  Wire.setClock(400000);
  return ok;  // mỗi byte I2C ~90 µs > 37 µs thời gian thực thi lệnh của HD44780
}

// Chỉ dùng lúc khởi tạo (LCD đang ở chế độ 8 bit, chỉ nhận nửa byte cao)
void sendNibble(uint8_t nibble) {
  const uint8_t v = (nibble << 4) | BIT_BL;
  Wire.setClock(100000);
  Wire.beginTransmission(addr);
  Wire.write(v | BIT_EN);
  Wire.write(v);
  Wire.endTransmission();
  Wire.setClock(400000);
}

}  // namespace

bool lcdBegin() {
  addr = 0;
  if (probe(LCD_ADDR_1)) addr = LCD_ADDR_1;
  else if (probe(LCD_ADDR_2)) addr = LCD_ADDR_2;
  if (!addr) return false;

  // Trình tự khởi tạo theo datasheet HD44780 (hình 24): 3 lần 0x3 rồi 0x2 để vào chế độ 4 bit
  delay(50);
  sendNibble(0x3); delay(5);
  sendNibble(0x3); delay(5);
  sendNibble(0x3); delayMicroseconds(200);
  sendNibble(0x2); delayMicroseconds(200);
  sendByte(0x28, 0);  // 4 bit, 2 dòng, font 5x8
  sendByte(0x0C, 0);  // bật hiển thị, tắt con trỏ
  sendByte(0x06, 0);  // con trỏ tự tăng
  sendByte(0x01, 0);  // xóa màn hình (mất ~1,5 ms)
  delay(2);

  memset(want, ' ', sizeof(want));
  memset(shown, ' ', sizeof(shown));
  cursor = NO_CURSOR;
  return true;
}

bool lcdOk() { return addr != 0; }

void lcdText(uint8_t row, const char* s) {
  if (row >= LCD_ROWS) return;
  for (uint8_t c = 0; c < LCD_COLS; c++) {
    want[row][c] = *s ? *s++ : ' ';
  }
}

void lcdUpdate() {
  if (!addr) return;
  constexpr uint8_t N = LCD_ROWS * LCD_COLS;
  for (uint8_t k = 0; k < N; k++) {
    const uint8_t pos = (scan + k) % N;
    const uint8_t row = pos / LCD_COLS, col = pos % LCD_COLS;
    if (want[row][col] == shown[row][col]) continue;

    if (cursor != pos) {
      if (!sendByte(0x80 | (row ? 0x40 : 0x00) | col, 0)) { cursor = NO_CURSOR; return; }
      cursor = pos;
      return;  // lần gọi sau mới ghi ký tự: mỗi lần gọi tối đa 1 giao dịch (~0,5 ms)
    }
    const char ch = want[row][col];
    if (!sendByte(ch, BIT_RS)) { cursor = NO_CURSOR; return; }
    shown[row][col] = ch;
    // HD44780 tự tăng con trỏ; hết dòng thì địa chỉ DDRAM không nhảy sang dòng 2
    cursor = col + 1 < LCD_COLS ? pos + 1 : NO_CURSOR;
    scan = (pos + 1) % N;
    return;
  }
}
