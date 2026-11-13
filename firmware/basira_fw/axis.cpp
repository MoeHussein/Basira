#include "axis.h"
#include <math.h>

namespace basira {

static double wrap360diff(double d) {  // -> (-180, 180]
  d = fmod(d, 360.0);
  if (d <= -180.0) d += 360.0;
  if (d > 180.0) d -= 360.0;
  return d;
}

void Axis::begin(IMotor* m, ISensor* s, const AxisParams& p) {
  m_ = m; s_ = s; p_ = p;
  havePos_ = sensorFailed_ = active_ = false;
  errCount_ = 0;
  if (m_) m_->drive(0);
}

bool Axis::sample() {
  float r;
  if (!s_ || !s_->read(r)) {
    if (++errCount_ >= p_.max_sensor_errors) sensorFailed_ = true;
    return !sensorFailed_;
  }
  errCount_ = 0;
  accept(r);
  return !sensorFailed_;
}

void Axis::accept(float r) {
  if (!havePos_) {
    unwrapped_ = p_.zero_deg + wrap360diff(r - p_.zero_deg);  // start within half a turn of zero
    havePos_ = true;
  } else {
    unwrapped_ += wrap360diff(r - lastRaw_);
  }
  lastRaw_ = r;
}

bool Axis::probe() {
  float r;
  if (!s_ || !s_->read(r)) return false;
  errCount_ = 0;
  accept(r);
  return true;
}

double Axis::pos() const {
  double a = p_.sign * (unwrapped_ - p_.zero_deg);  // deg of shaft
  if (p_.ring) return a;                            // ring: 1:1 sense gear, deg of ring
  return a * M_PI / 180.0 * p_.pinion_r_mm;         // plate: rack travel (mm)
}

bool Axis::setTarget(double t, uint32_t now_ms) {
  if (t < p_.lim_lo || t > p_.lim_hi) return false;
  target_ = t;
  double cur = pos();
  stage_ = (t < cur) ? 0 : 1;                      // moving down: overshoot below, then approach upward
  aim_ = stage_ == 0 ? t - p_.backlash : t;
  if (aim_ < p_.lim_lo) aim_ = p_.lim_lo;
  active_ = true;
  settledSince_ = 0;
  winStart_ = now_ms;
  winErr_ = fabs(aim_ - cur);
  return true;
}

void Axis::stop() {
  active_ = false;
  if (m_) m_->drive(0);
}

Axis::Step Axis::update(uint32_t now) {
  if (!active_) { if (m_) m_->drive(0); return Step::IDLE; }
  double e = aim_ - pos();
  if (fabs(e) <= p_.tol) {
    if (stage_ == 0) {                              // reached the pre-approach point
      stage_ = 1; aim_ = target_;
      winStart_ = now; winErr_ = fabs(aim_ - pos());
      return Step::MOVING;
    }
    if (m_) m_->drive(0);
    if (!settledSince_) settledSince_ = now ? now : 1;
    if (now - settledSince_ >= p_.settle_ms) { active_ = false; return Step::SETTLED; }
    return Step::MOVING;
  }
  settledSince_ = 0;
  float d = (float)(p_.kp * e);
  if (d > p_.dmax) d = p_.dmax;
  if (d < -p_.dmax) d = -p_.dmax;
  if (fabs(d) < p_.dmin) d = (e > 0 ? p_.dmin : -p_.dmin);
  if (m_) m_->drive(p_.motor_dir * d);
  if (now - winStart_ >= p_.stall_window_ms) {      // stall: error not shrinking while driven
    double need = p_.stall_min_progress;             // near the target, ask for half the remaining distance
    if ((winErr_ - p_.tol) * 0.5 < need) need = (winErr_ - p_.tol) * 0.5;
    if (winErr_ - fabs(e) < need) { stop(); return Step::STALL; }
    winStart_ = now; winErr_ = fabs(e);
  }
  return Step::MOVING;
}

}  // namespace basira
