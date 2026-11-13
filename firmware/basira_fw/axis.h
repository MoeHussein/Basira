// One driven element: gearmotor + AS5048A angle sensor, closed-loop position control.
// Hardware-independent (motor and sensor are interfaces), so test_host runs it against a simulated drive.
#pragma once
#include <stdint.h>

namespace basira {

struct IMotor {                       // duty in [-1, 1]; 0 = brake
  virtual void drive(float duty) = 0;
  virtual ~IMotor() {}
};
struct ISensor {                      // raw shaft angle in [0, 360) deg; false = read error (parity, error flag, magnet)
  virtual bool read(float& deg) = 0;
  virtual ~ISensor() {}
};

struct AxisParams {
  bool ring = true;           // ring: position in deg (cylinder axis); plate: position in mm (Alvarez shift)
  float zero_deg = 0;         // raw sensor angle at physical zero (bring-up / calibration)
  int8_t sign = 1;            // +1 / -1: raw angle direction vs physical direction
  int8_t motor_dir = 1;       // +1 / -1: positive duty moves the physical position up
  float pinion_r_mm = 5.0f;   // plate only: pinion pitch radius
  float tol = 0.3f;           // settle tolerance (deg or mm)
  float kp = 0.04f;           // duty per unit error
  float dmin = 0.30f;         // minimum duty that moves the drive (stiction), measured at bring-up
  float dmax = 1.0f;
  float backlash = 2.0f;      // final approach always from below by this margin (removes gear backlash)
  uint16_t settle_ms = 60;
  uint16_t stall_window_ms = 300;
  float stall_min_progress = 0.2f;  // error must shrink at least this much per window
  uint8_t max_sensor_errors = 5;    // consecutive bad reads before the sensor is declared failed
  float lim_lo = -1e9f, lim_hi = 1e9f;  // soft limits (plate travel)
};

class Axis {
 public:
  enum class Step : uint8_t { IDLE, MOVING, SETTLED, STALL };

  void begin(IMotor* m, ISensor* s, const AxisParams& p);
  void setParams(const AxisParams& p) { p_ = p; }
  const AxisParams& params() const { return p_; }

  bool sample();                    // read sensor; false when the sensor has failed (latched)
  bool probe();                     // one fresh read; true only if this read succeeded (used before clearing a fault)
  bool sensorOk() const { return !sensorFailed_; }
  bool hasPosition() const { return havePos_; }
  double pos() const;               // physical units
  float raw() const { return lastRaw_; }

  bool setTarget(double t, uint32_t now_ms);   // false if outside soft limits
  double target() const { return target_; }
  void stop();                      // motor brake, controller idle
  Step update(uint32_t now_ms);     // run controller once; call after sample()
  void clearFault() { sensorFailed_ = false; errCount_ = 0; }

 private:
  IMotor* m_ = nullptr;
  ISensor* s_ = nullptr;
  AxisParams p_;
  bool havePos_ = false, sensorFailed_ = false, active_ = false;
  uint8_t errCount_ = 0;
  float lastRaw_ = 0;
  double unwrapped_ = 0;            // raw angle, unwrapped (deg)
  double target_ = 0, aim_ = 0;
  uint8_t stage_ = 0;               // 0: pre-approach point below target, 1: target
  void accept(float r);
  uint32_t settledSince_ = 0, winStart_ = 0;
  double winErr_ = 0;
};

}  // namespace basira
