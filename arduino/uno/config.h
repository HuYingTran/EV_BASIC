// Cấu hình phần cứng và tham số điều khiển cho ATmega328P.
// Các giá trị đánh dấu "ĐO LẠI" phải chốt theo số đo trên xe thật (đề cương mục 1.3).
#pragma once
#include <Arduino.h>

// ======================= GÁN CHÂN (Uno+WiFi R3) =======================
// D0/D1: UART nối ESP8266 qua DIP 1,2 = ON, không dùng cho việc khác.
// 2 module BTS7960 (IBT-2):
//   module 1 (KÉO): 2 động cơ bánh sau mắc song song, kéo xe tiến/lùi
//   module 2 (LÁI): động cơ đánh lái 2 bánh trước (không lò xo, không cảm biến góc)
// Băm xung ở RPWM/LPWM, giữ R_EN = L_EN = 1: pha nghỉ của xung bật cả hai khóa dưới,
// nối ngắn động cơ, nên giảm xung là động cơ được hãm động năng thay vì trôi tự do.
constexpr uint8_t PIN_MOTOR_EN = 4;    // R_EN + L_EN của cả 2 module, nối chung vào D4
constexpr uint8_t PIN_DRIVE_FWD = 5;   // module kéo RPWM - tiến
constexpr uint8_t PIN_DRIVE_REV = 6;   // module kéo LPWM - lùi
constexpr uint8_t PIN_STEER_R = 9;     // module lái RPWM - bẻ bánh trước sang phải
constexpr uint8_t PIN_STEER_L = 10;    // module lái LPWM - bẻ bánh trước sang trái
constexpr uint8_t PIN_US_L_TRIG = 7;   // US-100 trái: Trig/TX
constexpr uint8_t PIN_US_L_ECHO = 8;   // US-100 trái: Echo/RX (PB0)
constexpr uint8_t PIN_US_R_TRIG = 11;  // US-100 phải: Trig/TX
constexpr uint8_t PIN_US_R_ECHO = 12;  // US-100 phải: Echo/RX (PB4)
constexpr uint8_t PIN_VBAT = A2;       // phân áp 100k / 33k từ pin 12 V
// A4/A5 (SDA/SCL): GY-530 (VL53L0X, địa chỉ 0x29) + LCD 16x2 I2C (PCF8574, 0x27 hoặc 0x3F)
// Hai chân Echo phải cùng thuộc cổng B (D8..D13) vì dùng chung ngắt PCINT0.

// ======================= XE (ĐO LẠI) =======================
// Giá trị khởi điểm lấy từ mô hình chương 2 (do_an_src/mo_phong.py) với tham số động cơ giả định
// (mô hình cũ 4 động cơ kéo; nay chỉ 2 động cơ bánh sau kéo nên càng phải đo lại).
constexpr float VMAX_MMPS = 1480.0f;         // tốc độ xác lập ở PWM 100 % qua BTS7960 (mô hình: 1,48 m/s)
constexpr uint8_t PWM_MIN_PCT = 39;          // bù vùng chết: độ rộng xung vừa thắng cản lăn
constexpr float TAU_S = 3.9f;                // hằng số thời gian cơ của xe (mô hình bậc nhất, công thức 2.6)
constexpr float SOFTSTART_PCT_PER_S = 50.0f; // lệnh chỉ được tăng 50 %/s (khởi động mềm, giảm dòng đỉnh)
constexpr float BRAKE_MPS2 = 0.25f;          // gia tốc hãm tối thiểu bảo đảm (chỉ tính cản lăn), dùng cho (2.15)
constexpr float BRAKE_HYST_PCT = 5.0f;       // tốc độ ước lượng vượt lệnh quá ngưỡng này mới hãm (chống rung do nhiễu)

// ======================= ĐÁNH LÁI (ĐO LẠI) =======================
// Không có cảm biến góc: góc lái được ƯỚC LƯỢNG theo thời gian cấp điện cho động cơ lái
// (−100 % = hết cỡ trái, 0 = thẳng, +100 % = hết cỡ phải).
// Mỗi lần lệnh lái hết cỡ, động cơ đẩy thêm STEER_PUSH_MS vào chặn cơ khí để căn lại ước lượng.
// Khởi động: bẻ hết cỡ trái (đủ lâu để đi hết hành trình từ bất kỳ vị trí nào) rồi về giữa.
constexpr uint8_t STEER_PWM_PCT = 60;    // độ rộng xung khi bẻ lái; thấp vừa đủ quay để kẹt ở chặn không quá dòng
constexpr uint16_t STEER_FULL_MS = 400;  // thời gian từ thẳng tới hết cỡ ở STEER_PWM_PCT (bấm giờ trên xe)
constexpr uint16_t STEER_PUSH_MS = 150;  // đẩy thêm vào chặn cơ khí khi lệnh hết cỡ
constexpr uint8_t STEER_TOL_PCT = 8;     // sai lệch góc nhỏ hơn mức này thì tắt động cơ lái

