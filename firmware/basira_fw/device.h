// BASIRA device: 2 eyes x (Alvarez plate, plus-cylinder ring, minus-cylinder ring) + 2 occluders.
// Owns the state machine and the optics/calibration model. Hardware-independent (test_host runs it).
#pragma once
#include <stdint.h>
#include "axis.h"
#include "optics.h"

namespace basira {

enum class State : uint8_t { PARKED, READY, MOVING, VERIFYING, CONFIRMED, FAULT };
enum class Fault : uint8_t { NONE, SENSOR, STALL, TIMEOUT, DRIVER, LINK, USER_STOP, TOLERANCE, DRIFT };
const char* stateName(State s);
const char* faultName(Fault f);

struct IBoard {
  virtual void motorsEnable(bool on) = 0;
  virtual bool driverFault() = 0;            // DRV8833 nFAULT asserted
  virtual void servo(int eye, bool closed) = 0;
  virtual ~IBoard() {}
};

// Everything that is fitted or measured for one eye; stored in flash (NVS) by the sketch.
struct EyeConfig {
  uint32_t magic = 0xBA51A001;
  Calib cal;
  float zero_deg[3] = {0, 0, 0};    // plate, ringP, ringM: raw sensor angle at physical zero
  int8_t sign[3] = {1, 1, 1};
  int8_t motor_dir[3] = {1, 1, 1};
  float pinion_r_mm = 5.0f;
  float dmin[3] = {0.30f, 0.30f, 0.30f};   // measured at bring-up
  bool fitted = false;              // true only after a calibration fit passed held-out validation
};

struct IListener {
  virtual void onAck(int id) = 0;
  virtual void onError(int id, const char* code, const char* msg) = 0;
  virtual void onState(State s, int eye) = 0;
  virtual void onConfirmed(int id, int eye, const PV& pv, const Rx& rx, const Pos& pos, bool fitted, const char* model) = 0;
  virtual void onFault(Fault f, const char* axis, const char* msg) = 0;
  virtual ~IListener() {}
};

class Device {
 public:
  static const int R = 0, L = 1;
  static const char* axisName(int i);   // "R.plate", "R.ringP", ...

  void begin(IMotor* motors[6], ISensor* sensors[6], IBoard* board, IListener* listener,
             const EyeConfig& cfgR, const EyeConfig& cfgL);
  void tick(uint32_t now_ms);           // call every CONTROL_PERIOD_US

  // commands (results through the listener)
  void cmdSetRx(int id, int eye, const Rx& rx);
  void cmdSetPos(int id, int eye, const Pos& p);   // calibration capture: positions directly
  void cmdOcclude(int id, int eye, bool closed);
  void cmdStop(int id);
  void cmdReset(int id);
  void cmdJog(int id, int axis, float duty, uint16_t ms);   // bring-up only
  void cmdSetConfig(int id, int eye, const EyeConfig& c);
  void linkActivity() { linkSeen_ = true; lastLink_ = now_; }
  void linkClosed() { if (state_ == State::MOVING || state_ == State::VERIFYING) fault(Fault::LINK, -1, "app disconnected during motion"); }

  // read-only state
  State state() const { return state_; }
  int activeEye() const { return eye_; }
  Fault fault() const { return fault_; }
  Pos pos(int eye) const;
  Pos target(int eye) const { return target_[eye]; }
  float raw(int axis) const { return ax_[axis].raw(); }
  bool sensorOk(int axis) const { return ax_[axis].sensorOk(); }
  bool occluded(int eye) const { return occl_[eye]; }
  bool confirmed(int eye) const { return confirmed_[eye]; }
  const EyeConfig& config(int eye) const { return cfg_[eye]; }

 private:
  void applyConfig(int eye);
  void fault(Fault f, int axis, const char* msg);
  void setState(State s);
  bool startMove(int id, int eye, const Pos& p);

  Axis ax_[6];
  IBoard* board_ = nullptr;
  IListener* L_ = nullptr;
  EyeConfig cfg_[2];
  State state_ = State::PARKED;
  Fault fault_ = Fault::NONE;
  int eye_ = 0, moveId_ = 0;
  Pos target_[2] = {{0, 45, 45}, {0, 45, 45}};
  bool confirmed_[2] = {false, false}, occl_[2] = {false, false}, settled_[3] = {false, false, false};
  uint32_t now_ = 0, moveStart_ = 0, verifyStart_ = 0, lastLink_ = 0, jogUntil_ = 0;
  int jogAxis_ = -1;
  float jogDuty_ = 0;
  bool linkSeen_ = false;
  double sum_[3] = {0, 0, 0};
  uint32_t nsum_ = 0;
  IMotor* motors_[6] = {};
  ISensor* sensors_[6] = {};
};

}  // namespace basira
