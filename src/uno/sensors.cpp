#include "sensors.h"

#include <VL53L0X.h>
#include <Wire.h>

#include "config.h"

namespace {

// ---------- GY-530 (VL53L0X) ----------
VL53L0X tof;
bool tofOk = false;
uint32_t tofLastMs = 0;
uint32_t tofPollMs = 0;

// ---------- US-100 chế độ xung (đã rút jumper) ----------
// Độ rộng xung Echo đo bằng ngắt đổi mức PCINT0 nên vòng lặp không bị pulseIn() chặn.
const uint8_t trigPin[2] = {PIN_US_L_TRIG, PIN_US_R_TRIG};
uint8_t echoMask[2];
volatile uint8_t pinbPrev;
volatile uint32_t echoRise[2];
volatile uint16_t echoUs[2];
volatile bool echoDone[2];
uint8_t usActive = 0;
uint32_t usTrigMs = 0;
uint16_t usHist[2][3];
uint8_t usHistIdx[2];

Ranges rng = {0, 0, 0, true};
float vbatMv = 0;
uint32_t vbatMs = 0;

uint16_t median3(uint16_t a, uint16_t b, uint16_t c) {
  return max(min(a, b), min(max(a, b), c));
}

float readVbatMv() {
  return analogRead(PIN_VBAT) * (float)VREF_MV / 1023.0f * VBAT_DIVIDER;
}

void triggerUs(uint8_t i) {
  noInterrupts();
  echoDone[i] = false;
  interrupts();
  digitalWrite(trigPin[i], HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin[i], LOW);
}

uint16_t finishUs(uint8_t i) {
  noInterrupts();
  const bool done = echoDone[i];
  const uint16_t us = echoUs[i];
  interrupts();
  if (!done) return RANGE_MAX_MM;  // không có vọng trong chu kỳ -> coi như trống
  const uint32_t mm = (uint32_t)us * 343 / 2000;  // v_âm = 343 m/s, đi và về
  return mm > RANGE_MAX_MM ? RANGE_MAX_MM : mm;
}

void updateUltrasonic(uint32_t now) {
  if (now - usTrigMs < US_PERIOD_MS) return;
  const uint8_t i = usActive;
  usHist[i][usHistIdx[i]] = finishUs(i);
  usHistIdx[i] = (usHistIdx[i] + 1) % 3;
  const uint16_t mm = median3(usHist[i][0], usHist[i][1], usHist[i][2]);
  if (i == 0) rng.left = mm; else rng.right = mm;

  // Kích lần lượt từng cảm biến để tránh nhiễu chéo (đề cương bảng 5.2)
  usActive ^= 1;
  triggerUs(usActive);
  usTrigMs = now;
}

void updateTof(uint32_t now) {
  if (tofOk && now - tofPollMs >= 5) {
    tofPollMs = now;
    if (tof.readReg(VL53L0X::RESULT_INTERRUPT_STATUS) & 0x07) {
      const uint16_t mm = tof.readReg16Bit(VL53L0X::RESULT_RANGE_STATUS + 10);
      tof.writeReg(VL53L0X::SYSTEM_INTERRUPT_CLEAR, 0x01);
      rng.center = mm >= TOF_MAX_MM ? RANGE_MAX_MM : mm;
      tofLastMs = now;
    }
  }
  rng.fault = !tofOk || now - tofLastMs > 200;
  if (rng.fault) rng.center = 0;  // an toàn: mất cảm biến giữa thì coi như phía trước bị chặn
}

}  // namespace

ISR(PCINT0_vect) {
  const uint8_t now = PINB;
  const uint8_t changed = now ^ pinbPrev;
  pinbPrev = now;
  const uint32_t t = micros();
  for (uint8_t i = 0; i < 2; i++) {
    if (!(changed & echoMask[i])) continue;
    if (now & echoMask[i]) {
      echoRise[i] = t;
    } else {
      const uint32_t w = t - echoRise[i];
      echoUs[i] = w > 60000 ? 60000 : w;
      echoDone[i] = true;
    }
  }
}

void sensorsBegin() {
  for (uint8_t i = 0; i < 2; i++) {
    pinMode(trigPin[i], OUTPUT);
    digitalWrite(trigPin[i], LOW);
  }
  pinMode(PIN_US_L_ECHO, INPUT);
  pinMode(PIN_US_R_ECHO, INPUT);
  echoMask[0] = digitalPinToBitMask(PIN_US_L_ECHO);
  echoMask[1] = digitalPinToBitMask(PIN_US_R_ECHO);
  pinbPrev = PINB;
  PCMSK0 |= echoMask[0] | echoMask[1];  // bit PCMSK0 trùng bit PORTB
  PCICR |= _BV(PCIE0);

  Wire.begin();
  Wire.setClock(400000);
  Wire.setWireTimeout(3000, true);  // bus I2C treo không làm treo cả vòng điều khiển
  tof.setTimeout(50);
  tofOk = tof.init();
  if (tofOk) {
    // Cấu hình tầm xa (~2 m) theo ví dụ của thư viện Pololu
    tof.setSignalRateLimit(0.1);
    tof.setVcselPulsePeriod(VL53L0X::VcselPeriodPreRange, 18);
    tof.setVcselPulsePeriod(VL53L0X::VcselPeriodFinalRange, 14);
    tof.setMeasurementTimingBudget(33000);
    tof.startContinuous();
    tofLastMs = millis();
  }

  pinMode(PIN_VBAT, INPUT);
  vbatMv = readVbatMv();
}

void sensorsUpdate() {
  const uint32_t now = millis();
  updateUltrasonic(now);
  updateTof(now);
  if (now - vbatMs >= 50) {
    vbatMs = now;
    vbatMv += 0.05f * (readVbatMv() - vbatMv);  // lọc thông thấp, bớt dao động do dòng động cơ
  }
}

const Ranges& sensorsRanges() { return rng; }

uint16_t batteryMv() { return (uint16_t)vbatMv; }

uint8_t batteryPct(uint16_t mv) {
  if (mv <= VBAT_EMPTY_MV) return 0;
  if (mv >= VBAT_FULL_MV) return 100;
  return (uint32_t)(mv - VBAT_EMPTY_MV) * 100 / (VBAT_FULL_MV - VBAT_EMPTY_MV);
}
