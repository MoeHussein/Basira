"""Fit the per-eye calibration model to lensmeter readings and check it on held-out readings.
    python3 fit_calibration.py READINGS.csv [--out DIR] [--model-id ID]
    python3 fit_calibration.py --selftest          (synthetic data with known truth)
    python3 fit_calibration.py NEW.csv --check calib_set_R.json   (periodic check of an existing model)
Outputs per eye: a calib_set message for the device (fitted=true only if the held-out check passes)
and a markdown report. Acceptance on >= 20 held-out settings (team target, blueprint rev 9 section 4):
  95th percentile |dM| <= 0.12 D, 95th percentile |dC| <= 0.12 D, and >= 95 % of axes within tolerance,
  tolerance = max(asin(0.12 / 2|C|), lensmeter axis resolution)  (see calib_model.axis_tolerance)."""
import argparse, datetime, json, math, os, sys
import numpy as np
from scipy.optimize import least_squares
from calib_model import read_csv, mech_from_meta, predict, errors, p95, rx_to_pv, PARAMS, LIMIT_D, AXIS_FLOOR_DEG


def residuals(x, rows, mech):
    p = dict(zip(PARAMS, x))
    out = []
    for r in rows:
        pv = predict(r, mech, p)
        m = rx_to_pv(r["lens_S"], r["lens_C"], r["lens_ax"])
        out.extend([pv[0] - m[0], pv[1] - m[1], pv[2] - m[2]])
    return np.array(out)


def fit_eye(rows, mech):
    best = None
    for za in range(0, 180, 15):                  # coarse start for the ring zeros (2-alpha periodic)
        for zb in range(0, 180, 15):
            x0 = np.array([0.0, 2.0, 3.0, -3.0, za, zb, 0.0, 0.0])
            c = float(np.sum(residuals(x0, rows, mech) ** 2))
            if best is None or c < best[0]: best = (c, x0)
    res = least_squares(residuals, best[1], args=(rows, mech), method="lm", x_scale="jac")
    x = res.x.copy()
    x[4] %= 180; x[5] %= 180                      # ring zeros are defined modulo 180 deg (optical symmetry)
    dof = max(1, len(res.fun) - len(x))
    s2 = float(np.sum(res.fun ** 2)) / dof
    try: se = np.sqrt(np.diag(np.linalg.inv(res.jac.T @ res.jac)) * s2)
    except np.linalg.LinAlgError: se = np.full(len(x), float("nan"))
    return dict(zip(PARAMS, x)), dict(zip(PARAMS, se)), math.sqrt(s2)


def evaluate(rows, mech, p):
    E = [errors(predict(r, mech, p), rx_to_pv(r["lens_S"], r["lens_C"], r["lens_ax"])) for r in rows]
    ax = [e for e in E if e["axTol"] is not None]
    nan = float("nan")
    return {"n": len(E), "p95_dM": p95([e["dM"] for e in E]), "p95_dC": p95([e["dC"] for e in E]),
            "axis_ok": (sum(e["dAx"] <= e["axTol"] for e in ax) / len(ax)) if ax else nan, "n_axis": len(ax),
            "max_dM": max(abs(e["dM"]) for e in E) if E else nan, "bias_M": float(np.mean([e["dM"] for e in E])) if E else nan,
            "p95_cylvec": p95([e["cylvec"] for e in E])}


def passes(ev):
    return ev["n"] >= 20 and ev["p95_dM"] <= LIMIT_D and ev["p95_dC"] <= LIMIT_D and (ev["n_axis"] == 0 or ev["axis_ok"] >= 0.95)


def run(path, out_dir, model_id=None):
    meta, rows = read_csv(path)
    os.makedirs(out_dir, exist_ok=True)
    stamp = datetime.date.today().isoformat()
    report = [f"# BASIRA calibration report\n\nSource: `{os.path.basename(path)}`  \nDate: {stamp}  \n"
              f"Acceptance on >= 20 held-out settings: 95th percentile |dM| and |dC| <= {LIMIT_D} D; >= 95 % of axes within "
              f"max(asin({LIMIT_D} / 2|C|), {AXIS_FLOOR_DEG} deg lensmeter resolution).\n"]
    if any("SYNTHETIC" in k or "SYNTHETIC" in v for k, v in meta.items()) or "SYNTHETIC" in os.path.basename(path):
        report.append("\n**SYNTHETIC DATA - not a device calibration.**\n")
    results = {}
    for eye in ("R", "L"):
        fit_rows = [r for r in rows if r["eye"] == eye and r["set"] == "fit"]
        hold = [r for r in rows if r["eye"] == eye and r["set"] == "holdout"]
        if not fit_rows: continue
        mech = mech_from_meta(meta, eye)
        p, se, rms = fit_eye(fit_rows, mech)
        ev_fit, ev_hold = evaluate(fit_rows, mech, p), evaluate(hold, mech, p)
        passed = passes(ev_hold)
        mid = (model_id or f"FIT-{stamp}") + f"-{eye}"
        msg = {"t": "calib_set", "id": 1, "eye": eye,
               "cal": {"P0": round(p["P0"], 4), "k": round(p["k"], 4), "Pp": round(p["Pp"], 4), "Pm": round(p["Pm"], 4),
                       "J0off": round(p["J0off"], 4), "J45off": round(p["J45off"], 4), "model_id": mid[:23]},
               "mech": {"zero_deg": [mech["zero_plate"], round(p["zero_a"], 3), round(p["zero_b"], 3)]},
               "fitted": passed}
        json.dump(msg, open(os.path.join(out_dir, f"calib_set_{eye}.json"), "w"))
        results[eye] = {"params": p, "se": se, "fit": ev_fit, "holdout": ev_hold, "passed": passed, "msg": msg}
        report.append(f"\n## Eye {eye}: {'PASS' if passed else 'FAIL'}\n\n| parameter | value | standard error |\n|---|---|---|\n")
        for k in PARAMS:
            report.append(f"| {k} | {p[k]:.4f} | {se[k]:.4f} |\n")
        report.append(f"\nFit: {ev_fit['n']} readings, residual RMS {rms:.3f} D.  \n"
                      f"Held-out: {ev_hold['n']} readings: p95 |dM| {ev_hold['p95_dM']:.3f} D, p95 |dC| {ev_hold['p95_dC']:.3f} D, "
                      f"axes within tolerance {100 * ev_hold['axis_ok']:.0f} % of {ev_hold['n_axis']}, max |dM| {ev_hold['max_dM']:.3f} D, "
                      f"mean dM {ev_hold['bias_M']:+.3f} D (p95 cylinder-vector error {ev_hold['p95_cylvec']:.3f} D, for information).\n")
        if len(hold) < 20: report.append("\nFewer than 20 held-out readings: cannot pass (blueprint section 8).\n")
        if abs(p["Pp"] + p["Pm"]) > 0.12:
            report.append(f"\nWarning: |Pp + Pm| = {abs(p['Pp'] + p['Pm']):.2f} D > 0.12 D. The cylinder pair cannot cancel below that value; use a matched pair.\n")
    open(os.path.join(out_dir, "calibration_report.md"), "w").write("".join(report))
    return results


