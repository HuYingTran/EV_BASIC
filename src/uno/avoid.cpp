// Thuật toán tránh vật cản phản xạ 3 sector (trái 45° / giữa / phải 45°) cho xe lái bánh trước.
// Là dạng rút gọn của VFH: mỗi sector là một ô histogram, chọn hướng trống nhất.
// Xe dẫn động bánh sau + đánh lái bánh trước nên KHÔNG quay tại chỗ được:
//   - phía trước bị chặn: bẻ hết lái về phía trống hơn rồi tiến chậm (ôm cua);
//   - quá gần, không đủ chỗ ôm cua: lùi với bánh trước bẻ NGƯỢC (đầu xe xoay về phía trống),
//     rồi ôm cua tiếp (kiểu quay đầu 3 điểm);
//   - lùi liên tiếp quá MAX_RETRIES lần mà không thoát thì dừng hẳn, báo kẹt.
// Mọi lần đổi góc lái lớn đều đứng chờ động cơ lái bẻ xong (steerReady) mới chạy, vì động cơ lái
// cần tới STEER_FULL_MS để đi từ thẳng ra hết cỡ.
#include "avoid.h"

#include "config.h"

namespace {

enum class State : uint8_t { Cruise, Backup, Turn, Stuck };

State state = State::Cruise;
uint32_t since = 0;
int8_t turnDir = 1;  // +1 vòng sang phải, -1 vòng sang trái
uint8_t retries = 0;

void enter(State s, uint32_t now) {
  state = s;
  since = now;
}

int8_t freerSide(const Ranges& r) { return r.right >= r.left ? 1 : -1; }

void startBackup(const Ranges& r, uint32_t now) {
  turnDir = freerSide(r);
  enter(++retries > MAX_RETRIES ? State::Stuck : State::Backup, now);
}

}  // namespace

void avoidReset() {
  state = State::Cruise;
  retries = 0;
}

bool avoidStuck() { return state == State::Stuck; }

Motion avoidStep(const Ranges& r, uint32_t now, bool steerReady) {
  if (r.fault) return {0, 0};
  const uint16_t dMin = min(r.center, min(r.left, r.right));

  switch (state) {
    case State::Cruise: {
      // Phía trước hoặc cả hai bên quá gần: không đủ chỗ ôm cua -> lùi
      if (r.center <= D_BACKUP_MM || (r.left <= D_BACKUP_MM && r.right <= D_BACKUP_MM)) {
        startBackup(r, now);
        return {0, 0};
      }
      // Phía trước bị chặn hoặc một bên sát vật -> ôm cua về phía trống hơn
      if (r.center < D_BLOCK_MM || dMin <= D_BACKUP_MM) {
        turnDir = freerSide(r);
        enter(State::Turn, now);
        return {0, (int16_t)(turnDir * 100)};
      }
      if (dMin > D_CLEAR_MM) retries = 0;
      // "Lực đẩy" hai bên: vật càng gần bên nào thì lái càng mạnh sang bên kia (vừa chạy vừa lái)
      const int16_t pushL = r.left < D_SIDE_MM ? D_SIDE_MM - r.left : 0;
      const int16_t pushR = r.right < D_SIDE_MM ? D_SIDE_MM - r.right : 0;
      const int16_t steer = (int32_t)(pushL - pushR) * AUTO_SIDE_STEER_MAX / D_SIDE_MM;
      return {AUTO_CRUISE_PCT, steer};
    }

    case State::Backup: {
      // Lùi với bánh trước bẻ ngược hướng muốn vòng: đuôi xe đi về phía vật, đầu xe xoay ra chỗ trống
      const int16_t steer = -turnDir * 100;
      if (!steerReady) {
        since = now;  // chưa bẻ lái xong thì chưa tính thời gian lùi
        return {0, steer};
      }
      if (now - since >= BACKUP_MS) {
        enter(State::Turn, now);
        return {0, (int16_t)(turnDir * 100)};
      }
      return {-(int16_t)BACKUP_PCT, steer};
    }

    case State::Turn: {
      const int16_t steer = turnDir * 100;
      // Chỉ thoát khi cả ba vùng đều thoáng, tránh vừa thoát đã bị lớp an toàn chặn lại
      if (r.center >= D_CLEAR_MM && min(r.left, r.right) >= D_BLOCK_MM) {
        enter(State::Cruise, now);
        return {0, 0};
      }
      // Phía trước hoặc phía đang vòng vào đã quá sát, hoặc vòng quá lâu: lùi lấy chỗ rồi thử lại
      const uint16_t dInner = turnDir > 0 ? r.right : r.left;
      if (r.center <= D_BACKUP_MM || dInner <= D_BACKUP_MM || now - since >= TURN_MAX_MS) {
        startBackup(r, now);
        return {0, 0};
      }
      if (!steerReady) {
        since = now;
        return {0, steer};
      }
      return {AUTO_TURN_PCT, steer};
    }

    case State::Stuck:
    default:
      return {0, 0};
  }
}
