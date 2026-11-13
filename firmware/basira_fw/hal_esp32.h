// ESP32-S3 hardware: DRV8833 motor channels, AS5048A angle sensors (SPI), occluder servos.
// Arduino-ESP32 core 3.x (LEDC API: ledcAttach / ledcWrite by pin).
#pragma once
#ifdef ARDUINO
#include <Arduino.h>
#include <SPI.h>
#include "config.h"
#include "device.h"

namespace basira {

// One DRV8833 H-bridge. IN1 = PWM, IN2 = direction:
//   duty > 0: IN2 = 0, IN1 = duty      (forward / coast)
//   duty < 0: IN2 = 1, IN1 = 1 + duty  (reverse / brake)
//   duty = 0: IN1 = IN2 = 1            (brake: holds position)
class Drv8833Motor : public IMotor {
 public:
  void begin(uint8_t pinPwm, uint8_t pinDir) {
    pwm_ = pinPwm; dir_ = pinDir;
    pinMode(dir_, OUTPUT);
    ledcAttach(pwm_, MOTOR_PWM_HZ, 10);
    drive(0);
  }
  void drive(float d) override {
    const uint32_t FULL = 1023;
    if (d > 1) d = 1;
    if (d < -1) d = -1;
    if (d > 0) { digitalWrite(dir_, LOW); ledcWrite(pwm_, (uint32_t)(d * FULL)); }
    else if (d < 0) { digitalWrite(dir_, HIGH); ledcWrite(pwm_, (uint32_t)((1 + d) * FULL)); }
    else { digitalWrite(dir_, HIGH); ledcWrite(pwm_, FULL); }
  }
 private:
  uint8_t pwm_ = 0, dir_ = 0;
};

// AS5048A, SPI mode 1, 16-bit frames: bit 15 even parity, bit 14 read (command) / error flag (response).
// The response to a command arrives in the next frame.
class As5048a : public ISensor {
 public:
  void begin(SPIClass* spi, uint8_t pinCs) {
    spi_ = spi; cs_ = pinCs;
    pinMode(cs_, OUTPUT);
    digitalWrite(cs_, HIGH);
  }
  bool read(float& deg) override {
    xfer(CMD_ANGLE);
    uint16_t r = xfer(CMD_NOP);
    if (!parityOk(r)) return false;
    if (r & 0x4000) { xfer(CMD_CLEAR_ERR); xfer(CMD_NOP); return false; }   // error flag: read clears it
    if (++n_ % 50 == 0) {                                                    // magnet field check every 50 reads
      xfer(CMD_DIAG);
      uint16_t dgn = xfer(CMD_NOP);
      if (!parityOk(dgn) || (dgn & 0x4000)) return false;
      bool ocf = dgn & 0x0100, compLow = dgn & 0x0400, compHigh = dgn & 0x0800;
      if (!ocf || compLow || compHigh) return false;  // offset loop not finished, magnet too strong or too weak
    }
    deg = (r & 0x3FFF) * (360.0f / 16384.0f);
    return true;
  }
 private:
  static const uint16_t CMD_NOP = 0x0000, CMD_ANGLE = 0xFFFF, CMD_DIAG = 0x7FFD, CMD_CLEAR_ERR = 0x4001;
  static bool parityOk(uint16_t v) { return __builtin_parity(v) == 0; }
  uint16_t xfer(uint16_t cmd) {
    spi_->beginTransaction(SPISettings(SPI_HZ, MSBFIRST, SPI_MODE1));
    digitalWrite(cs_, LOW);
    uint16_t r = spi_->transfer16(cmd);
    digitalWrite(cs_, HIGH);
    spi_->endTransaction();
    return r;
  }
  SPIClass* spi_ = nullptr;
  uint8_t cs_ = 0;
  uint32_t n_ = 0;
};

class Esp32Board : public IBoard {
 public:
  void begin() {
    pinMode(PIN_MOTOR_SLEEP, OUTPUT);
    digitalWrite(PIN_MOTOR_SLEEP, LOW);
    pinMode(PIN_MOTOR_FAULT, INPUT_PULLUP);
    for (int e = 0; e < 2; e++) ledcAttach(PIN_SERVO[e], 50, 14);
  }
  void motorsEnable(bool on) override { digitalWrite(PIN_MOTOR_SLEEP, on ? HIGH : LOW); }
  bool driverFault() override { return digitalRead(PIN_MOTOR_FAULT) == LOW; }
  void servo(int eye, bool closed) override {
    uint32_t us = closed ? SERVO_US_CLOSED : SERVO_US_OPEN;
    ledcWrite(PIN_SERVO[eye], (uint32_t)(us * 16383UL / 20000UL));
  }
};

}  // namespace basira
#endif
