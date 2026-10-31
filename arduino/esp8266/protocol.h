// Giao thức UART giữa ATmega328P và ESP8266 (115200 baud, dùng chung cho cả hai chip).
//
// Khung: [0xAA][LEN][TYPE][PAYLOAD ...][CHK]
//   LEN = 1 + độ dài PAYLOAD (tính cả byte TYPE)
//   CHK = XOR của LEN, TYPE và mọi byte PAYLOAD
// Khung sai checksum bị bỏ qua, nên rác lúc ESP8266 khởi động không gây lệnh sai.
#pragma once
#include <stdint.h>
#include <string.h>

namespace proto {

constexpr uint8_t SOF = 0xAA;
constexpr uint8_t MAX_PAYLOAD = 24;

enum Type : uint8_t {
  CMD = 0x01,    // ESP8266 -> ATmega, chu kỳ 50 ms
  TELEM = 0x81,  // ATmega -> ESP8266, chu kỳ 100 ms
};

enum Mode : uint8_t {
  MODE_STOP = 0,
  MODE_MANUAL = 1,  // lái bằng joystick trên điện thoại (remote)
  MODE_AUTO = 2,    // tự tránh vật cản
};

// Cờ trạng thái trong Telem::flags
enum Flag : uint16_t {
  F_ESTOP = 1 << 0,      // nút dừng khẩn trên web
  // bit 1 để trống (trước đây là công tắc hành trình cản trước, đã bỏ)
  F_LINK_LOST = 1 << 2,  // quá 300 ms không nhận lệnh từ ESP8266
  F_SLOW = 1 << 3,       // đang giảm tốc theo khoảng cách
  F_OBST_STOP = 1 << 4,  // tốc độ tiến bị ép về 0 vì vật cản
  F_TTC = 1 << 5,        // thời gian tới va chạm < ngưỡng
  F_STUCK = 1 << 6,      // tự hành không tìm được hướng thoát
  F_LATCHED = 1 << 7,    // đã khóa sau sự cố, chờ lệnh STOP để mở
  F_SENSOR = 1 << 8,     // lỗi cảm biến GY-530 (VL53L0X)
  F_OVERRIDE = 1 << 9,   // remote đang ghi đè chế độ tự hành
};

struct __attribute__((packed)) Cmd {
  uint8_t mode;      // Mode
  int8_t throttle;   // -100..100 (%), dương = tiến
  int8_t steer;      // -100..100 (%), dương = rẽ phải
  uint8_t vmax;      // 10..100 (%), giới hạn tốc độ người dùng đặt
  uint8_t estop;     // 1 = dừng khẩn từ web
};

struct __attribute__((packed)) Telem {
  uint8_t mode;         // chế độ đang thực thi (sau khi xét an toàn)
  uint16_t flags;       // Flag
  uint16_t dLeft;       // mm, US-100 trái
  uint16_t dCenter;     // mm, GY-530 giữa
  uint16_t dRight;      // mm, US-100 phải
  uint16_t vbat_mV;
  uint8_t batPct;
  int16_t speed_mmps;   // có dấu, dương = tiến
  int8_t pwmL, pwmR;    // % thực xuất ra driver
};

static_assert(sizeof(Cmd) <= MAX_PAYLOAD, "Cmd quá lớn");
static_assert(sizeof(Telem) <= MAX_PAYLOAD, "Telem quá lớn");

class Parser {
 public:
  // Nạp từng byte; trả về true khi vừa nhận đủ một khung hợp lệ.
  bool feed(uint8_t b) {
    switch (st_) {
      case 0:
        if (b == SOF) st_ = 1;
        return false;
      case 1:
        if (b == 0 || b > MAX_PAYLOAD + 1) { st_ = 0; return false; }
        len_ = b; chk_ = b; idx_ = 0; st_ = 2;
        return false;
      case 2:
        buf_[idx_++] = b; chk_ ^= b;
        if (idx_ == len_) st_ = 3;
        return false;
      default:
        st_ = 0;
        return b == chk_;
    }
  }
  uint8_t type() const { return buf_[0]; }
  uint8_t payloadLen() const { return len_ - 1; }
  const uint8_t* payload() const { return buf_ + 1; }

  template <class T>
  bool read(uint8_t expectedType, T& out) const {
    if (type() != expectedType || payloadLen() != sizeof(T)) return false;
    memcpy(&out, payload(), sizeof(T));
    return true;
  }

 private:
  uint8_t st_ = 0, len_ = 0, idx_ = 0, chk_ = 0;
  uint8_t buf_[MAX_PAYLOAD + 1];
};

template <class S, class T>
void send(S& serial, uint8_t type, const T& data) {
  const uint8_t* p = reinterpret_cast<const uint8_t*>(&data);
  const uint8_t len = sizeof(T) + 1;
  uint8_t chk = len ^ type;
  for (uint8_t i = 0; i < sizeof(T); i++) chk ^= p[i];
  const uint8_t hdr[3] = {SOF, len, type};
  serial.write(hdr, 3);
  serial.write(p, sizeof(T));
  serial.write(chk);
}

}  // namespace proto
