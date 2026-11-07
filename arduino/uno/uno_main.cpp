// Firmware ATmega328P: tầng thời gian thực của xe.
//  - Đọc 2 US-100 (trái/phải) + GY-530 (giữa), điện áp pin
//  - Nhận lệnh từ ESP8266 (remote trên điện thoại), chạy thuật toán tránh vật cản
//  - Lớp an toàn luôn chạy, kể cả khi lái tay; ESP8266 treo thì tự dừng sau 300 ms
//  - Điều khiển 2 module BTS7960 (kéo bánh sau + đánh lái bánh trước),
//    gửi trạng thái về ESP8266 (hiển thị trên điện thoại)
//  - Hiển thị chế độ, cảnh báo, điện áp pin và khoảng cách lên LCD 16x2 I2C
#include <Arduino.h>
#include <stdio.h>

#include "avoid.h"
#include "config.h"
#include "drive.h"
#include "lcd.h"
#include "protocol.h"
#include "sensors.h"

namespace {

using namespace proto;

Parser parser;
Cmd cmd = {MODE_STOP, 0, 0, 100, 0};
uint32_t lastCmdMs = 0;
bool haveCmd = false;
bool latched = true;  // khởi động ở trạng thái khóa, phải nhận lệnh STOP mới chạy được
uint8_t activeMode = MODE_STOP;
uint16_t flags = 0;
uint32_t tControl = 0, tTelem = 0, tLcd = 0;

void readLink() {
  while (Serial.available()) {
    if (parser.feed(Serial.read()) && parser.read(CMD, cmd)) {
      lastCmdMs = millis();
      haveCmd = true;
    }
  }
}

// Vòng an toàn ngoài: v_đặt = min(v_lệnh, sqrt(2·a_h·(d − d_at))) (công thức 2.15 trong quyển đồ án), và giới hạn TTC (2.16)
int16_t limitForward(int16_t v, const Ranges& r) {
  if (v <= 0) return v;
  const uint16_t dCenter = min(r.center, TOF_SURE_MM);  // vật tối màu có thể chưa hiện ở xa hơn
  const uint16_t d = min(dCenter, min(r.left, r.right));
  int16_t lim = 0;
  if (d > D_SAFE_MM) {
    const float vAllowed = sqrtf(2.0f * BRAKE_MPS2 * (d - D_SAFE_MM) / 1000.0f) * 1000.0f;  // mm/s
    lim = min(100.0f, vAllowed * 100.0f / VMAX_MMPS);
  }

  const int16_t spd = driveSpeedMmps();
  if (spd > 50 && (uint32_t)d * 1000 < (uint32_t)TTC_MIN_MS * spd) {
    flags |= F_TTC;
    const int16_t ttcLim = (uint32_t)d * 1000 / TTC_MIN_MS * 100 / (uint32_t)VMAX_MMPS;
    lim = min(lim, ttcLim);
  }
  if (lim < v) {
    flags |= lim == 0 ? F_OBST_STOP : F_SLOW;
    return lim;
  }
  return v;
}

void control(uint32_t now) {
  const Ranges& r = sensorsRanges();
  flags = 0;

  // Thứ tự ưu tiên: dừng khẩn (nút trên web) -> mất liên lạc -> chế độ
  const bool estop = cmd.estop;
  const bool linkLost = !haveCmd || now - lastCmdMs > LINK_TIMEOUT_MS;
  if (estop) flags |= F_ESTOP;
  if (linkLost) flags |= F_LINK_LOST;
  if (r.fault) flags |= F_SENSOR;

  // Sau sự cố, xe chỉ chạy lại khi người giám sát chủ động bấm DỪNG rồi chọn chế độ
  if (estop || linkLost) latched = true;
  else if (cmd.mode == MODE_STOP) latched = false;
  if (latched) flags |= F_LATCHED;

  const uint8_t mode = latched ? MODE_STOP : cmd.mode;
  if (mode != activeMode) {
    if (mode == MODE_AUTO) avoidReset();
    activeMode = mode;
  }

  int16_t v = 0, steer = 0;
  const bool stickActive = abs(cmd.throttle) > STICK_DEADBAND || abs(cmd.steer) > STICK_DEADBAND;
  if (mode == MODE_MANUAL || (mode == MODE_AUTO && stickActive)) {
    // Remote luôn thắng lệnh tự động
    v = cmd.throttle;
    steer = cmd.steer;  // joystick ngang = góc lái (tỉ lệ), kéo hết sang bên = hết cỡ
    if (mode == MODE_AUTO) flags |= F_OVERRIDE;
  } else if (mode == MODE_AUTO) {
    const Motion m = avoidStep(r, now, steerReady());
    v = min(m.v, (int16_t)AUTO_MAX_PCT);
    steer = m.steer;
    if (avoidStuck()) flags |= F_STUCK;
  }

  // Giới hạn tốc độ người dùng đặt chỉ áp cho động cơ kéo; góc lái giữ nguyên
  const uint8_t vmax = constrain(cmd.vmax, 10, 100);
  v = v * vmax / 100;
  v = limitForward(v, r);
  v = max(v, -(int16_t)REVERSE_MAX_PCT);
  driveUpdate(v, steer, estop);
}

Telem buildTelem() {
  const Ranges& r = sensorsRanges();
  Telem t;
  t.mode = activeMode;
  t.flags = flags;
  t.dLeft = r.left;
  t.dCenter = r.center;
  t.dRight = r.right;
  t.vbat_mV = batteryMv();
  t.batPct = batteryPct(t.vbat_mV);
  t.speed_mmps = driveSpeedMmps();
  t.pwmDrive = drivePwmPct();
  t.steer = steerPct();
  return t;
}

// Khoảng cách dạng 3 ký tự theo cm; "---" = ngoài tầm (không có vọng)
void fmtCm(char* out, uint16_t mm) {
  if (mm >= RANGE_MAX_MM) strcpy(out, "---");
  else snprintf(out, 4, "%3u", mm / 10);
}

// Dòng 1: trạng thái (cảnh báo ưu tiên hơn tên chế độ) + điện áp pin, ví dụ "TU HANH    12.4V"
// Dòng 2: khoảng cách trái / giữa / phải (cm) + dấu lớp an toàn, ví dụ "T 85 G120 P--- !"
//   '!' = tốc độ tiến bị ép về 0 vì vật cản, '-' = đang giảm tốc theo khoảng cách hoặc TTC
// LCD không có dấu tiếng Việt nên chữ viết không dấu.
void refreshLcd() {
  const Ranges& r = sensorsRanges();
  const char* label;
  if (flags & F_ESTOP) label = "KHAN CAP";
  else if (flags & F_LINK_LOST) label = "MAT KETNOI";
  else if (flags & F_SENSOR) label = "LOI GY-530";
  else if (flags & F_STUCK) label = "BI KET";
  else if (flags & F_LATCHED) label = "DA KHOA";
  else if (flags & F_OVERRIDE) label = "GHI DE TAY";
  else if (activeMode == MODE_AUTO) label = "TU HANH";
  else if (activeMode == MODE_MANUAL) label = "LAI TAY";
  else label = "DUNG";

  const uint16_t mv = batteryMv();
  char line[LCD_COLS + 1];
  snprintf(line, sizeof(line), "%-10s %2u.%uV", label, mv / 1000, (mv / 100) % 10);
  lcdText(0, line);

  char dl[4], dc[4], dr[4];
  fmtCm(dl, r.left);
  if (r.fault) strcpy(dc, "LOI");
  else fmtCm(dc, r.center);
  fmtCm(dr, r.right);
  const char mark = (flags & F_OBST_STOP) ? '!' : (flags & (F_SLOW | F_TTC)) ? '-' : ' ';
  snprintf(line, sizeof(line), "T%s G%s P%s %c", dl, dc, dr, mark);
  lcdText(1, line);
}

}  // namespace

void setup() {
  Serial.begin(115200);
  driveBegin();
  sensorsBegin();  // đã gọi Wire.begin()
  if (lcdBegin()) {
    lcdText(0, "WiFi: EV-DATN");
    lcdText(1, "192.168.4.1");
  }
}

void loop() {
  const uint32_t now = millis();
  readLink();
  sensorsUpdate();

  if (now - tControl >= CONTROL_MS) {
    tControl = now;
    control(now);
  }
  if (now - tTelem >= TELEM_MS) {
    tTelem = now;
    send(Serial, TELEM, buildTelem());
  }
  if (lcdOk()) {
    if (now >= LCD_SPLASH_MS && now - tLcd >= LCD_MS) {
      tLcd = now;
      refreshLcd();
    }
    lcdUpdate();
  }
}
