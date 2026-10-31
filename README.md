# Firmware xe điện 4WD tự tránh vật cản (ĐATN)

Phần cứng: **Arduino Uno+WiFi R3 (ATmega328P + ESP8266 trên cùng một bo)**, 2 × **US-100**,
1 × **GY-530 (VL53L0X)**, **2 × BTS7960 (IBT-2)**, 4 động cơ 390, ắc quy 12 V, mạch hạ áp 12 V → 5 V.

Xe không có màn hình, còi, công tắc hành trình hay nút dừng khẩn cứng: mọi trạng thái hiển thị và nút
**DỪNG KHẨN CẤP** đều nằm trên trang web điện thoại.

## Kiến trúc 2 tầng

| Tầng | Chip | Nhiệm vụ |
|---|---|---|
| Thời gian thực | ATmega328P | Đọc cảm biến, lớp an toàn, thuật toán tránh vật cản, PWM động cơ. Tự dừng khi ESP8266 treo (300 ms). |
| Giao tiếp | ESP8266 | Phát WiFi, trang web remote + bảng hiển thị trên điện thoại, chuyển lệnh qua UART. |

Cảm biến nối thẳng vào ATmega nên lệnh dừng do vật cản không phụ thuộc WiFi.

```
 US-100 trái (30°)   GY-530 giữa   US-100 phải (30°)
          \              |              /
           +------- ATmega328P ---------+---> BTS7960 #1 -> 2 động cơ trái
           |  UART 115200 (DIP 1,2)     +---> BTS7960 #2 -> 2 động cơ phải
        ESP8266  ~~~ WiFi "EV-DATN" ~~~  Điện thoại (http://192.168.4.1)
```

## Đấu nối (chân GPIO)

Mọi dây tín hiệu đều cắm vào hàng chân của **ATmega328P** (Dx, Ax in trên bo). ESP8266 không cần đấu
dây ngoài: nó nối với ATmega qua đường UART có sẵn trên bo, bật bằng DIP 1, 2.

### Bảng tổng hợp theo chân Uno

| Chân Uno | Chân AVR | Nối tới | Chiều | Ghi chú |
|---|---|---|---|---|
| D0 (RX) | PD0 | ESP8266 TX | vào | Nối sẵn trên bo qua DIP 1. **Không cắm gì khác** |
| D1 (TX) | PD1 | ESP8266 RX | ra | Nối sẵn trên bo qua DIP 2. **Không cắm gì khác** |
| D4 | PD4 | R_EN + L_EN của **cả 2** BTS7960 (4 chân nối chung) | ra | Firmware giữ mức 1 |
| D5 | PD5 / OC0B | BTS7960 **trái** RPWM | ra (PWM) | Bánh trái tiến |
| D6 | PD6 / OC0A | BTS7960 **trái** LPWM | ra (PWM) | Bánh trái lùi |
| D7 | PD7 | US-100 **trái** Trig/TX | ra | |
| D8 | PB0 | US-100 **trái** Echo/RX | vào | Ngắt PCINT0 |
| D9 | PB1 / OC1A | BTS7960 **phải** RPWM | ra (PWM) | Bánh phải tiến |
| D10 | PB2 / OC1B | BTS7960 **phải** LPWM | ra (PWM) | Bánh phải lùi |
| D11 | PB3 | US-100 **phải** Trig/TX | ra | |
| D12 | PB4 | US-100 **phải** Echo/RX | vào | Ngắt PCINT0 |
| A2 | PC2 / ADC2 | Điểm giữa cầu phân áp pin | vào (analog) | 12 V — 100 kΩ — A2 — 33 kΩ — GND |
| A4 (SDA) | PC4 | GY-530 SDA | I2C | |
| A5 (SCL) | PC5 | GY-530 SCL | I2C | |
| 5V | — | VCC của BTS7960 ×2, US-100 ×2, GY-530 VIN | nguồn | |
| GND | — | GND logic của mọi module + GND nguồn | đất | Đất chung |
| **Trống** | | D2, D3, D13, A0, A1, A3 | | Dành cho mở rộng (A3 có thể đo dòng, xem dưới) |

