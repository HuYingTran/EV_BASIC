// Firmware ESP8266 (trên bo Arduino Uno+WiFi R3) - bản cho Arduino IDE.
// Phát WiFi, phục vụ trang remote trên điện thoại, chuyển lệnh/trạng thái qua UART với ATmega328P.
//
// Cách nạp bằng Arduino IDE:
//   1. Boards Manager: cài "esp8266" của ESP8266 Community (đã có bản 3.1.2).
//   2. Library Manager: cài "WebSockets" của Markus Sattler (bản 2.4.x).
//   3. Tools > Board: "Generic ESP8266 Module", rồi chỉnh:
//        Flash Size: "1MB (FS:64KB OTA:~470KB)"   Flash Mode: "DIO"   Upload Speed: "115200"
//   4. Gạt DIP 5,6,7 = ON, còn lại OFF, nhấn RESET, chọn cổng COM rồi Upload.
//   5. Chạy thật: DIP 1,2 = ON (ATmega <-> ESP8266), còn lại OFF.
//
// Toàn bộ chương trình nằm ở các tab bên cạnh (setup()/loop() trong esp8266_main.cpp).
// Các file đó được sao chép tự động từ src/esp của dự án PlatformIO (tools/sync_arduino.py):
// muốn sửa code thì sửa trong src/esp, sửa ở đây sẽ bị ghi đè ở lần build PlatformIO sau.

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WebSocketsServer.h>
