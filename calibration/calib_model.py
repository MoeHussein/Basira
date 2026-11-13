"""Calibration model shared by the fitting and bench-verification scripts.
A reading = one lensmeter measurement of one eye's optical stack at a set of raw sensor angles.
Model (blueprint rev 9, section 8, thin elements), per eye:
    delta = sign_p * r * rad(raw_plate - zero_plate)      plate zero fixed (it is degenerate with P0)
    alpha = sign_a * (raw_ringP - zero_a),   beta = sign_b * (raw_ringM - zero_b)
    power = forward((delta, alpha, beta), Calib(P0, k, Pp, Pm, J0off, J45off))
Fitted parameters: P0, k, Pp, Pm, zero_a, zero_b, J0off, J45off."""
import csv, math, os, sys
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "physics"))
from basira_optics import Calib, forward, rx_to_pv, pv_to_rx, inverse, OutOfRange

FIELDS = ["point", "set", "eye", "raw_plate", "raw_ringP", "raw_ringM", "lens_S", "lens_C", "lens_ax"]
PARAMS = ["P0", "k", "Pp", "Pm", "zero_a", "zero_b", "J0off", "J45off"]


def read_csv(path):
    meta, rows = {}, []
    with open(path, newline="") as f:
        lines = [l for l in f]
    for l in lines:
        if l.startswith("#") and "=" in l:
            k, v = l[1:].strip().split("=", 1)
            meta[k.strip()] = v.strip()
    for r in csv.DictReader([l for l in lines if not l.startswith("#")]):
        for k in FIELDS:
            if k not in ("point", "set", "eye"): r[k] = float(r[k])
        rows.append(r)
    return meta, rows


def mech_from_meta(meta, eye):
    g = lambda k, d: float(meta.get(f"{eye}.{k}", d))
    return {"r": g("pinion_r_mm", 5.0), "zero_plate": g("zero_plate", 0.0),
            "sign_p": g("sign_plate", 1), "sign_a": g("sign_ringP", 1), "sign_b": g("sign_ringM", 1)}


def positions(row, mech, zero_a, zero_b):
    d = mech["sign_p"] * mech["r"] * math.radians(row["raw_plate"] - mech["zero_plate"])
    return d, mech["sign_a"] * (row["raw_ringP"] - zero_a), mech["sign_b"] * (row["raw_ringM"] - zero_b)


def predict(row, mech, p):
    cal = Calib(P0=p["P0"], k=p["k"], Pp=p["Pp"], Pm=p["Pm"], J0off=p["J0off"], J45off=p["J45off"])
    return forward(positions(row, mech, p["zero_a"], p["zero_b"]), cal)


LIMIT_D = 0.12          # team target for sphere-equivalent and cylinder power (blueprint rev 9, section 4)
AXIS_FLOOR_DEG = 2.0    # assumed axis resolution of the reference lensmeter (set to your instrument)


def axis_tolerance(C):
    """Axis error that creates a LIMIT_D cylinder error, asin(LIMIT_D / 2|C|) (the reasoning behind ANSI Z80.1
    axis tolerances), but never below the reference instrument's axis resolution. None when |C| < LIMIT_D."""
    C = abs(C)
    if C < LIMIT_D: return None
    return max(AXIS_FLOOR_DEG, math.degrees(math.asin(min(1.0, LIMIT_D / (2 * C)))))


def errors(pred_pv, meas_pv):
    """Per-setting errors: dM (D), dC (D, cylinder magnitude), dAx (deg, None if no cylinder), axis tolerance (deg),
    and the cylinder-vector error 2|dJ| (D, reported for information)."""
    p, m = pv_to_rx(*pred_pv), pv_to_rx(*meas_pv)
    dAx, tol = None, axis_tolerance(m[1])
    if tol is not None:
        dAx = abs(((p[2] - m[2]) + 90) % 180 - 90)
    return {"dM": pred_pv[0] - meas_pv[0], "dC": abs(p[1]) - abs(m[1]), "dAx": dAx, "axTol": tol,
            "cylvec": 2 * math.hypot(pred_pv[1] - meas_pv[1], pred_pv[2] - meas_pv[2])}


def p95(xs):
    xs = sorted(abs(x) for x in xs)
    if not xs: return float("nan")
    i = 0.95 * (len(xs) - 1)
    lo, hi = int(math.floor(i)), int(math.ceil(i))
    return xs[lo] + (xs[hi] - xs[lo]) * (i - lo)