Hai chân Echo (D8, D12) phải cùng thuộc cổng B (D8..D13) vì dùng chung ngắt PCINT0; các chân PWM
động cơ phải là chân có timer phần cứng (D5/D6 = Timer0, D9/D10 = Timer1). Đổi chân thì sửa
`src/uno/config.h` và giữ đúng hai ràng buộc này.

### Theo từng module

**BTS7960 #1 – cặp bánh TRÁI**

| Chân module | Nối tới |
|---|---|
| RPWM | D5 |
| LPWM | D6 |
| R_EN, L_EN | D4 (chung với module phải) |
| R_IS, L_IS | Để hở |
| VCC / GND | 5V / GND của Uno |
| B+ / B− | +12 V (sau cầu chì) / −12 V |
| M+ / M− | 2 động cơ bên trái mắc nối tiếp |

**BTS7960 #2 – cặp bánh PHẢI**: giống hệt, chỉ khác RPWM → **D9**, LPWM → **D10**.

Nếu xe tiến mà một bên quay lùi: đảo hai dây M+ / M− của bên đó (không cần sửa code).

**US-100 trái / phải** (rút jumper ở mặt sau để chạy chế độ xung Trig/Echo)

| Chân module | Trái | Phải |
|---|---|---|
| VCC | 5V | 5V |
| Trig/TX | D7 | D11 |
| Echo/RX | D8 | D12 |
| GND (cả 2 chân GND) | GND | GND |

**GY-530 (VL53L0X)**: VIN → 5V (module có ổn áp sẵn), GND → GND, SDA → A4, SCL → A5.
Chân XSHUT và GPIO1 để hở.

**Phân áp đo pin**: +12 V — điện trở 100 kΩ — **A2** — điện trở 33 kΩ — GND (A2 tối đa ≈ 3,2 V ở 12,7 V).

### Nguồn và lắp đặt

- Buck 12 V → 5 V ≥ 3 A cấp vào chân **5V** của Uno (không qua jack DC); đất động cơ và đất tín hiệu
  nối chung tại một điểm.
- Nên có **cầu chì + công tắc nguồn chính** trên dây 12 V vào hai BTS7960, vì xe không còn nút dừng khẩn cứng.
- GY-530 đặt giữa cản trước, hướng thẳng. Hai US-100 đặt hai góc cản trước, quay lệch ra ngoài khoảng 30°.
- PWM đặt ở RPWM/LPWM (không đặt ở EN) để pha nghỉ của xung là **hãm động năng** (hai khóa dưới cùng dẫn)
  chứ không phải trôi tự do; nhờ vậy khi lớp an toàn giảm lệnh, xe giảm tốc chủ động.
- Muốn đo dòng động cơ: nối R_IS với L_IS của một module rồi đưa vào A3, thêm 1 kΩ từ A3 xuống GND
  (firmware chưa đọc chân này).
- BTS7960 chịu dòng lớn (giới hạn điển hình 43 A, điện trở đường dẫn 16 mΩ) nên sụt áp không đáng kể: tốc độ
  tối đa theo mô hình 1,48 m/s (~5,3 km/h), dòng khởi động mỗi bên ~6 A, không cần tản nhiệt thêm.

## Nạp code

### 1. Cài đặt (làm một lần)

1. Cài **VS Code**, vào Extensions cài **PlatformIO IDE** (hoặc chỉ cài PlatformIO Core: `pip install platformio`).
2. Cài driver USB-Serial **CH340** (bo Uno+WiFi R3 dùng chip CH340G). Cắm USB, kiểm tra trong
   Device Manager → *Ports (COM & LPT)* thấy `USB-SERIAL CH340 (COMx)`.
3. Mở thư mục `firmware` bằng VS Code (File → Open Folder). Lần build đầu PlatformIO tự tải toolchain
   AVR, ESP8266 và thư viện (VL53L0X, WebSockets) nên cần mạng.

Các lệnh bên dưới gõ trong **PlatformIO: New Terminal** (hoặc terminal có `pio` trong PATH), tại thư mục `firmware`.

### 2. Công tắc DIP trên bo

