// BASIRA firmware for ESP32-S3-DevKitC-1 (Arduino-ESP32 core 3.x).
// Links: USB serial (460800 baud, JSON lines) and Wi-Fi access point (app at http://192.168.4.1, WebSocket at /ws).
// One external library: ArduinoJson 7.x. Build: see firmware/README.md.
#include <Arduino.h>
#include <SPI.h>
#include <Preferences.h>
#include "config.h"
#include "device.h"
#include "protocol.h"
#include "hal_esp32.h"
#include "net_esp32.h"

using namespace basira;

static Drv8833Motor motors[6];
static As5048a sensors[6];
static Esp32Board board;
static Device device;
static Protocol proto;
static Preferences prefs;

struct Sink : ISink {
  void send(const char* json) override {
    Serial.println(json);
    net::broadcast(json);
  }
} sink;

static void saveConfig(int eye, const EyeConfig& c) {
  prefs.putBytes(eye == Device::L ? "cfgL" : "cfgR", &c, sizeof(c));
}

static EyeConfig loadConfig(int eye) {
  EyeConfig c;
  EyeConfig stored;
  size_t n = prefs.getBytes(eye == Device::L ? "cfgL" : "cfgR", &stored, sizeof(stored));
  if (n == sizeof(stored) && stored.magic == c.magic) return stored;
  return c;   // nominal defaults (fitted = false)
}

void setup() {
  Serial.begin(SERIAL_BAUD);
  SPI.begin(PIN_SPI_SCK, PIN_SPI_MISO, PIN_SPI_MOSI);
  board.begin();
  IMotor* m[6];
  ISensor* s[6];
  for (int i = 0; i < 6; i++) {
    motors[i].begin(PIN_MOTOR_PWM[i], PIN_MOTOR_DIR[i]);
    sensors[i].begin(&SPI, PIN_SENSOR_CS[i]);
    m[i] = &motors[i];
    s[i] = &sensors[i];
  }
  prefs.begin("basira", false);
  device.begin(m, s, &board, &proto, loadConfig(Device::R), loadConfig(Device::L));
  proto.begin(&device, &sink, saveConfig);

  net::begin();
  proto.sendInfo();
}

void loop() {
  static char line[512];
  static size_t n = 0;
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\n' || c == '\r') {
      if (n) { line[n] = 0; proto.handleLine(line); n = 0; }
    } else if (n < sizeof(line) - 1) {
      line[n++] = c;
    } else {
      n = 0;   // over-long line: drop it
    }
  }
  net::Line ln;
  while (net::poll(ln)) proto.handleLine(ln.s);
  if (net::takeClosed()) device.linkClosed();
  if (net::takeOpened()) { proto.sendInfo(); proto.sendStatus(); }

  static uint32_t lastTick = 0, lastStatus = 0;
  uint32_t us = micros();
  if (us - lastTick >= CONTROL_PERIOD_US) {
    lastTick = us;
    device.tick(millis());
  }
  uint32_t ms = millis();
  State st = device.state();
  uint32_t period = (st == State::MOVING || st == State::VERIFYING) ? STATUS_MOVING_MS : STATUS_IDLE_MS;
  if (ms - lastStatus >= period) { lastStatus = ms; proto.sendStatus(); }
}
