// Thuật toán tránh vật cản phản xạ 3 sector (trái 30° / giữa / phải 30°).
// Là dạng rút gọn của VFH: mỗi sector là một ô histogram, chọn hướng trống nhất;
// không còn hướng trống thì lùi rồi quay, thử quá MAX_RETRIES lần thì dừng hẳn.
#include "avoid.h"

#include "config.h"

namespace {

enum class State : uint8_t { Cruise, Backup, Turn, Stuck };

State state = State::Cruise;
uint32_t since = 0;
int8_t turnDir = 1;  // +1 quay phải, -1 quay trái
uint8_t retries = 0;

void enter(State s, uint32_t now) {
  state = s;
  since = now;
}

int8_t freerSide(const Ranges& r) { return r.right >= r.left ? 1 : -1; }

}  // namespace

void avoidReset() {
  state = State::Cruise;
  retries = 0;
}

bool avoidStuck() { return state == State::Stuck; }

Motion avoidStep(const Ranges& r, uint32_t now) {
  if (r.fault) return {0, 0};
  const uint16_t dMin = min(r.center, min(r.left, r.right));

  switch (state) {
    case State::Cruise: {
      // Phía trước hoặc cả hai bên quá gần: không đủ chỗ quay -> lùi
      if (r.center <= D_BACKUP_MM || (r.left <= D_BACKUP_MM && r.right <= D_BACKUP_MM)) {
        turnDir = freerSide(r);
        enter(State::Backup, now);
        return {0, 0};
      }
      // Phía trước bị chặn hoặc một bên sát vật -> quay tại chỗ về phía trống hơn
      if (r.center < D_BLOCK_MM || dMin <= D_BACKUP_MM) {
        turnDir = freerSide(r);
        enter(State::Turn, now);
        return {0, 0};
      }
      if (dMin > D_CLEAR_MM) retries = 0;
      // "Lực đẩy" hai bên: vật càng gần bên nào thì lách càng mạnh sang bên kia
      const int16_t pushL = r.left < D_SIDE_MM ? D_SIDE_MM - r.left : 0;
      const int16_t pushR = r.right < D_SIDE_MM ? D_SIDE_MM - r.right : 0;
      const int16_t turn = (int32_t)(pushL - pushR) * AUTO_SIDE_TURN_MAX / D_SIDE_MM;
      return {AUTO_CRUISE_PCT, turn};
    }

    case State::Backup:
      if (now - since >= BACKUP_MS) enter(State::Turn, now);
      return {-(int16_t)BACKUP_PCT, 0};

    case State::Turn: {
      // Chỉ thoát khi cả ba vùng đều thoáng, tránh vừa thoát đã bị lớp an toàn chặn lại
      if (r.center >= D_CLEAR_MM && min(r.left, r.right) >= D_BLOCK_MM) {
        enter(State::Cruise, now);
        return {0, 0};
      }
      if (now - since >= TURN_MAX_MS) {
        if (++retries >= MAX_RETRIES) {
          enter(State::Stuck, now);
          return {0, 0};
        }
        turnDir = -turnDir;  // hướng này không thoát được, lùi rồi thử phía kia
        enter(State::Backup, now);
      }
      return {0, (int16_t)(turnDir * AUTO_TURN_PCT)};
    }

    case State::Stuck:
    default:
      return {0, 0};
  }
}