Bo có dãy DIP 8 công tắc chọn ai nói chuyện với ai. **Rút nguồn 12 V của động cơ trước khi gạt DIP và nạp code.**

| Việc | DIP ON | DIP còn lại |
|---|---|---|
| Nạp ATmega328P | **3, 4** | OFF |
| Nạp ESP8266 | **5, 6, 7** | OFF |
| Chạy thật | **1, 2** | OFF |

### 3. Nạp ATmega328P

1. Gạt DIP 3, 4 = ON, còn lại OFF. Cắm USB.
2. Chạy:
   ```
   pio run -e uno -t upload
   ```
   (Trong VS Code: thanh dưới cùng chọn môi trường `env:uno`, bấm nút mũi tên **Upload**.)
3. Nếu PlatformIO không tự tìm đúng cổng: thêm `--upload-port COM5` (thay số cổng thực tế).

**Hoặc nạp bằng Arduino IDE** (không cần PlatformIO):

1. Mở `arduino/uno/uno.ino` bằng Arduino IDE.
2. Library Manager: cài **VL53L0X** của Pololu (bản 1.3.x).
3. Tools > Board: **Arduino Uno**, chọn cổng COM, gạt DIP 3, 4 = ON rồi bấm **Upload**.

Code trong `arduino/uno/` là bản sao của `src/uno/` + `include/protocol.h`, được
`tools/sync_arduino.py` tự cập nhật mỗi lần build bằng PlatformIO
(hoặc chạy tay `python tools/sync_arduino.py`). Sửa code trong `src/uno/`, không sửa trong `arduino/uno/`.

### 4. Nạp ESP8266

1. Gạt DIP 5, 6, 7 = ON, còn lại OFF (DIP 7 kéo GPIO0 xuống GND để ESP8266 vào chế độ nạp).
2. Nhấn nút **RESET** (hoặc rút/cắm lại USB) để ESP8266 khởi động lại ở chế độ nạp.
3. Chạy:
   ```
   pio run -e esp8266 -t upload
   ```
4. Nạp xong **gạt DIP 7 về OFF** rồi nhấn RESET, nếu không ESP8266 sẽ luôn khởi động vào chế độ nạp.

Nếu báo `Failed to connect to ESP8266`: kiểm tra lại DIP 7, nhấn RESET ngay trước khi lệnh bắt đầu
kết nối, thử cắm cổng USB khác.

**Hoặc nạp bằng Arduino IDE** (không cần PlatformIO):

1. Mở `arduino/esp8266/esp8266.ino` bằng Arduino IDE.
2. Boards Manager: cài **esp8266** (ESP8266 Community). Library Manager: cài **WebSockets** của Markus Sattler (bản 2.4.x).
3. Tools > Board: **Generic ESP8266 Module**, chỉnh Flash Size **1MB (FS:64KB OTA:~470KB)**,
   Flash Mode **DIO**, Upload Speed **115200**.
4. Gạt DIP 5, 6, 7 = ON, nhấn RESET, chọn cổng COM rồi bấm **Upload**. Nạp xong gạt DIP 7 về OFF.

Code trong `arduino/esp8266/` là bản sao của `src/esp/` + `include/protocol.h`, cập nhật tự động như bản Uno.

### 5. Chạy thật

1. Gạt DIP 1, 2 = ON, còn lại OFF (ATmega ↔ ESP8266). Có thể rút USB, cấp nguồn từ buck 5 V.
2. Bật nguồn 12 V cho động cơ.
3. Điện thoại kết nối WiFi **`EV-DATN`**, mật khẩu **`datn2026`**, mở trình duyệt vào **`http://192.168.4.1`**.
4. Bấm **DỪNG** một lần để mở khóa, sau đó chọn LÁI TAY hoặc TỰ HÀNH.

Muốn xem log UART của một chip: dùng DIP nạp của chip đó và chạy `pio device monitor -e uno`
(hoặc `-e esp8266`), 115200 baud. Ở chế độ chạy thật (DIP 1, 2) cổng USB không nối với chip nào.

## Cách xe vận hành

