// Điều khiển 2 module BTS7960:
//   - module KÉO: 2 động cơ bánh sau mắc song song (tiến / lùi)
//   - module LÁI: động cơ đánh lái 2 bánh trước
// R_EN = L_EN luôn = 1, băm xung ở RPWM/LPWM:
//   Chiều thuận: RPWM = PWM, LPWM = 0     Chiều nghịch: RPWM = 0, LPWM = PWM
//   PWM = 0: RPWM = LPWM = 0 -> cả hai khóa dưới dẫn, động cơ nối ngắn -> hãm động năng.
//
// Động cơ kéo:
//   - lệnh đã khởi động mềm `cmd` (chỉ giới hạn tốc độ TĂNG, giảm thì theo ngay);
//   - tốc độ ước lượng `est` tính bằng mô hình bậc nhất của chương 2: est' = (v_xl − est) / τ,
//     v_xl là tốc độ xác lập ứng với độ rộng xung đang xuất (đã bù vùng chết nên v_xl = lệnh %).
//   Khi est lớn hơn lệnh quá BRAKE_HYST_PCT (hoặc lệnh = 0) thì HÃM (PWM = 0) tới khi est về lệnh;
//   mô hình cho thấy chỉ hạ độ rộng xung thì xe trôi rất chậm (τ ≈ 3,9 s).
//
// Động cơ lái (vòng hở, không cảm biến góc):
//   - góc ước lượng tăng/giảm đều 100 % sau mỗi STEER_FULL_MS khi động cơ đang chạy ở STEER_PWM_PCT;
//   - sai lệch với góc lệnh > STEER_TOL_PCT thì chạy về phía đích, ngược lại tắt (hãm giữ chỗ:
//     không có lò xo nên bánh đứng yên nhờ ma sát hộp số);
//   - lệnh hết cỡ: tới ±100 % thì đẩy thêm STEER_PUSH_MS vào chặn cơ khí -> góc thật chắc chắn
//     bằng ±100 %, xóa sai số tích lũy của ước lượng;
//   - khởi động: bẻ hết cỡ trái trong 2·STEER_FULL_MS + STEER_PUSH_MS (đủ đi hết hành trình từ
//     vị trí bất kỳ) để gốc ước lượng là −100 %, sau đó mới lái theo lệnh.
#include "drive.h"

#include "config.h"

