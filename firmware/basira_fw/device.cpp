#include "device.h"
#include "config.h"
#include <math.h>
#include <string.h>

namespace basira {

const char* stateName(State s) {
  switch (s) {
    case State::PARKED: return "PARKED";
    case State::READY: return "READY";
    case State::MOVING: return "MOVING";
    case State::VERIFYING: return "VERIFYING";
    case State::CONFIRMED: return "CONFIRMED";
    default: return "FAULT";
  }
}
const char* faultName(Fault f) {
  switch (f) {
    case Fault::NONE: return "NONE";
    case Fault::SENSOR: return "SENSOR";
    case Fault::STALL: return "STALL";
    case Fault::TIMEOUT: return "TIMEOUT";
    case Fault::DRIVER: return "DRIVER";
    case Fault::LINK: return "LINK";
    case Fault::USER_STOP: return "USER_STOP";
    case Fault::TOLERANCE: return "TOLERANCE";
    default: return "DRIFT";
  }
}
const char* Device::axisName(int i) {
  static const char* n[6] = {"R.plate", "R.ringP", "R.ringM", "L.plate", "L.ringP", "L.ringM"};
  return (i >= 0 && i < 6) ? n[i] : "-";
}

static AxisParams defaults(bool ring) {
  AxisParams p;
  p.ring = ring;
  if (ring) { p.tol = 0.3f; p.kp = 0.04f; p.backlash = 2.0f; p.stall_min_progress = 0.2f; }
  else { p.tol = 0.010f; p.kp = 1.5f; p.backlash = 0.10f; p.stall_min_progress = 0.005f; }
  return p;
}

void Device::applyConfig(int eye) {
  const EyeConfig& c = cfg_[eye];
  for (int j = 0; j < 3; j++) {
    AxisParams p = defaults(j != 0);
    p.zero_deg = c.zero_deg[j];
    p.sign = c.sign[j];
    p.motor_dir = c.motor_dir[j];
    p.dmin = c.dmin[j];
    p.pinion_r_mm = c.pinion_r_mm;
    if (j == 0) { p.lim_lo = -(float)c.cal.delta_max - 0.3f; p.lim_hi = (float)c.cal.delta_max + 0.3f; }
    ax_[eye * 3 + j].setParams(p);
  }
}

void Device::begin(IMotor* motors[6], ISensor* sensors[6], IBoard* board, IListener* listener,
                   const EyeConfig& cfgR, const EyeConfig& cfgL) {
  board_ = board; L_ = listener;
  cfg_[R] = cfgR; cfg_[L] = cfgL;
  for (int i = 0; i < 6; i++) {
    motors_[i] = motors[i]; sensors_[i] = sensors[i];
    ax_[i].begin(motors[i], sensors[i], defaults(i % 3 != 0));
  }
  applyConfig(R); applyConfig(L);
  state_ = State::PARKED; fault_ = Fault::NONE;
  if (board_) { board_->motorsEnable(true); board_->servo(R, false); board_->servo(L, false); }
}

Pos Device::pos(int eye) const {
  return {ax_[eye * 3].pos(), ax_[eye * 3 + 1].pos(), ax_[eye * 3 + 2].pos()};
}

void Device::setState(State s) {
  if (s == state_) return;
  state_ = s;
  if (L_) L_->onState(s, eye_);
}

void Device::fault(Fault f, int axis, const char* msg) {
  for (int i = 0; i < 6; i++) ax_[i].stop();
  jogAxis_ = -1;
  confirmed_[R] = confirmed_[L] = false;   // correction unknown
  fault_ = f;
  state_ = State::FAULT;
  if (L_) { L_->onFault(f, axisName(axis), msg); L_->onState(State::FAULT, eye_); }
}

void Device::tick(uint32_t now) {
  now_ = now;
  for (int i = 0; i < 6; i++) ax_[i].sample();
  if (state_ != State::FAULT) {
    for (int i = 0; i < 6; i++)
      if (!ax_[i].sensorOk()) { fault(Fault::SENSOR, i, "angle sensor not reading (parity, error flag or magnet)"); return; }
    if (board_ && board_->driverFault()) { fault(Fault::DRIVER, -1, "motor driver reports a fault (over-current or over-temperature)"); return; }
  }
  // bring-up jog runs open loop, outside the state machine
  if (jogAxis_ >= 0) {
    if ((int32_t)(now - jogUntil_) >= 0) { motors_[jogAxis_]->drive(0); jogAxis_ = -1; }
    else motors_[jogAxis_]->drive(jogDuty_);
    return;
  }
  switch (state_) {
    case State::PARKED: {
      bool all = true;
      for (int i = 0; i < 6; i++) all = all && ax_[i].hasPosition();
      if (all) setState(State::READY);
      break;
    }
    case State::MOVING: {
      if (linkSeen_ && now - lastLink_ > LINK_TIMEOUT_MS) { fault(Fault::LINK, -1, "no message from the app during motion"); return; }
      bool all = true;
      for (int j = 0; j < 3; j++) {
        int i = eye_ * 3 + j;
        if (!settled_[j]) {
          Axis::Step st = ax_[i].update(now);
          if (st == Axis::Step::STALL) { fault(Fault::STALL, i, "drive is not moving toward its target"); return; }
          if (st == Axis::Step::SETTLED || st == Axis::Step::IDLE) settled_[j] = true;
        }
        all = all && settled_[j];
      }
      if (all) { verifyStart_ = now; sum_[0] = sum_[1] = sum_[2] = 0; nsum_ = 0; setState(State::VERIFYING); }
      else if (now - moveStart_ > MOVE_TIMEOUT_MS) fault(Fault::TIMEOUT, -1, "move did not finish in time");
      break;
    }
    case State::VERIFYING: {
      Pos p = pos(eye_);
      sum_[0] += p.delta; sum_[1] += p.alpha; sum_[2] += p.beta; nsum_++;
      if (now - verifyStart_ >= VERIFY_MS) {
        Pos a = {sum_[0] / nsum_, sum_[1] / nsum_, sum_[2] / nsum_};
        const Pos& t = target_[eye_];
        if (fabs(a.delta - t.delta) > VERIFY_TOL_PLATE_MM) { fault(Fault::TOLERANCE, eye_ * 3, "plate outside tolerance after settling"); return; }
        if (fabs(a.alpha - t.alpha) > VERIFY_TOL_RING_DEG) { fault(Fault::TOLERANCE, eye_ * 3 + 1, "ring outside tolerance after settling"); return; }
        if (fabs(a.beta - t.beta) > VERIFY_TOL_RING_DEG) { fault(Fault::TOLERANCE, eye_ * 3 + 2, "ring outside tolerance after settling"); return; }
        PV pv = forward(a, cfg_[eye_].cal);       // value reported from the sensed positions, not the command
        confirmed_[eye_] = true;
        setState(State::CONFIRMED);
        if (L_) L_->onConfirmed(moveId_, eye_, pv, pvToRx(pv), a, cfg_[eye_].fitted, cfg_[eye_].cal.model_id);
      }
      break;
    }
    case State::CONFIRMED: {
      for (int e = 0; e < 2; e++) {
        if (!confirmed_[e]) continue;
        Pos p = pos(e);
        const Pos& t = target_[e];
        if (fabs(p.delta - t.delta) > DRIFT_TOL_PLATE_MM) { fault(Fault::DRIFT, e * 3, "plate moved after confirmation"); return; }
        if (fabs(p.alpha - t.alpha) > DRIFT_TOL_RING_DEG) { fault(Fault::DRIFT, e * 3 + 1, "ring moved after confirmation"); return; }
        if (fabs(p.beta - t.beta) > DRIFT_TOL_RING_DEG) { fault(Fault::DRIFT, e * 3 + 2, "ring moved after confirmation"); return; }
      }
      break;
    }
    default: break;
  }
}

bool Device::startMove(int id, int eye, const Pos& p) {
  for (int j = 0; j < 3; j++) settled_[j] = false;
  double tv[3] = {p.delta, p.alpha, p.beta};
  for (int j = 0; j < 3; j++) {
    if (!ax_[eye * 3 + j].setTarget(tv[j], now_)) {
      for (int k = 0; k < 3; k++) ax_[eye * 3 + k].stop();
      if (L_) L_->onError(id, "RANGE", "target outside the drive's travel");
      return false;
    }
  }
  target_[eye] = p;
  confirmed_[eye] = false;
  eye_ = eye; moveId_ = id; moveStart_ = now_;
  if (L_) L_->onAck(id);
  setState(State::MOVING);
  return true;
}

static bool busyOrNot(State s, IListener* L, int id) {
  if (s == State::FAULT) { if (L) L->onError(id, "FAULT", "device is in FAULT; send reset first"); return true; }
  if (s == State::MOVING || s == State::VERIFYING) { if (L) L->onError(id, "BUSY", "a move is in progress"); return true; }
  if (s == State::PARKED) { if (L) L->onError(id, "NOT_READY", "sensors not read yet"); return true; }
  return false;
}

void Device::cmdSetRx(int id, int eye, const Rx& rx) {
  if (busyOrNot(state_, L_, id)) return;
  if (!cfg_[eye].fitted && !BASIRA_ALLOW_NOMINAL_CALIB) { if (L_) L_->onError(id, "UNCALIBRATED", "no valid calibration for this eye"); return; }
  if (!(rx.C <= 0.0) || isnan(rx.S) || isnan(rx.ax)) { if (L_) L_->onError(id, "BAD_RX", "expect minus cylinder: C <= 0"); return; }
  Pos out;
  InvResult r = inverse(rxToPv(rx), cfg_[eye].cal, pos(eye), out);
  if (r == InvResult::SPHERE_RANGE) { if (L_) L_->onError(id, "RANGE_SPHERE", "spherical equivalent outside the Alvarez range"); return; }
  if (r == InvResult::CYL_RANGE) { if (L_) L_->onError(id, "RANGE_CYL", "cylinder larger than the ring pair can make"); return; }
  startMove(id, eye, out);
}

void Device::cmdSetPos(int id, int eye, const Pos& p) {
  if (busyOrNot(state_, L_, id)) return;
  Pos cur = pos(eye), q = p;
  q.alpha = cur.alpha + wrap180(q.alpha - cur.alpha);   // nearest equivalent ring angle
  q.beta = cur.beta + wrap180(q.beta - cur.beta);
  startMove(id, eye, q);
}

void Device::cmdOcclude(int id, int eye, bool closed) {
  occl_[eye] = closed;
  if (board_) board_->servo(eye, closed);
  if (L_) L_->onAck(id);
}

void Device::cmdStop(int id) {
  if (L_) L_->onAck(id);
  fault(Fault::USER_STOP, -1, "stopped by the operator");
}

void Device::cmdReset(int id) {
  for (int i = 0; i < 6; i++) {
    if (!ax_[i].probe()) {                 // demand a fresh good read before re-arming
      if (L_) L_->onError(id, "SENSOR", axisName(i));
      return;
    }
  }
  for (int i = 0; i < 6; i++) {
    ax_[i].clearFault();
    if (!ax_[i].hasPosition()) {
      if (L_) L_->onError(id, "SENSOR", axisName(i));
      return;
    }
  }
  if (board_ && board_->driverFault()) { if (L_) L_->onError(id, "DRIVER", "motor driver still reports a fault"); return; }
  for (int i = 0; i < 6; i++) ax_[i].stop();
  fault_ = Fault::NONE;
  confirmed_[R] = confirmed_[L] = false;
  if (L_) L_->onAck(id);
  state_ = State::FAULT;   // force the transition event
  setState(State::READY);
}

void Device::cmdJog(int id, int axis, float duty, uint16_t ms) {
  if (axis < 0 || axis > 5 || fabs(duty) > 1.0f || ms > 2000) { if (L_) L_->onError(id, "BAD_ARG", "axis 0..5, |duty| <= 1, ms <= 2000"); return; }
  if (state_ == State::MOVING || state_ == State::VERIFYING) { if (L_) L_->onError(id, "BUSY", "a move is in progress"); return; }
  confirmed_[axis / 3] = false;
  if (state_ == State::CONFIRMED) setState(State::READY);
  jogAxis_ = axis; jogDuty_ = duty; jogUntil_ = now_ + ms;
  if (L_) L_->onAck(id);
}

void Device::cmdSetConfig(int id, int eye, const EyeConfig& c) {
  if (state_ == State::MOVING || state_ == State::VERIFYING) { if (L_) L_->onError(id, "BUSY", "a move is in progress"); return; }
  if (!(c.cal.k > 0.1) || !(c.cal.Pp > 0) || !(c.cal.Pm < 0) || !(c.pinion_r_mm > 0)) { if (L_) L_->onError(id, "BAD_ARG", "k > 0.1, Pp > 0, Pm < 0, pinion_r_mm > 0"); return; }
  cfg_[eye] = c;
  applyConfig(eye);
  for (int j = 0; j < 3; j++) { ax_[eye * 3 + j].stop(); ax_[eye * 3 + j].begin(motors_[eye * 3 + j], sensors_[eye * 3 + j], ax_[eye * 3 + j].params()); }  // re-reference to the new zero
  confirmed_[eye] = false;
  if (L_) L_->onAck(id);
}

}  // namespace basira
