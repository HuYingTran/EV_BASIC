// Điều khiển 2 module BTS7960: module trái lái cặp bánh trái, module phải lái cặp bánh phải, lái kiểu vi sai.
// R_EN = L_EN luôn = 1, băm xung ở RPWM/LPWM:
//   Tiến: RPWM = PWM, LPWM = 0     Lùi: RPWM = 0, LPWM = PWM
//   Pha nghỉ / PWM = 0: RPWM = LPWM = 0 -> cả hai khóa dưới dẫn, động cơ nối ngắn -> hãm động năng.
//
// Mỗi bên có:
//   - lệnh đã khởi động mềm `cmd` (chỉ giới hạn tốc độ TĂNG, giảm thì theo ngay);
//   - tốc độ ước lượng `est` tính bằng mô hình bậc nhất của chương 2: est' = (v_xl − est) / τ,
//     v_xl là tốc độ xác lập ứng với độ rộng xung đang xuất (đã bù vùng chết nên v_xl = lệnh %).
// Khi est lớn hơn lệnh quá BRAKE_HYST_PCT (hoặc lệnh = 0) thì HÃM (PWM = 0) tới khi est về lệnh;
// mô hình cho thấy chỉ hạ độ rộng xung thì xe trôi rất chậm (τ ≈ 3,9 s).
#include "drive.h"

#include "config.h"

namespace {

const uint8_t pinFwd[2] = {PIN_L_IN1, PIN_R_IN3};
const uint8_t pinRev[2] = {PIN_L_IN2, PIN_R_IN4};

// Tốc độ xác lập (theo % VMAX) khi hãm hoàn toàn (PWM = 0): giá trị âm "ảo" của mô hình tuyến tính
constexpr float BRAKE_SS_PCT = PWM_MIN_PCT * 100.0f / (100 - PWM_MIN_PCT);
constexpr float DT_S = CONTROL_MS / 1000.0f;

float cmd[2];           // lệnh sau khởi động mềm, %
float est[2];           // tốc độ ước lượng, % của VMAX_MMPS
bool braking[2];
int16_t pwmOut[2];      // -255..255
int16_t speedMmps[2];

void writeSide(uint8_t i, int16_t pwm) {
  pwm = constrain(pwm, -255, 255);
  pwmOut[i] = pwm;
  if (pwm >= 0) {
    analogWrite(pinRev[i], 0);
    analogWrite(pinFwd[i], pwm);
  } else {
    analogWrite(pinFwd[i], 0);
    analogWrite(pinRev[i], -pwm);
  }
}

// Bù vùng chết: lệnh p % tốc độ -> độ rộng xung PWM_MIN + (100 − PWM_MIN)·p (tốc độ gần tuyến tính theo p)
float feedForward(float pct) {
  if (pct == 0) return 0;
  const float duty = PWM_MIN_PCT + (100 - PWM_MIN_PCT) * fabsf(pct) / 100.0f;
  return (pct > 0 ? duty : -duty) * 2.55f;
}

// Cập nhật lệnh khởi động mềm, quyết định hãm và tốc độ ước lượng của một bên.
// Trả về true nếu chu kỳ này phải hãm.
bool updateSide(uint8_t i, float tgt) {
  // Khởi động mềm: chỉ hạn chế khi |lệnh| tăng hoặc đổi chiều
  const float step = SOFTSTART_PCT_PER_S * DT_S;
  if (fabsf(tgt) > fabsf(cmd[i]) || tgt * cmd[i] < 0) {
    if (tgt * cmd[i] < 0) cmd[i] = 0;
    cmd[i] = tgt > cmd[i] ? min(cmd[i] + step, tgt) : max(cmd[i] - step, tgt);
  } else {
    cmd[i] = tgt;
  }
  const float c = cmd[i];
  const float e = est[i];

  // Hãm khi tốc độ ước lượng vượt lệnh cùng chiều quá ngưỡng, khi lệnh = 0, hoặc khi đổi chiều
  const bool over = (e > 0 && (c <= 0 || e - c > BRAKE_HYST_PCT)) ||
                    (e < 0 && (c >= 0 || c - e > BRAKE_HYST_PCT));
  if (over) braking[i] = true;
  if (braking[i]) {
    const float stopAt = e > 0 ? max(c, 0.0f) : min(c, 0.0f);
    est[i] = e + ((e > 0 ? -BRAKE_SS_PCT : BRAKE_SS_PCT) - e) * DT_S / TAU_S;
    if ((e > 0 && est[i] <= stopAt) || (e < 0 && est[i] >= stopAt) || e == 0) {
      est[i] = stopAt;
      braking[i] = false;
    }
    return braking[i];
  }
  est[i] = e + (c - e) * DT_S / TAU_S;
  return false;
}

}  // namespace

void driveBegin() {
  for (uint8_t i = 0; i < 2; i++) {
    pinMode(pinFwd[i], OUTPUT);
    pinMode(pinRev[i], OUTPUT);
    writeSide(i, 0);
  }
  // Timer1 (D9/D10) sang Fast PWM 8 bit ~980 Hz cho giống Timer0 (D5/D6)
  TCCR1A = (TCCR1A & ~_BV(WGM11)) | _BV(WGM10);
  TCCR1B = (TCCR1B & ~_BV(WGM13)) | _BV(WGM12);

  pinMode(PIN_MOTOR_EN, OUTPUT);
  digitalWrite(PIN_MOTOR_EN, HIGH);
}

void driveUpdate(int16_t left, int16_t right, bool brake) {
  const int16_t tgtIn[2] = {left, right};
  for (uint8_t i = 0; i < 2; i++) {
    const float tgt = brake ? 0 : constrain(tgtIn[i], -100, 100);
    const bool brakeNow = updateSide(i, tgt);

    writeSide(i, brakeNow ? 0 : feedForward(cmd[i]));
    speedMmps[i] = est[i] * VMAX_MMPS / 100.0f;  // không có encoder: dùng tốc độ ước lượng
  }
}

int16_t driveSpeedMmps() { return (speedMmps[0] + speedMmps[1]) / 2; }

int8_t drivePwmPct(uint8_t side) { return (int32_t)pwmOut[side] * 100 / 255; }
