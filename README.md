# Firmware xe điện 4 bánh tự tránh vật cản (ĐATN)

Phần cứng: **Arduino Uno+WiFi R3 (ATmega328P + ESP8266 trên cùng một bo)**, 2 × **US-100**,
1 × **GY-530 (VL53L0X)**, **2 × BTS7960 (IBT-2)**, 1 × **LCD 16x2 I2C (PCF8574)**, 2 động cơ 390 kéo bánh sau,
1 động cơ đánh lái bánh trước, ắc quy 12 V, mạch hạ áp 12 V → 5 V.

Cơ cấu truyền động kiểu ô tô: **BTS7960 #1 kéo đồng thời 2 bánh sau** (2 động cơ mắc song song),
**BTS7960 #2 đánh lái 2 bánh trước**. Cơ cấu lái không có lò xo và không có cảm biến góc, nên góc lái được
ước lượng theo thời gian cấp điện (xem mục *Đánh lái (vòng hở)* bên dưới).

Xe không có còi, công tắc hành trình hay nút dừng khẩn cứng: điều khiển và nút **DỪNG KHẨN CẤP** nằm trên trang
web điện thoại. LCD 16x2 trên xe hiển thị nhanh chế độ, cảnh báo, điện áp pin và khoảng cách (xem bên dưới).

## Kiến trúc 2 tầng

| Tầng | Chip | Nhiệm vụ |
|---|---|---|
| Thời gian thực | ATmega328P | Đọc cảm biến, lớp an toàn, thuật toán tránh vật cản, PWM động cơ. Tự dừng khi ESP8266 treo (300 ms). |
| Giao tiếp | ESP8266 | Phát WiFi, trang web remote + bảng hiển thị trên điện thoại, chuyển lệnh qua UART. |

Cảm biến nối thẳng vào ATmega nên lệnh dừng do vật cản không phụ thuộc WiFi.

