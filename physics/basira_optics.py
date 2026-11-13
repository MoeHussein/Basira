"""BASIRA reference optics model (one eye). This file is the definition; firmware (C++) and app (JS)
are checked against golden vectors generated from it (tools/make_golden.py).

Conventions
-----------
* Prescriptions are minus cylinder: S (D), C <= 0 (D), axis theta in (0, 180] deg, TABO (counter-clockwise
  as the examiner faces the patient).
* Power vectors (Thibos 1997): M = S + C/2, J0 = -(C/2) cos 2theta, J45 = -(C/2) sin 2theta.
* Element positions: delta = shift of each Alvarez plate (mm, plates move in opposite directions),
  alpha = axis of the plus cylinder (deg), beta = axis of the minus cylinder (deg).

Calibrated model (blueprint rev 9, section 8), thin elements, per eye:
    M   = P0 + k*delta + (Pp + Pm)/2
    J   = -(Pp/2) e(2 alpha) - (Pm/2) e(2 beta) + (J0off, J45off),   e(phi) = (cos phi, sin phi)
with Pp > 0 (plus cylinder), Pm < 0 (minus cylinder). Nominal: P0 = 0, k = 2.00 D/mm, Pp = +3, Pm = -3, J offsets 0.
For Pp = -Pm = P this reduces to the closed form: (alpha + beta)/2 = theta + 45 deg, alpha - beta = asin(|C| / 2P).
"""
from __future__ import annotations
import math
from dataclasses import dataclass, asdict

DEG = math.pi / 180


@dataclass
class Calib:
    P0: float = 0.0          # sphere offset (D)
    k: float = 2.0           # Alvarez gain (D per mm of each plate's shift)
    Pp: float = 3.0          # plus cylinder power (D)
    Pm: float = -3.0         # minus cylinder power (D)
    J0off: float = 0.0       # fixed J0 offset, e.g. constant Alvarez cross-slide misalignment (D)
    J45off: float = 0.0      # fixed J45 offset (D)
    delta_max: float = 5.0   # plate travel limit (mm)
    model_id: str = "NOMINAL"

    def to_dict(self):
        return asdict(self)


def wrap180(a_deg: float) -> float:
    """Map an angle difference to (-90, 90]: cylinders repeat every 180 deg."""
    a = math.fmod(a_deg, 180.0)
    if a <= -90: a += 180
    if a > 90: a -= 180
    return a


def rx_to_pv(S: float, C: float, ax: float):
    return (S + C / 2, -(C / 2) * math.cos(2 * ax * DEG), -(C / 2) * math.sin(2 * ax * DEG))


def pv_to_rx(M: float, J0: float, J45: float):
    """Minus-cylinder Rx from a power vector. Axis is 180 when the cylinder is (numerically) zero."""
    Jm = math.hypot(J0, J45)
    C = -2 * Jm
    S = M - C / 2
    if Jm < 1e-9:
        return S, 0.0, 180.0
    ax = (math.atan2(J45, J0) / 2) / DEG     # minus cylinder: J = (|C|/2)(cos 2ax, sin 2ax)
    ax = ax % 180
    if ax < 1e-9: ax = 180.0
    return S, C, ax


def forward(pos, cal: Calib):
    """Element positions -> power vector delivered by the device."""
    d, a, b = pos
    M = cal.P0 + cal.k * d + (cal.Pp + cal.Pm) / 2
    J0 = -(cal.Pp / 2) * math.cos(2 * a * DEG) - (cal.Pm / 2) * math.cos(2 * b * DEG) + cal.J0off
    J45 = -(cal.Pp / 2) * math.sin(2 * a * DEG) - (cal.Pm / 2) * math.sin(2 * b * DEG) + cal.J45off
    return M, J0, J45


class OutOfRange(ValueError):
    pass


