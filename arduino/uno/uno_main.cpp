// Firmware ATmega328P: tầng thời gian thực của xe.
//  - Đọc 2 US-100 (trái/phải) + GY-530 (giữa), điện áp pin
//  - Nhận lệnh từ ESP8266 (remote trên điện thoại), chạy thuật toán tránh vật cản
//  - Lớp an toàn luôn chạy, kể cả khi lái tay; ESP8266 treo thì tự dừng sau 300 ms
//  - Điều khiển 2 module BTS7960, gửi trạng thái về ESP8266 (hiển thị trên điện thoại)
#include <Arduino.h>

#include "avoid.h"
#include "config.h"
#include "drive.h"
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
uint32_t tControl = 0, tTelem = 0;

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

  int16_t v = 0, turn = 0;
  const bool stickActive = abs(cmd.throttle) > STICK_DEADBAND || abs(cmd.steer) > STICK_DEADBAND;
  if (mode == MODE_MANUAL || (mode == MODE_AUTO && stickActive)) {
    // Remote luôn thắng lệnh tự động
    v = cmd.throttle;
    turn = cmd.steer;
    if (mode == MODE_AUTO) flags |= F_OVERRIDE;
  } else if (mode == MODE_AUTO) {
    const Motion m = avoidStep(r, now);
    v = min(m.v, (int16_t)AUTO_MAX_PCT);
    turn = m.turn;
    if (avoidStuck()) flags |= F_STUCK;
  }

  const uint8_t vmax = constrain(cmd.vmax, 10, 100);
  v = v * vmax / 100;
  turn = turn * vmax / 100;
  v = limitForward(v, r);
  v = max(v, -(int16_t)REVERSE_MAX_PCT);

  // Lái vi sai: trái = v + rẽ, phải = v − rẽ; chia tỉ lệ nếu vượt 100 %
  int16_t left = v + turn, right = v - turn;
  const int16_t peak = max(abs(left), abs(right));
  if (peak > 100) {
    left = (int32_t)left * 100 / peak;
    right = (int32_t)right * 100 / peak;
  }
  driveUpdate(left, right, estop);
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
  t.pwmL = drivePwmPct(0);
  t.pwmR = drivePwmPct(1);
  return t;
}

}  // namespace

void setup() {
  Serial.begin(115200);
  driveBegin();
  sensorsBegin();
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
}