// ======================= CHU KỲ =======================
constexpr uint16_t CONTROL_MS = 20;       // vòng điều khiển chính
constexpr uint16_t TELEM_MS = 100;        // gửi trạng thái sang ESP8266
constexpr uint16_t LINK_TIMEOUT_MS = 300; // mất lệnh quá thời gian này thì dừng
constexpr uint16_t US_PERIOD_MS = 30;     // kích luân phiên 2 US-100, mỗi cái 60 ms/lần

// ======================= TRUYỀN ĐỘNG =======================
// STEER_TOL phải lớn hơn bước ước lượng mỗi chu kỳ, nếu không động cơ lái sẽ rung qua lại quanh đích
static_assert(STEER_TOL_PCT * STEER_FULL_MS > 100 * CONTROL_MS, "STEER_TOL_PCT quá nhỏ so với STEER_FULL_MS");
constexpr uint8_t REVERSE_MAX_PCT = 22;     // không có cảm biến sau nên lùi chậm (~0,33 m/s)
constexpr uint8_t STICK_DEADBAND = 8;       // %, dưới ngưỡng coi như không chạm joystick

// ======================= LỚP AN TOÀN (mọi chế độ) =======================
constexpr uint16_t RANGE_MAX_MM = 4000;  // không có vọng = trống
constexpr uint16_t TOF_MAX_MM = 2000;    // tầm của VL53L0X ở chế độ tầm xa (vật trắng 88 %)
// Tầm phát hiện BẢO ĐẢM với vật tối màu (phản xạ 17 %: ~0,7-0,8 m theo DocID029104, Bảng 11).
// Lớp an toàn coi khoảng cách giữa không xa hơn giá trị này, nên luôn dừng kịp trước vật tối màu.
constexpr uint16_t TOF_SURE_MM = 800;
constexpr uint16_t D_SAFE_MM = 400;      // tốc độ tiến bị ép về 0 (dư 0,1 m so với yêu cầu dừng >= 0,3 m)
constexpr uint16_t TTC_MIN_MS = 1500;    // TTC = d / v nhỏ hơn thì giới hạn tốc độ
// Tốc độ tiến cho phép v = sqrt(2 · BRAKE_MPS2 · (d − D_SAFE)): luôn dừng kịp trước D_SAFE

// ======================= TỰ HÀNH =======================
// Các mức % dưới đây là % của VMAX_MMPS (đã bù vùng chết nên tốc độ tỉ lệ với %)
constexpr uint8_t AUTO_MAX_PCT = 36;        // ~0,53 m/s (~1,9 km/h)
constexpr uint8_t AUTO_CRUISE_PCT = 36;
constexpr uint8_t AUTO_TURN_PCT = 25;       // tốc độ tiến khi ôm cua hết lái (~0,37 m/s)
constexpr uint8_t AUTO_SIDE_STEER_MAX = 50; // góc lái tối đa (% hết cỡ) khi lách vật hai bên
constexpr uint8_t BACKUP_PCT = 29;          // ~0,43 m/s
// Xe lái bánh trước không quay tại chỗ được: cần chỗ để ôm cua, nên ngưỡng lùi phải lớn hơn D_SAFE
// (nếu không, lớp an toàn chặn ở D_SAFE trước khi thuật toán kịp chuyển sang lùi).
constexpr uint16_t D_BACKUP_MM = 450;  // gần hơn -> lùi (lùi kèm lái ngược để quay đầu xe)
constexpr uint16_t D_BLOCK_MM = 700;   // phía trước bị chặn -> quay tìm hướng
constexpr uint16_t D_CLEAR_MM = 1000;  // hướng được coi là trống
constexpr uint16_t D_SIDE_MM = 900;    // vật bên hông gần hơn -> lách
constexpr uint16_t BACKUP_MS = 2000;   // xe có quán tính lớn (τ ≈ 3,9 s): lùi ngắn hơn gần như không dịch chuyển
constexpr uint16_t TURN_MAX_MS = 4000; // ôm cua quá thời gian này mà chưa thoát -> lùi lấy đà lại
constexpr uint8_t MAX_RETRIES = 3;     // số lần lùi liên tiếp tối đa, quá thì dừng hẳn, báo kẹt
static_assert(D_BACKUP_MM > D_SAFE_MM, "D_BACKUP_MM phải lớn hơn D_SAFE_MM");

// ======================= LCD 16x2 =======================
constexpr uint8_t LCD_ADDR_1 = 0x27;    // PCF8574T; thử địa chỉ này trước
constexpr uint8_t LCD_ADDR_2 = 0x3F;    // PCF8574AT
constexpr uint16_t LCD_MS = 250;        // chu kỳ làm mới nội dung (nhanh hơn thì số nhảy khó đọc)
constexpr uint16_t LCD_SPLASH_MS = 2000; // màn hình chào lúc khởi động (tên WiFi + địa chỉ web)

// ======================= PIN =======================
constexpr float VBAT_DIVIDER = (100.0f + 33.0f) / 33.0f;
constexpr uint16_t VREF_MV = 5000;
constexpr uint16_t VBAT_EMPTY_MV = 11800;  // ắc quy chì 12 V lúc nghỉ: ~0 %
constexpr uint16_t VBAT_FULL_MV = 12700;   //                            ~100 %
