// Firmware ATmega328P (Arduino Uno+WiFi R3) - bản cho Arduino IDE.
//
// Cách nạp bằng Arduino IDE:
//   1. Library Manager: cài "VL53L0X" của Pololu (bản 1.3.x).
//   2. Tools > Board: "Arduino Uno"; chọn đúng cổng COM.
//   3. Gạt DIP 3,4 = ON, còn lại OFF, rồi Upload.
//   4. Chạy thật: DIP 1,2 = ON (ATmega <-> ESP8266), còn lại OFF.
//
// Toàn bộ chương trình nằm ở các tab .cpp/.h bên cạnh (setup()/loop() trong uno_main.cpp).
// Các file đó được sao chép tự động từ src/uno của dự án PlatformIO (tools/sync_arduino.py):
// muốn sửa code thì sửa trong src/uno, sửa ở đây sẽ bị ghi đè ở lần build PlatformIO sau.
// Tham số xe (chân, tốc độ, ngưỡng khoảng cách) nằm trong config.h.

#include <Wire.h>
#include <VL53L0X.h>