```
 US-100 trái (45°)   GY-530 giữa   US-100 phải (45°)
          \              |              /
           +------- ATmega328P ---------+---> BTS7960 #1 (KÉO) -> 2 động cơ bánh sau (song song)
           |  UART 115200 (DIP 1,2)     +---> BTS7960 #2 (LÁI) -> động cơ đánh lái bánh trước
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
| D5 | PD5 / OC0B | BTS7960 **kéo** RPWM | ra (PWM) | Bánh sau tiến |
| D6 | PD6 / OC0A | BTS7960 **kéo** LPWM | ra (PWM) | Bánh sau lùi |
| D7 | PD7 | US-100 **trái** Trig/TX | ra | |
| D8 | PB0 | US-100 **trái** Echo/RX | vào | Ngắt PCINT0 |
| D9 | PB1 / OC1A | BTS7960 **lái** RPWM | ra (PWM) | Bẻ bánh trước sang phải |
| D10 | PB2 / OC1B | BTS7960 **lái** LPWM | ra (PWM) | Bẻ bánh trước sang trái |
| D11 | PB3 | US-100 **phải** Trig/TX | ra | |
| D12 | PB4 | US-100 **phải** Echo/RX | vào | Ngắt PCINT0 |
| A2 | PC2 / ADC2 | Điểm giữa cầu phân áp pin | vào (analog) | 12 V — 100 kΩ — A2 — 33 kΩ — GND |
| A4 (SDA) | PC4 | GY-530 SDA + LCD SDA | I2C | Bus chung |
| A5 (SCL) | PC5 | GY-530 SCL + LCD SCL | I2C | Bus chung |
| 5V | — | VCC của BTS7960 ×2, US-100 ×2, GY-530 VIN, LCD | nguồn | |
| GND | — | GND logic của mọi module + GND nguồn | đất | Đất chung |
| **Trống** | | D2, D3, D13, A0, A1, A3 | | Dành cho mở rộng (A3 có thể đo dòng, xem dưới) |

Hai chân Echo (D8, D12) phải cùng thuộc cổng B (D8..D13) vì dùng chung ngắt PCINT0; các chân PWM
động cơ phải là chân có timer phần cứng (D5/D6 = Timer0, D9/D10 = Timer1). Đổi chân thì sửa
`src/uno/config.h` và giữ đúng hai ràng buộc này.

### Theo từng module

**BTS7960 #1 – KÉO 2 bánh sau**

| Chân module | Nối tới |
|---|---|
| RPWM | D5 |
| LPWM | D6 |
| R_EN, L_EN | D4 (chung với module lái) |
| R_IS, L_IS | Để hở |
| VCC / GND | 5V / GND của Uno |
| B+ / B− | +12 V (sau cầu chì) / −12 V |
| M+ / M− | 2 động cơ bánh sau mắc **song song** (cùng chiều quay khi xe tiến) |

**BTS7960 #2 – ĐÁNH LÁI 2 bánh trước**: giống hệt, chỉ khác RPWM → **D9**, LPWM → **D10**, M+ / M− → động cơ lái.

Kiểm tra chiều khi chạy lần đầu (không cần sửa code, chỉ đảo dây M+ / M−):
- Lên nguồn, bánh trước phải bẻ hết sang **TRÁI** rồi về thẳng (hiệu chuẩn). Nếu bẻ sang phải: đảo dây động cơ lái.
- Đẩy joystick lên mà xe lùi: đảo dây M+ / M− của module kéo. Nếu một bánh sau quay ngược bánh kia: đảo dây
  riêng động cơ đó.

**US-100 trái / phải** (rút jumper ở mặt sau để chạy chế độ xung Trig/Echo)

| Chân module | Trái | Phải |
|---|---|---|
| VCC | 5V | 5V |
| Trig/TX | D7 | D11 |
| Echo/RX | D8 | D12 |
| GND (cả 2 chân GND) | GND | GND |

**GY-530 (VL53L0X)**: VIN → 5V (module có ổn áp sẵn), GND → GND, SDA → A4, SCL → A5.
Chân XSHUT và GPIO1 để hở.

**LCD 16x2 I2C** (LCD 1602 + module PCF8574 hàn sẵn phía sau): VCC → 5V, GND → GND, SDA → A4, SCL → A5
(cắm song song với GY-530). Firmware tự dò địa chỉ **0x27** (PCF8574T) rồi **0x3F** (PCF8574AT); địa chỉ khác thì
sửa `LCD_ADDR_1` trong `src/uno/config.h`. Lên nguồn mà màn hình sáng nhưng không thấy chữ: vặn biến trở
xanh phía sau module để chỉnh độ tương phản. Không cắm LCD thì xe vẫn chạy bình thường.

**Phân áp đo pin**: +12 V — điện trở 100 kΩ — **A2** — điện trở 33 kΩ — GND (A2 tối đa ≈ 3,2 V ở 12,7 V).

### Nguồn và lắp đặt

- Buck 12 V → 5 V ≥ 3 A cấp vào chân **5V** của Uno (không qua jack DC); đất động cơ và đất tín hiệu
  nối chung tại một điểm.
- Nên có **cầu chì + công tắc nguồn chính** trên dây 12 V vào hai BTS7960, vì xe không còn nút dừng khẩn cứng.
- GY-530 đặt giữa cản trước, hướng thẳng. Hai US-100 đặt hai góc cản trước, quay lệch ra ngoài khoảng 45°.
- PWM đặt ở RPWM/LPWM (không đặt ở EN) để pha nghỉ của xung là **hãm động năng** (hai khóa dưới cùng dẫn)
  chứ không phải trôi tự do; nhờ vậy khi lớp an toàn giảm lệnh, xe giảm tốc chủ động.
- Muốn đo dòng động cơ: nối R_IS với L_IS của một module rồi đưa vào A3, thêm 1 kΩ từ A3 xuống GND
  (firmware chưa đọc chân này).
- BTS7960 chịu dòng lớn (giới hạn điển hình 43 A, điện trở đường dẫn 16 mΩ) nên sụt áp không đáng kể. Module kéo
  gánh 2 động cơ song song nên dòng khởi động khoảng ~12 A, vẫn xa giới hạn, không cần tản nhiệt thêm.
  Tốc độ tối đa 1,48 m/s lấy từ mô hình cũ 4 động cơ kéo, nay chỉ 2 động cơ kéo nên phải đo lại.

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
- **LÁI TAY**: joystick trên web (thay remote): trục dọc = ga bánh sau, trục ngang = góc lái bánh trước
  (kéo hết sang một bên = lái hết cỡ). Thanh "Tốc độ tối đa" chỉ giới hạn ga, không giới hạn góc lái.
  Lớp an toàn vẫn chạy: không cho tiến khi vật cản ≤ 0,4 m.
- **TỰ HÀNH**: lệnh 36 % (~0,53 m/s). Xe lái bánh trước nên **không quay tại chỗ** được:
  - đi thẳng, vừa chạy vừa lái lệch (tối đa 50 % góc lái) khi có vật bên hông;
  - phía trước bị chặn (< 0,7 m) hoặc một bên sát vật: đứng chờ bẻ hết lái về phía trống hơn rồi tiến 25 % (ôm cua),
    thoát khi cả ba vùng thoáng;
  - phía trước hoặc phía đang vòng vào quá gần (≤ 0,45 m), hoặc ôm cua quá 4 s: bẻ lái **ngược** rồi lùi 29 % trong 2 s
    (đầu xe xoay ra chỗ trống), sau đó ôm cua tiếp, kiểu quay đầu 3 điểm;
  - lùi liên tiếp quá 3 lần mà chưa ra chỗ thoáng thì dừng và báo **KẸT**.

  Chạm joystick trong lúc tự hành thì remote ghi đè ngay. Khi không chạm, núm joystick chuyển xanh lá và tự di chuyển
  theo ga thực của bánh sau (trục dọc, đã phóng đại để dễ nhìn) và góc lái ước lượng (trục ngang), kèm dòng chữ mô tả
  chuyển động; phần hiển thị này không gửi lệnh về xe.
- **DỪNG KHẨN CẤP** (nút trên web) hoặc mất liên lạc > 300 ms: xe hãm và **khóa**;
  phải bấm DỪNG rồi chọn lại chế độ mới chạy tiếp.

### Màn hình LCD 16x2

2 giây đầu hiện tên WiFi và địa chỉ web (`WiFi: EV-DATN` / `192.168.4.1`), sau đó làm mới mỗi 250 ms:

```
TU HANH    12.4V      dòng 1: trạng thái + điện áp pin
T 85 G120 P--- !      dòng 2: khoảng cách trái / giữa / phải (cm) + dấu lớp an toàn
```

- Trạng thái (ưu tiên từ trên xuống): `KHAN CAP` (dừng khẩn), `MAT KETNOI` (mất lệnh từ ESP8266),
  `LOI GY-530`, `BI KET`, `DA KHOA` (chờ bấm DỪNG), `GHI DE TAY` (remote ghi đè tự hành), rồi tới tên chế độ
  `DUNG` / `LAI TAY` / `TU HANH`.
- Khoảng cách: `---` = ngoài tầm (không có vọng), `LOI` = GY-530 không trả số liệu.
- Dấu cuối dòng 2: `!` = tốc độ tiến bị ép về 0 vì vật cản, `-` = đang giảm tốc theo khoảng cách hoặc TTC.

Driver LCD (`lcd.cpp`) tự viết, không cần cài thư viện: mỗi vòng `loop()` chỉ gửi tối đa một ký tự đã thay đổi
(~0,5 ms ở 100 kHz) nên không làm trễ vòng điều khiển 20 ms; bus I2C tạm hạ về 100 kHz khi nói với PCF8574 rồi
trả lại 400 kHz cho VL53L0X.

Lớp an toàn (mọi chế độ): `v_đặt = min(v_lệnh, √(2·a_h·(d − 0,4 m)))` với a_h = 0,25 m/s², trong đó khoảng
cách giữa bị chặn trên ở `TOF_SURE_MM` = 0,8 m (VL53L0X chỉ thấy vật tối màu ở ~0,8 m). Hệ quả: tốc độ tiến
tối đa ~30 % (~0,45 m/s). Thêm giới hạn TTC = d / v ≥ 1,5 s. Lùi tối đa 22 % (~0,33 m/s) vì không có cảm biến phía sau.

### Đánh lái (vòng hở)

Không có lò xo hay cảm biến góc, nên `drive.cpp` **ước lượng** góc lái: động cơ lái chạy ở `STEER_PWM_PCT` thì góc
đổi đều 100 % sau mỗi `STEER_FULL_MS`. Sai lệch giữa góc lệnh và góc ước lượng lớn hơn `STEER_TOL_PCT` thì chạy về
phía đích, còn lại thì tắt (hãm giữ chỗ, bánh đứng yên nhờ ma sát hộp số).

- **Hiệu chuẩn lúc khởi động**: bẻ hết cỡ trái trong `2·STEER_FULL_MS + STEER_PUSH_MS` (đủ đi hết hành trình từ
  vị trí bất kỳ), lấy đó làm gốc −100 %, rồi mới về thẳng. Nhấn DỪNG KHẨN CẤP thì hiệu chuẩn tạm dừng.
- **Tự căn lại**: mỗi lần lệnh lái hết cỡ, động cơ đẩy thêm `STEER_PUSH_MS` vào chặn cơ khí nên sai số tích lũy được
  xóa. Tự hành dùng hết lái thường xuyên nên ước lượng luôn sát. Lái tay nhiều lần ở góc nhỏ thì vị trí "thẳng" có thể
  lệch dần: kéo joystick hết sang một bên một lần là căn lại.
- **Dừng khẩn**: động cơ lái đứng yên tại chỗ. **DỪNG / mất liên lạc / đã khóa**: bánh trước tự về thẳng.

Cách đo `STEER_FULL_MS`: gác xe lên, cho bánh trước ở hết cỡ trái, kéo joystick hết sang phải và bấm giờ tới lúc
bánh chạm chặn phải, rồi chia đôi (hành trình trái → phải gấp đôi hành trình thẳng → hết cỡ). Nếu lái tay về giữa
mà bánh vẫn lệch một bên thì chỉnh lại giá trị này. `STEER_PWM_PCT` để mức thấp nhất mà động cơ vẫn bẻ lái chắc khi
xe đứng trên sàn, để lúc đẩy vào chặn cơ khí dòng không quá lớn.

### Động cơ kéo

Điều khiển động cơ kéo (`drive.cpp`, vòng hở, không dùng encoder): bù vùng chết (`PWM_MIN_PCT`), tốc độ ước lượng theo mô hình bậc nhất
(`TAU_S`), lệnh nhỏ hơn tốc độ ước lượng quá 5 % thì hãm động năng (PWM = 0). Lý do và kiểm chứng: Chương 2 và 5
của quyển đồ án, mô phỏng ở `../do_an_src/mo_phong.py`.

## Tham số cần chỉnh (`src/uno/config.h`)

Các giá trị hiện tại lấy từ mô hình với tham số động cơ **giả định**; đo xe thật (phụ lục A4 của quyển) rồi sửa:

- `VMAX_MMPS`, `PWM_MIN_PCT`, `TAU_S`: tốc độ ở 100 %, độ rộng xung bắt đầu lăn, hằng số thời gian
  (nay chỉ 2 động cơ bánh sau kéo, các giá trị cũ của mô hình 4 động cơ không còn đúng).
- `STEER_FULL_MS`, `STEER_PWM_PCT`, `STEER_PUSH_MS`: cơ cấu lái (xem mục *Đánh lái (vòng hở)*).
- `AUTO_TURN_PCT`, `D_BACKUP_MM`, `TURN_MAX_MS`: chỉnh theo bán kính quay thực của xe.
- `TOF_SURE_MM`: nâng lên nếu sân thử chỉ có vật sáng màu (xe chạy nhanh hơn, kém an toàn với vật tối).
- `VBAT_EMPTY_MV / VBAT_FULL_MV`: ắc quy chì 12 V, đo lúc nghỉ.
- Khoảng cách `D_*`: chỉnh sau thử nghiệm vật cản tĩnh.

## Cấu trúc mã nguồn

```
include/protocol.h     Khung UART dùng chung 2 chip (0xAA, LEN, TYPE, PAYLOAD, XOR)
src/uno/config.h       Gán chân GPIO và mọi tham số
src/uno/main.cpp       Vòng 20 ms: ưu tiên an toàn -> chế độ -> giới hạn tốc độ -> ga + góc lái
src/uno/sensors.cpp    US-100 qua ngắt PCINT (không chặn), lọc trung vị; VL53L0X đọc không chặn; pin
src/uno/avoid.cpp      Tránh vật cản 3 sector cho xe lái bánh trước (đi thẳng / ôm cua / lùi lái ngược / kẹt)
src/uno/drive.cpp      BTS7960 kéo (bù vùng chết, ước lượng tốc độ, hãm) + BTS7960 lái (ước lượng góc vòng hở)
src/uno/lcd.cpp        LCD 16x2 I2C (HD44780 + PCF8574), ghi không chặn từng ký tự thay đổi
src/esp/main.cpp       WiFi AP, HTTP, WebSocket, cầu nối UART, watchdog trình duyệt 500 ms
src/esp/web_page.h     Giao diện remote + hiển thị trên điện thoại
arduino/uno/           Sketch Arduino IDE cho ATmega328P (sao chép tự động từ src/uno)
arduino/esp8266/       Sketch Arduino IDE cho ESP8266 (sao chép tự động từ src/esp)
tools/sync_arduino.py  Script sao chép src/ -> arduino/, chạy trước mỗi lần build PlatformIO
```
