// BASIRA optics model (one eye). C++ port of physics/basira_optics.py — that file is the definition;
// test_host checks this port against tests/golden_vectors.json generated from it.
#pragma once
#include <stdint.h>

namespace basira {

struct Calib {
  double P0 = 0.0;        // sphere offset (D)
  double k = 2.0;         // Alvarez gain (D per mm of each plate's shift)
  double Pp = 3.0;        // plus cylinder power (D)
  double Pm = -3.0;       // minus cylinder power (D)
  double J0off = 0.0;     // fixed J0 offset (D)
  double J45off = 0.0;    // fixed J45 offset (D)
  double delta_max = 5.0; // plate travel limit (mm)
  char model_id[24] = "NOMINAL";
};

struct PV { double M, J0, J45; };      // Thibos power vector (D)
struct Rx { double S, C, ax; };        // minus cylinder, axis in (0, 180] deg, TABO
struct Pos { double delta, alpha, beta; };  // plate shift (mm), plus-cyl axis (deg), minus-cyl axis (deg)

enum class InvResult : uint8_t { OK, SPHERE_RANGE, CYL_RANGE };

double wrap180(double a_deg);              // -> (-90, 90]
PV rxToPv(const Rx& rx);
Rx pvToRx(const PV& pv);                   // axis 180 when cylinder is zero
PV forward(const Pos& p, const Calib& c);  // positions -> delivered power
// Target power -> positions. Of the two ring solutions the one with least ring travel from `cur` is chosen,
// and ring angles are the nearest equivalents (mod 180) to the current angles.
InvResult inverse(const PV& target, const Calib& c, const Pos& cur, Pos& out);

}  // namespace basira
