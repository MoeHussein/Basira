"""Verification of the reference optics model. Run: python3 test_basira_optics.py"""
import math, random
from basira_optics import Calib, forward, inverse, rx_to_pv, pv_to_rx, command_rx, vertex_rx, compose_refinement, wrap180, OutOfRange

def close(a, b, tol): return all(abs(x - y) <= tol for x, y in zip(a, b))
random.seed(7)

# 1. nominal calibration: round trip over the whole range, and agreement with the closed form
cal, n = Calib(), 0
for i in range(-40, 25):
    for j in range(0, 25):
        S, C = i * 0.25, -j * 0.25
        if abs(S + C / 2) > 10: continue
        for ax in range(1, 181, 7):
            d, a, b = command_rx(S, C, ax, cal, current=(0, random.uniform(0, 180), random.uniform(0, 180)))
            assert close(forward((d, a, b), cal), rx_to_pv(S, C, ax), 1e-9), (S, C, ax)
            if C < 0:  # closed form: alpha - beta = asin(|C|/2P) (mod 180), mean = ax + 45 (mod 90 for the pair)
                D = math.degrees(math.asin(-C / 6))
                assert abs(wrap180(a - b) - D) < 1e-6 or abs(wrap180(b - a) - D) < 1e-6
            n += 1
print(f"nominal round trip + closed form: {n} prescriptions OK")

# 2. calibrated lenses: unequal cylinder powers and fixed J offsets
n_res = 0
for trial in range(2000):
    c = Calib(P0=random.uniform(-.3, .3), k=random.uniform(1.8, 2.2), Pp=random.uniform(2.85, 3.15), Pm=-random.uniform(2.85, 3.15),
              J0off=random.uniform(-.08, .08), J45off=random.uniform(-.08, .08))
    C, ax = -random.uniform(0.25, 5.5), random.uniform(1, 180); S = random.uniform(-9, 9) - C / 2
    pos = command_rx(S, C, ax, c, current=(0, random.uniform(0, 180), random.uniform(0, 180)))
    f, t = forward(pos, c), rx_to_pv(S, C, ax)
    T, gap = math.hypot(t[1] - c.J0off, t[2] - c.J45off), abs(c.Pp + c.Pm) / 2
    if T >= gap:
        assert close(f, t, 1e-9)
    else:   # mismatched pair cannot cancel below |Pp + Pm|: residual must be the minimum possible, gap - T
        assert abs(f[0] - t[0]) < 1e-9 and abs(math.hypot(f[1] - t[1], f[2] - t[2]) - (gap - T)) < 1e-9
        n_res += 1
print(f"calibrated round trip (unequal powers, offsets): 2000 OK, {n_res} low-cylinder cases at minimum residual")

# 3. minimum-travel choice: a small change never swings the rings far
c = Calib(); cur = command_rx(-2, -1.0, 90, c)
nxt = command_rx(-2, -1.0, 92, c, current=cur)
assert abs(nxt[1] - cur[1]) < 3 and abs(nxt[2] - cur[2]) < 3, (cur, nxt)
print("minimum-travel ring solution OK")

# 4. range checks
for bad in [(-12, 0, 90), (0, -6.5, 90)]:
    try: command_rx(*bad, Calib()); raise AssertionError(bad)
    except OutOfRange: pass
print("out-of-range rejected OK")

# 5. conversions
for trial in range(5000):
    S, C, ax = random.uniform(-10, 8), -random.uniform(0.01, 6), random.uniform(0.5, 180)
    S2, C2, ax2 = pv_to_rx(*rx_to_pv(S, C, ax))
    assert abs(S2 - S) < 1e-9 and abs(C2 - C) < 1e-9 and abs(wrap180(ax2 - ax)) < 1e-6, (S, C, ax, S2, C2, ax2)
s, cc, a = pv_to_rx(*rx_to_pv(-2.25, -0.75, 175)); assert close((s, cc, a), (-2.25, -0.75, 175), 1e-9)
s2, c2, _ = vertex_rx(-10, 0, 90, 13, 12); assert abs(s2 - (-10 / (1 + 0.001 * 10))) < 1e-12
M, J0, J45 = compose_refinement(-2, -1, 90, jcc=+1); S1, C1, ax1 = pv_to_rx(M, J0, J45)
assert abs(M - rx_to_pv(-2, -1, 90)[0]) < 1e-12  # JCC keeps the spherical equivalent
print("conversions and JCC OK")
print("ALL PASS")
