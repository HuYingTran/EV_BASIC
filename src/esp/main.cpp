// Firmware ESP8266: cầu nối WiFi giữa điện thoại và ATmega328P.
//  - Phát WiFi "EV-DATN", trang điều khiển tại http://192.168.4.1
//  - Trình duyệt gửi lệnh qua WebSocket (cổng 81) mỗi 100 ms
//  - Chuyển lệnh sang ATmega qua UART mỗi 50 ms; trình duyệt im quá 500 ms thì gửi STOP
//  - Nhận gói trạng thái từ ATmega, đẩy JSON lên mọi trình duyệt đang mở
// Lưu ý: UART0 nối thẳng sang ATmega nên tuyệt đối không in debug ra Serial.
#include <Arduino.h>
#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h>
#include <WebSocketsServer.h>

#include "protocol.h"
#include "web_page.h"

namespace {

using namespace proto;

const char* AP_SSID = "EV-DATN";
const char* AP_PASS = "datn2026";  // tối thiểu 8 ký tự

constexpr uint32_t CMD_PERIOD_MS = 50;
constexpr uint32_t PUSH_PERIOD_MS = 200;
constexpr uint32_t WEB_TIMEOUT_MS = 500;
constexpr uint32_t UNO_TIMEOUT_MS = 500;
constexpr uint8_t NO_CLIENT = 0xFF;

ESP8266WebServer http(80);
WebSocketsServer ws(81);
Parser parser;

Cmd cmd = {MODE_STOP, 0, 0, 100, 0};
Telem telem = {};
uint32_t lastWebMs = 0, lastTelemMs = 0, tCmd = 0, tPush = 0;
uint8_t controller = NO_CLIENT;  // chỉ một điện thoại được lái; máy khác chỉ xem và dừng khẩn
uint8_t estopMask = 0;           // bit i = client i đang giữ dừng khẩn

void handleText(uint8_t num, const char* s) {
  int m, t, st, v, e;
  if (sscanf(s, "C,%d,%d,%d,%d,%d", &m, &t, &st, &v, &e) != 5) return;
  if (num < 8) {
    if (e) estopMask |= 1 << num;
    else estopMask &= ~(1 << num);
  }
  if (controller == NO_CLIENT) controller = num;
  if (num != controller) return;
  cmd.mode = m <= MODE_AUTO ? m : MODE_STOP;
  cmd.throttle = constrain(t, -100, 100);
  cmd.steer = constrain(st, -100, 100);
  cmd.vmax = constrain(v, 10, 100);
  lastWebMs = millis();
}

void onWsEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t len) {
  switch (type) {
    case WStype_DISCONNECTED:
      if (num < 8) estopMask &= ~(1 << num);
      if (num == controller) {
        controller = NO_CLIENT;
        cmd.mode = MODE_STOP;
        cmd.throttle = cmd.steer = 0;
      }
      break;
    case WStype_TEXT: {
      char buf[48];
      const size_t n = len < sizeof buf - 1 ? len : sizeof buf - 1;
      memcpy(buf, payload, n);
      buf[n] = '\0';
      handleText(num, buf);
      break;
    }
    default:
      break;
  }
}

void readUno() {
  while (Serial.available()) {
    if (parser.feed(Serial.read()) && parser.read(TELEM, telem)) lastTelemMs = millis();
  }
}

void sendCmd(uint32_t now) {
  Cmd out = cmd;
  if (now - lastWebMs > WEB_TIMEOUT_MS) {
    out.mode = MODE_STOP;
    out.throttle = out.steer = 0;
  }
  out.estop = estopMask ? 1 : 0;
  send(Serial, CMD, out);
}

void pushTelem(uint32_t now) {
  if (ws.connectedClients() == 0) return;
  char json[200];
  const int n = snprintf(json, sizeof json,
                         "{\"m\":%u,\"f\":%u,\"l\":%u,\"c\":%u,\"r\":%u,\"v\":%u,\"p\":%u,"
                         "\"s\":%d,\"pd\":%d,\"sa\":%d,\"u\":%d,\"ctl\":%d}",
                         telem.mode, telem.flags, telem.dLeft, telem.dCenter, telem.dRight,
                         telem.vbat_mV, telem.batPct, telem.speed_mmps, telem.pwmDrive, telem.steer,
                         now - lastTelemMs < UNO_TIMEOUT_MS ? 1 : 0,
                         controller == NO_CLIENT ? -1 : controller);
  ws.broadcastTXT(json, n);
}

}  // namespace

void setup() {
  Serial.begin(115200);
  Serial.setDebugOutput(false);

  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASS);

  http.on("/", [] { http.send_P(200, "text/html; charset=utf-8", WEB_PAGE); });
  http.onNotFound([] { http.send(404, "text/plain", "404"); });
  http.begin();

  ws.begin();
  ws.onEvent(onWsEvent);
}

void loop() {
  const uint32_t now = millis();
  http.handleClient();
  ws.loop();
  readUno();

  if (now - tCmd >= CMD_PERIOD_MS) {
    tCmd = now;
    sendCmd(now);
  }
  if (now - tPush >= PUSH_PERIOD_MS) {
    tPush = now;
    pushTelem(now);
  }
}