- **DỪNG**: dừng có kiểm soát; đồng thời mở khóa sau sự cố.
- **LÁI TAY**: joystick trên web (thay remote). Lớp an toàn vẫn chạy: không cho tiến khi vật cản ≤ 0,4 m.
- **TỰ HÀNH**: lệnh 36 % (~0,53 m/s). Đi thẳng, lách khi có vật bên hông; phía trước bị chặn hoặc một bên sát vật thì
  quay tại chỗ về phía trống hơn; phía trước hoặc cả hai bên quá gần thì lùi 29 % trong 2 s rồi quay; thử 3 lần
  không thoát thì dừng và báo **KẸT**. Chạm joystick trong lúc tự hành thì remote ghi đè ngay.
- **DỪNG KHẨN CẤP** (nút trên web) hoặc mất liên lạc > 300 ms: xe hãm và **khóa**;
  phải bấm DỪNG rồi chọn lại chế độ mới chạy tiếp.

Lớp an toàn (mọi chế độ): `v_đặt = min(v_lệnh, √(2·a_h·(d − 0,4 m)))` với a_h = 0,25 m/s², trong đó khoảng
cách giữa bị chặn trên ở `TOF_SURE_MM` = 0,8 m (VL53L0X chỉ thấy vật tối màu ở ~0,8 m). Hệ quả: tốc độ tiến
tối đa ~30 % (~0,45 m/s). Thêm giới hạn TTC = d / v ≥ 1,5 s. Lùi tối đa 22 % (~0,33 m/s) vì không có cảm biến phía sau.

Điều khiển động cơ (`drive.cpp`, vòng hở, không dùng encoder): bù vùng chết (`PWM_MIN_PCT`), tốc độ ước lượng theo mô hình bậc nhất
(`TAU_S`), lệnh nhỏ hơn tốc độ ước lượng quá 5 % thì hãm động năng (PWM = 0). Lý do và kiểm chứng: Chương 2 và 5
của quyển đồ án, mô phỏng ở `../do_an_src/mo_phong.py`.

## Tham số cần chỉnh (`src/uno/config.h`)

Các giá trị hiện tại lấy từ mô hình với tham số động cơ **giả định**; đo xe thật (phụ lục A4 của quyển) rồi sửa:

- `VMAX_MMPS`, `PWM_MIN_PCT`, `TAU_S`: tốc độ ở 100 %, độ rộng xung bắt đầu lăn, hằng số thời gian.
- `TOF_SURE_MM`: nâng lên nếu sân thử chỉ có vật sáng màu (xe chạy nhanh hơn, kém an toàn với vật tối).
- `VBAT_EMPTY_MV / VBAT_FULL_MV`: ắc quy chì 12 V, đo lúc nghỉ.
- Khoảng cách `D_*`: chỉnh sau thử nghiệm vật cản tĩnh.

## Cấu trúc mã nguồn

```
include/protocol.h     Khung UART dùng chung 2 chip (0xAA, LEN, TYPE, PAYLOAD, XOR)
src/uno/config.h       Gán chân GPIO và mọi tham số
src/uno/main.cpp       Vòng 20 ms: ưu tiên an toàn -> chế độ -> giới hạn tốc độ -> trộn vi sai
src/uno/sensors.cpp    US-100 qua ngắt PCINT (không chặn), lọc trung vị; VL53L0X đọc không chặn; pin
src/uno/avoid.cpp      Thuật toán tránh vật cản 3 sector (máy trạng thái Cruise/Backup/Turn/Stuck)
src/uno/drive.cpp      2 × BTS7960 (PWM ở RPWM/LPWM), bù vùng chết, ước lượng tốc độ, hãm
src/esp/main.cpp       WiFi AP, HTTP, WebSocket, cầu nối UART, watchdog trình duyệt 500 ms
src/esp/web_page.h     Giao diện remote + hiển thị trên điện thoại
arduino/uno/           Sketch Arduino IDE cho ATmega328P (sao chép tự động từ src/uno)
arduino/esp8266/       Sketch Arduino IDE cho ESP8266 (sao chép tự động từ src/esp)
tools/sync_arduino.py  Script sao chép src/ -> arduino/, chạy trước mỗi lần build PlatformIO
```
