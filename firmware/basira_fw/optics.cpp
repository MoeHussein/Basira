#include "optics.h"
#include <math.h>

namespace basira {

static const double DEG = M_PI / 180.0;

double wrap180(double a) {
  a = fmod(a, 180.0);
  if (a <= -90.0) a += 180.0;
  if (a > 90.0) a -= 180.0;
  return a;
}

PV rxToPv(const Rx& r) {
  return {r.S + r.C / 2.0, -(r.C / 2.0) * cos(2.0 * r.ax * DEG), -(r.C / 2.0) * sin(2.0 * r.ax * DEG)};
}

Rx pvToRx(const PV& p) {
  double Jm = hypot(p.J0, p.J45);
  double C = -2.0 * Jm, S = p.M - C / 2.0;
  if (Jm < 1e-9) return {S, 0.0, 180.0};
  double ax = fmod(atan2(p.J45, p.J0) / 2.0 / DEG + 360.0, 180.0);
  if (ax < 1e-9) ax = 180.0;
  return {S, C, ax};
}

PV forward(const Pos& q, const Calib& c) {
  return {c.P0 + c.k * q.delta + (c.Pp + c.Pm) / 2.0,
          -(c.Pp / 2.0) * cos(2.0 * q.alpha * DEG) - (c.Pm / 2.0) * cos(2.0 * q.beta * DEG) + c.J0off,
          -(c.Pp / 2.0) * sin(2.0 * q.alpha * DEG) - (c.Pm / 2.0) * sin(2.0 * q.beta * DEG) + c.J45off};
}

InvResult inverse(const PV& t, const Calib& c, const Pos& cur, Pos& out) {
  double delta = (t.M - c.P0 - (c.Pp + c.Pm) / 2.0) / c.k;
  if (fabs(delta) > c.delta_max + 1e-9) return InvResult::SPHERE_RANGE;
  double tx = t.J0 - c.J0off, ty = t.J45 - c.J45off, T = hypot(tx, ty);
  double a_ = c.Pp / 2.0, b_ = -c.Pm / 2.0;
  if (T > a_ + b_ + 1e-9) return InvResult::CYL_RANGE;

  double du[2], dv[2];
  int n = 0;
  if (T < fabs(a_ - b_) + 1e-12 || T < 1e-12) {
    // residual cylinder (mismatched pair) or zero cylinder: rings anti-parallel
    double phi = (T > 1e-12) ? atan2(ty, tx) : 2.0 * cur.alpha * DEG + M_PI;
    double d = (T > 1e-12 && a_ < b_) ? phi + M_PI : phi;
    if (T < 1e-12) d = phi;
    du[0] = d; dv[0] = d + M_PI; n = 1;
  } else {
    double phi = atan2(ty, tx);
    double cg = (a_ * a_ + T * T - b_ * b_) / (2.0 * a_ * T);
    if (cg > 1) cg = 1;
    if (cg < -1) cg = -1;
    double g = acos(cg);
    for (int s = 0; s < 2; s++) {
      double d = phi + (s == 0 ? g : -g);
      du[n] = d;
      dv[n] = atan2(ty - a_ * sin(d), tx - a_ * cos(d));
      n++;
    }
  }
  double bestTravel = 1e9;
  for (int i = 0; i < n; i++) {
    double a = (du[i] - M_PI) / 2.0 / DEG, b = dv[i] / 2.0 / DEG;
    a = cur.alpha + wrap180(a - cur.alpha);
    b = cur.beta + wrap180(b - cur.beta);
    double travel = fabs(a - cur.alpha) + fabs(b - cur.beta);
    if (travel < bestTravel - 1e-9) {
      bestTravel = travel;
      out = {delta, a, b};
    }
  }
  return InvResult::OK;
}

}  // namespace basira