namespace {

// Tốc độ xác lập (theo % VMAX) khi hãm hoàn toàn (PWM = 0): giá trị âm "ảo" của mô hình tuyến tính
constexpr float BRAKE_SS_PCT = PWM_MIN_PCT * 100.0f / (100 - PWM_MIN_PCT);
constexpr float DT_S = CONTROL_MS / 1000.0f;
constexpr float STEER_STEP = 100.0f * CONTROL_MS / STEER_FULL_MS;  // % góc mỗi chu kỳ
constexpr int16_t STEER_PWM = STEER_PWM_PCT * 255 / 100;

// ---------- kéo ----------
float cmd;          // lệnh sau khởi động mềm, %
float est;          // tốc độ ước lượng, % của VMAX_MMPS
bool braking;
int16_t pwmDrive;   // -255..255

// ---------- lái ----------
float steerEst = 0;       // góc ước lượng, %
int16_t steerTarget = 0;  // góc lệnh, %
uint16_t pushMs = 0;      // thời gian đã đẩy vào chặn ở lần hết cỡ hiện tại
int16_t calibMs = 2 * STEER_FULL_MS + STEER_PUSH_MS;  // > 0: đang hiệu chuẩn lúc khởi động

void writeMotor(uint8_t pinA, uint8_t pinB, int16_t pwm) {
  pwm = constrain(pwm, -255, 255);
  if (pwm >= 0) {
    analogWrite(pinB, 0);
    analogWrite(pinA, pwm);
  } else {
    analogWrite(pinA, 0);
    analogWrite(pinB, -pwm);
  }
}

// Bù vùng chết: lệnh p % tốc độ -> độ rộng xung PWM_MIN + (100 − PWM_MIN)·p (tốc độ gần tuyến tính theo p)
float feedForward(float pct) {
  if (pct == 0) return 0;
  const float duty = PWM_MIN_PCT + (100 - PWM_MIN_PCT) * fabsf(pct) / 100.0f;
  return (pct > 0 ? duty : -duty) * 2.55f;
}

// Cập nhật lệnh khởi động mềm, quyết định hãm và tốc độ ước lượng. Trả về true nếu chu kỳ này phải hãm.
bool updateDrive(float tgt) {
  // Khởi động mềm: chỉ hạn chế khi |lệnh| tăng hoặc đổi chiều
  const float step = SOFTSTART_PCT_PER_S * DT_S;
  if (fabsf(tgt) > fabsf(cmd) || tgt * cmd < 0) {
    if (tgt * cmd < 0) cmd = 0;
    cmd = tgt > cmd ? min(cmd + step, tgt) : max(cmd - step, tgt);
  } else {
    cmd = tgt;
  }
  const float c = cmd;
  const float e = est;

  // Hãm khi tốc độ ước lượng vượt lệnh cùng chiều quá ngưỡng, khi lệnh = 0, hoặc khi đổi chiều
  const bool over = (e > 0 && (c <= 0 || e - c > BRAKE_HYST_PCT)) ||
                    (e < 0 && (c >= 0 || c - e > BRAKE_HYST_PCT));
  if (over) braking = true;
  if (braking) {
    const float stopAt = e > 0 ? max(c, 0.0f) : min(c, 0.0f);
    est = e + ((e > 0 ? -BRAKE_SS_PCT : BRAKE_SS_PCT) - e) * DT_S / TAU_S;
    if ((e > 0 && est <= stopAt) || (e < 0 && est >= stopAt) || e == 0) {
      est = stopAt;
      braking = false;
    }
    return braking;
  }
  est = e + (c - e) * DT_S / TAU_S;
  return false;
}

// Trả về chiều chạy động cơ lái chu kỳ này: -1 trái, 0 tắt, +1 phải
int8_t updateSteer(bool hold) {
  if (hold) return 0;  // dừng khẩn: động cơ lái đứng yên tại chỗ, ước lượng giữ nguyên

  if (calibMs > 0) {
    calibMs -= CONTROL_MS;
    if (calibMs <= 0) steerEst = -100;  // đã chắc chắn nằm ở chặn trái
    return -1;
  }

  // Lệnh hết cỡ: chạy tới chặn rồi đẩy thêm STEER_PUSH_MS để căn lại ước lượng
  if (abs(steerTarget) >= 100) {
    const int8_t dir = steerTarget > 0 ? 1 : -1;
    if (steerEst * dir < 100) {
      pushMs = 0;
      return dir;
    }
    if (pushMs < STEER_PUSH_MS) {
      pushMs += CONTROL_MS;
      return dir;
    }
    return 0;
  }
  pushMs = 0;

  const float err = steerTarget - steerEst;
  if (fabsf(err) <= STEER_TOL_PCT) return 0;
  return err > 0 ? 1 : -1;
}

}  // namespace

void driveBegin() {
  pinMode(PIN_DRIVE_FWD, OUTPUT);
  pinMode(PIN_DRIVE_REV, OUTPUT);
  pinMode(PIN_STEER_R, OUTPUT);
  pinMode(PIN_STEER_L, OUTPUT);
  writeMotor(PIN_DRIVE_FWD, PIN_DRIVE_REV, 0);
  writeMotor(PIN_STEER_R, PIN_STEER_L, 0);
  // Timer1 (D9/D10) sang Fast PWM 8 bit ~980 Hz cho giống Timer0 (D5/D6)
  TCCR1A = (TCCR1A & ~_BV(WGM11)) | _BV(WGM10);
  TCCR1B = (TCCR1B & ~_BV(WGM13)) | _BV(WGM12);

  pinMode(PIN_MOTOR_EN, OUTPUT);
  digitalWrite(PIN_MOTOR_EN, HIGH);
}

void driveUpdate(int16_t v, int16_t steer, bool brake) {
  const float tgt = brake ? 0 : constrain(v, -100, 100);
  const bool brakeNow = updateDrive(tgt);
  pwmDrive = brakeNow ? 0 : feedForward(cmd);
  writeMotor(PIN_DRIVE_FWD, PIN_DRIVE_REV, pwmDrive);

  steerTarget = constrain(steer, -100, 100);
  const int8_t dir = updateSteer(brake);
  if (dir && calibMs <= 0) steerEst = constrain(steerEst + dir * STEER_STEP, -100.0f, 100.0f);
  writeMotor(PIN_STEER_R, PIN_STEER_L, dir * STEER_PWM);
}

int16_t driveSpeedMmps() { return est * VMAX_MMPS / 100.0f; }  // không có encoder: dùng tốc độ ước lượng

int8_t drivePwmPct() { return (int32_t)pwmDrive * 100 / 255; }

int8_t steerPct() { return (int8_t)steerEst; }

bool steerReady() {
  if (calibMs > 0) return false;
  if (abs(steerTarget) >= 100) return steerEst * (steerTarget > 0 ? 1 : -1) >= 100;
  return fabsf(steerTarget - steerEst) <= STEER_TOL_PCT;
}
