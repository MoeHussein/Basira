// BASIRA firmware configuration. PLACEHOLDER VALUES: confirm every pin against your wiring (docs/WIRING.md)
// and measure the drive parameters at bring-up (docs/CALIBRATION.md, step 1).
#pragma once
#include <stdint.h>

#define BASIRA_FW_VERSION "0.1.0"
#define BASIRA_PROTO_VERSION 1

// Bench bring-up: allow CONFIRMED with the nominal (unfitted) optics model; every "confirmed" message then
// carries "calibrated": false and the app shows NOT CALIBRATED. Set to 0 before any test on a person.
#define BASIRA_ALLOW_NOMINAL_CALIB 1

// ---------------- pins: ESP32-S3-DevKitC-1 (N8R8: GPIO 35-37 are used by PSRAM) ----------------
// Each DRV8833 channel: IN1 = PWM (speed), IN2 = direction.  Forward: IN2=0, IN1=duty.  Reverse: IN2=1, IN1=1-duty.
// Order of axes: 0 = R plate, 1 = R ring(+3.00), 2 = R ring(-3.00), 3 = L plate, 4 = L ring(+3.00), 5 = L ring(-3.00)
static const uint8_t PIN_MOTOR_PWM[6] = {4, 6, 15, 17, 8, 1};
static const uint8_t PIN_MOTOR_DIR[6] = {5, 7, 16, 18, 9, 2};
static const uint8_t PIN_MOTOR_SLEEP = 14;   // all DRV8833 nSLEEP pins; HIGH = enabled
static const uint8_t PIN_MOTOR_FAULT = 21;   // all DRV8833 nFAULT pins (open drain, wired together); LOW = fault

// AS5048A sensors on one SPI bus (mode 1), one chip select each, same axis order as the motors
static const uint8_t PIN_SPI_SCK = 12, PIN_SPI_MISO = 13, PIN_SPI_MOSI = 11;
static const uint8_t PIN_SENSOR_CS[6] = {10, 38, 39, 40, 41, 42};
static const uint32_t SPI_HZ = 4000000;

// Occluder servos (R, L). GPIO 48 also drives the DevKitC v1.0 on-board RGB LED: harmless flicker.
static const uint8_t PIN_SERVO[2] = {47, 48};
static const uint16_t SERVO_US_OPEN = 1000, SERVO_US_CLOSED = 2000;

// ---------------- timing ----------------
static const uint16_t CONTROL_PERIOD_US = 2000;   // 500 Hz control loop
static const uint32_t MOVE_TIMEOUT_MS = 4000;     // whole move, all three drives of one eye
static const uint32_t VERIFY_MS = 150;            // motors braked, sensors averaged, then checked
static const uint32_t LINK_TIMEOUT_MS = 1500;     // no message from the app during motion -> FAULT LINK
static const uint32_t STATUS_MOVING_MS = 50, STATUS_IDLE_MS = 500;
static const uint32_t MOTOR_PWM_HZ = 20000;

// ---------------- verification tolerances (sensed vs target) ----------------
// Requirements (blueprint rev 9, section 4): 0.12 D -> ring differential <= 1.15 deg, plate <= 60 um at k = 2.00 D/mm
static const float VERIFY_TOL_RING_DEG = 0.6f, VERIFY_TOL_PLATE_MM = 0.030f;
static const float DRIFT_TOL_RING_DEG = 1.0f, DRIFT_TOL_PLATE_MM = 0.050f;   // while CONFIRMED

// ---------------- Wi-Fi access point (the app is served from the device) ----------------
static const char* WIFI_SSID = "BASIRA-01";
static const char* WIFI_PASS = "basira-setup";   // change before use; WPA2 needs >= 8 characters
static const uint32_t SERIAL_BAUD = 460800;     // status stream ~6 kB/s while moving: 115200 would block the loop