def selftest():
    import tempfile
    from make_synthetic import main as make, TRUTH
    d = tempfile.mkdtemp()
    path = os.path.join(d, "SYNTHETIC.csv")
    make(path)
    res = run(path, d, "SELFTEST")
    ok = True
    for eye, r in res.items():
        for k in TRUTH[eye]:
            v, t = r["params"][k], TRUTH[eye][k]
            if k.startswith("zero"):
                diff = abs(((v - t) + 90) % 180 - 90)
                tol = 0.5
            else:
                diff, tol = abs(v - t), 0.02
            if diff > tol: ok = False; print(f"  {eye}.{k}: fitted {v:.4f}, truth {t:.4f} (diff {diff:.4f} > {tol})")
        h = r["holdout"]
        print(f"eye {eye}: held-out p95 |dM| {h['p95_dM']:.3f} D, p95 |dC| {h['p95_dC']:.3f} D, axes in tolerance {100 * h['axis_ok']:.0f} %, {'PASS' if r['passed'] else 'FAIL'}")
        ok = ok and r["passed"]
    # negative control: a +0.25 D sphere error on the held-out readings must be caught
    meta, rows = read_csv(path)
    bad = [dict(r, lens_S=r["lens_S"] + 0.25) if r["set"] == "holdout" else r for r in rows]
    mech = mech_from_meta(meta, "R")
    p, _, _ = fit_eye([r for r in bad if r["eye"] == "R" and r["set"] == "fit"], mech)
    caught = not passes(evaluate([r for r in bad if r["eye"] == "R" and r["set"] == "holdout"], mech, p))
    print("negative control (+0.25 D on held-out readings):", "FAIL detected as expected" if caught else "NOT DETECTED")
    ok = ok and caught
    print("calibration self-test:", "PASS (fit recovers the true parameters; a real error is rejected)" if ok else "FAIL")
    return 0 if ok else 1


def check(path, model_path):
    """Periodic check: evaluate an existing calib_set model on new held-out readings (nothing is fitted)."""
    meta, rows = read_csv(path)
    msg = json.load(open(model_path))
    eye, c, z = msg["eye"], msg["cal"], msg["mech"]["zero_deg"]
    p = {"P0": c["P0"], "k": c["k"], "Pp": c["Pp"], "Pm": c["Pm"], "J0off": c["J0off"], "J45off": c["J45off"], "zero_a": z[1], "zero_b": z[2]}
    ev = evaluate([r for r in rows if r["eye"] == eye], mech_from_meta(meta, eye), p)
    ok = ev["n"] >= 10 and ev["p95_dM"] <= LIMIT_D and ev["p95_dC"] <= LIMIT_D and (ev["n_axis"] == 0 or ev["axis_ok"] >= 0.95)
    print(f"check {c.get('model_id')} on {ev['n']} readings: p95 |dM| {ev['p95_dM']:.3f} D, p95 |dC| {ev['p95_dC']:.3f} D, "
          f"axes in tolerance {100 * ev['axis_ok']:.0f} % -> {'PASS' if ok else 'FAIL: model invalid, refit'}")
    return 0 if ok else 1


if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("csv", nargs="?")
    ap.add_argument("--out", default=os.path.join(os.path.dirname(os.path.abspath(__file__)), "out"))
    ap.add_argument("--model-id")
    ap.add_argument("--selftest", action="store_true")
    ap.add_argument("--check", metavar="CALIB_SET_JSON", help="evaluate this existing model on the readings instead of fitting")
    a = ap.parse_args()
    if a.check: sys.exit(check(a.csv, a.check))
    if a.selftest or not a.csv: sys.exit(selftest())
    res = run(a.csv, a.out, a.model_id)
    for eye, r in res.items(): print(f"eye {eye}: {'PASS' if r['passed'] else 'FAIL'} -> {os.path.join(a.out, 'calib_set_' + eye + '.json')}")