def inverse(target_pv, cal: Calib, current=(0.0, 45.0, 45.0)):
    """Power vector target -> positions (delta, alpha, beta). Of the two ring solutions, the one needing the
    least ring travel from `current` is returned; ring angles are returned as the nearest equivalent (mod 180)
    to the current angles. Raises OutOfRange when the target cannot be produced."""
    M, J0, J45 = target_pv
    delta = (M - cal.P0 - (cal.Pp + cal.Pm) / 2) / cal.k
    if abs(delta) > cal.delta_max + 1e-9:
        raise OutOfRange(f"sphere: plate shift {delta:+.3f} mm exceeds +/-{cal.delta_max} mm")
    # ring vectors: u = -(Pp/2) e(2a) (length a_=Pp/2, direction 2a+pi); v = -(Pm/2) e(2b) (length b_=-Pm/2, direction 2b)
    tx, ty = J0 - cal.J0off, J45 - cal.J45off
    T = math.hypot(tx, ty)
    a_, b_ = cal.Pp / 2, -cal.Pm / 2
    if T > a_ + b_ + 1e-9:
        raise OutOfRange(f"cylinder: |C| = {2 * T:.2f} D exceeds {2 * (a_ + b_):.2f} D")
    _, a_cur, b_cur = current
    if T < abs(a_ - b_) + 1e-12 or T < 1e-12:
        # zero cylinder, or residual cylinder with a mismatched pair (Pp != -Pm): rings anti-parallel.
        # At zero cylinder the plus ring stays where it is; otherwise the larger ring points along the target.
        if T < 1e-12:
            dir_u = 2 * a_cur * DEG + math.pi
        else:
            phi = math.atan2(ty, tx)
            dir_u = phi if a_ >= b_ else phi + math.pi
        candidates = [(dir_u, dir_u + math.pi)]
    else:
        phi = math.atan2(ty, tx)
        cg = (a_ * a_ + T * T - b_ * b_) / (2 * a_ * T)
        g = math.acos(max(-1.0, min(1.0, cg)))
        candidates = []
        for s in (+1, -1):
            du = phi + s * g
            ux, uy = a_ * math.cos(du), a_ * math.sin(du)
            dv = math.atan2(ty - uy, tx - ux)
            candidates.append((du, dv))
    best = None
    for du, dv in candidates:
        a = ((du - math.pi) / 2) / DEG
        b = (dv / 2) / DEG
        a = a_cur + wrap180(a - a_cur)
        b = b_cur + wrap180(b - b_cur)
        travel = abs(a - a_cur) + abs(b - b_cur)
        if best is None or travel < best[0] - 1e-9:
            best = (travel, a, b)
    return delta, best[1], best[2]


def command_rx(S, C, ax, cal: Calib, current=(0.0, 45.0, 45.0)):
    return inverse(rx_to_pv(S, C, ax), cal, current)


def vertex_rx(S, C, ax, d_from_mm, d_to_mm):
    """Move a prescription from one vertex distance to another, per principal meridian: F2 = F1 / (1 - d F1)."""
    d = (d_from_mm - d_to_mm) / 1000.0
    f = lambda F: F / (1 - d * F)
    s2, c2 = f(S), f(S + C)
    return s2, c2 - s2, ax


def compose_refinement(S, C, ax, jcc=0, fog=0.0):
    """Clinical refinements in power-vector space. jcc = +1 / -1: +/-0.25 cross-cylinder (M = 0, |C| = 0.50 D)
    with its minus axis at ax +/- 45 deg. fog: plus sphere (D)."""
    M, J0, J45 = rx_to_pv(S, C, ax)
    M += fog
    if jcc:
        a = (ax + (45 if jcc > 0 else -45)) * DEG
        J0 += 0.25 * math.cos(2 * a)   # -(C/2) cos 2a with C = -0.50
        J45 += 0.25 * math.sin(2 * a)
    return M, J0, J45


if __name__ == "__main__":
    cal = Calib()
    for rx in [(-2.25, -0.75, 175), (1.5, -1.25, 90), (-4, -2, 30), (0, 0, 180)]:
        pos = command_rx(*rx, cal)
        back = pv_to_rx(*forward(pos, cal))
        print(rx, "->", tuple(round(v, 3) for v in pos), "->", tuple(round(v, 3) for v in back))
