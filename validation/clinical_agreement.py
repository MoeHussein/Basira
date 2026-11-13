"""Agreement and repeatability of BASIRA refraction against a reference refraction, in power vectors (M, J0, J45).
    python3 clinical_agreement.py DATA.csv [--eye R|L|both] [--out DIR]
    python3 clinical_agreement.py --make-example       (writes SYNTHETIC example data, then analyses it)
DATA.csv columns: subject, eye (R/L), method (device/reference), session (1 = first, 2 = repeat), S, C, ax  (minus cylinder).
Methods (Bland & Altman 1986/1999; Thibos 1997 power vectors):
  agreement     device session 1 - reference, per component: bias, SD, 95 % limits of agreement (LoA) = bias +/- 1.96 SD,
                95 % CI of the bias (t) and of each limit (approximate SE = sqrt(3 SD^2 / n)).
  repeatability device session 2 - session 1: within-subject SD Sw = sqrt(sum d^2 / 2n), repeatability = 2.77 Sw.
One eye per subject by default (both eyes of a person are correlated; --eye both reports them but says so).
Benchmark: Salvador-Roger et al., PLoS One 2025 (tunable sphere + Stokes lens vs optometrist): LoA M +/-0.64 D,
J0 +/-0.36 D, J45 +/-0.26 D (as summarised in docs/BASIRA_final_blueprint_rev9.md)."""
import argparse, csv, math, os, random, sys
import numpy as np
from scipy import stats

BENCH = {"M": 0.64, "J0": 0.36, "J45": 0.26}
COMP = ["M", "J0", "J45"]


def pv(S, C, ax):
    a = math.radians(ax)
    return {"M": S + C / 2, "J0": -(C / 2) * math.cos(2 * a), "J45": -(C / 2) * math.sin(2 * a)}


def load(path):
    rows = []
    with open(path, newline="") as f:
        for r in csv.DictReader(l for l in f if not l.startswith("#")):
            r["session"] = int(r.get("session") or 1)
            r.update(pv(float(r["S"]), float(r["C"]), float(r["ax"])))
            rows.append(r)
    return rows


def pairs(rows, eye, a, b):
    """{(subject, eye): (value_a, value_b)} for method/session keys a and b, e.g. ('device', 1)."""
    idx = {}
    for r in rows:
        if eye != "both" and r["eye"] != eye: continue
        idx[(r["subject"], r["eye"], r["method"], r["session"])] = r
    out = {}
    for (s, e, m, ses), r in idx.items():
        if (m, ses) == a and (s, e, b[0], b[1]) in idx: out[(s, e)] = (r, idx[(s, e, b[0], b[1])])
    return out


def agreement(d):
    d = np.asarray(d, float); n = len(d)
    bias, sd = float(d.mean()), float(d.std(ddof=1))
    t = stats.t.ppf(0.975, n - 1)
    se_b, se_l = sd / math.sqrt(n), math.sqrt(3 * sd * sd / n)
    return {"n": n, "bias": bias, "sd": sd, "bias_ci": (bias - t * se_b, bias + t * se_b),
            "loa": (bias - 1.96 * sd, bias + 1.96 * sd), "loa_lo_ci": (bias - 1.96 * sd - t * se_l, bias - 1.96 * sd + t * se_l),
            "loa_hi_ci": (bias + 1.96 * sd - t * se_l, bias + 1.96 * sd + t * se_l)}


def plot(means, diffs, res, comp, path):
    try:
        import matplotlib; matplotlib.use("Agg"); import matplotlib.pyplot as plt
    except ImportError:
        return False
    fig, ax = plt.subplots(figsize=(5.2, 3.6), dpi=150)
    ax.scatter(means, diffs, s=14, color="#0F766E", alpha=.8, edgecolors="none")
    for y, ls in [(res["bias"], "-"), (res["loa"][0], "--"), (res["loa"][1], "--")]: ax.axhline(y, color="#13212B", ls=ls, lw=1)
    for y in (-BENCH[comp], BENCH[comp]): ax.axhline(y, color="#B4410C", ls=":", lw=1)
    ax.set_xlabel(f"mean of device and reference, {comp} (D)"); ax.set_ylabel(f"device - reference, {comp} (D)")
    ax.set_title(f"Bland-Altman, {comp} (n = {res['n']}); dotted = 2025 benchmark LoA", fontsize=8)
    fig.tight_layout(); fig.savefig(path); plt.close(fig)
    return True


def analyse(path, eye, out):
    rows = load(path)
    os.makedirs(out, exist_ok=True)
    synthetic = "SYNTHETIC" in os.path.basename(path) or any("SYNTHETIC" in l for l in open(path) if l.startswith("#"))
    rep = ["# BASIRA clinical agreement report\n\n", f"Data: `{os.path.basename(path)}`, eye: {eye}\n\n"]
    if synthetic: rep.append("**SYNTHETIC DATA - illustrates the analysis only; these are not results.**\n\n")
    if eye == "both": rep.append("Note: both eyes included; fellow eyes are correlated, so CIs are too narrow.\n\n")
    ag = pairs(rows, eye, ("device", 1), ("reference", 1))
    rep.append("## Agreement: device (session 1) - reference\n\n| component | n | bias (95 % CI) | 95 % LoA | LoA 95 % CI | benchmark LoA |\n|---|---|---|---|---|---|\n")
    summary = {}
    for c in COMP:
        d = [p[0][c] - p[1][c] for p in ag.values()]
        if len(d) < 3: rep.append(f"| {c} | {len(d)} | too few pairs | | | |\n"); continue
        r = agreement(d); summary[c] = r
        rep.append(f"| {c} | {r['n']} | {r['bias']:+.2f} ({r['bias_ci'][0]:+.2f}, {r['bias_ci'][1]:+.2f}) D | {r['loa'][0]:+.2f} to {r['loa'][1]:+.2f} D "
                   f"| low {r['loa_lo_ci'][0]:+.2f}..{r['loa_lo_ci'][1]:+.2f}, high {r['loa_hi_ci'][0]:+.2f}..{r['loa_hi_ci'][1]:+.2f} | +/-{BENCH[c]:.2f} D |\n")
        plot([(p[0][c] + p[1][c]) / 2 for p in ag.values()], d, r, c, os.path.join(out, f"bland_altman_{c}.png"))
    if "M" in summary:
        dM = [abs(p[0]["M"] - p[1]["M"]) for p in ag.values()]
        rep.append(f"\nWithin +/-0.25 D (M): {100 * np.mean([x <= 0.25 for x in dM]):.0f} %; within +/-0.50 D: {100 * np.mean([x <= 0.50 for x in dM]):.0f} %.\n")
    rp = pairs(rows, eye, ("device", 2), ("device", 1))
    rep.append("\n## Repeatability: device session 2 - session 1\n\n| component | n | mean difference | Sw | repeatability (2.77 Sw) |\n|---|---|---|---|---|\n")
    for c in COMP:
        d = np.array([p[0][c] - p[1][c] for p in rp.values()])
        if len(d) < 3: rep.append(f"| {c} | {len(d)} | too few repeats | | |\n"); continue
        sw = math.sqrt(float(np.sum(d * d)) / (2 * len(d)))
        rep.append(f"| {c} | {len(d)} | {d.mean():+.3f} D | {sw:.3f} D | {2.77 * sw:.2f} D |\n")
    rep.append("\nReading the table: a bias whose CI excludes 0 is a systematic offset (check calibration and the chart-distance "
               "correction); limits of agreement are judged against the clinical need, with the 2025 benchmark as context, "
               "and the CI of each limit shows how much the sample size limits that judgement.\n")
    open(os.path.join(out, "agreement_report.md"), "w").write("".join(rep))
    return summary


def make_example(path, n=40, seed=3):
    rnd = random.Random(seed)
    with open(path, "w", newline="") as f:
        f.write("# SYNTHETIC example generated by validation/clinical_agreement.py --make-example - NOT clinical data\n")
        w = csv.writer(f); w.writerow(["subject", "eye", "method", "session", "S", "C", "ax"])
        for i in range(n):
            for eye in ("R", "L"):
                S, C, ax = rnd.uniform(-6, 3), -rnd.choice([0, 0.25, 0.5, 0.75, 1, 1.5, 2, 3]), rnd.randint(1, 180)
                q = lambda x: round(x * 4) / 4
                w.writerow([f"S{i:03d}", eye, "reference", 1, q(S), q(C), ax])
                for ses in (1, 2):
                    w.writerow([f"S{i:03d}", eye, "device", ses, q(S + 0.05 + rnd.gauss(0, 0.22)), min(0, q(C + rnd.gauss(0, 0.15))),
                                (ax + round(rnd.gauss(0, 6))) % 180 or 180])


if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("csv", nargs="?"); ap.add_argument("--eye", default="R", choices=["R", "L", "both"])
    ap.add_argument("--out", default=None); ap.add_argument("--make-example", action="store_true")
    a = ap.parse_args()
    here = os.path.dirname(os.path.abspath(__file__))
    if a.make_example:
        a.csv = os.path.join(here, "examples", "SYNTHETIC_clinical_pairs.csv"); make_example(a.csv)
        a.out = a.out or os.path.join(here, "examples", "SYNTHETIC_agreement_output")
    if not a.csv: ap.error("give a CSV or --make-example")
    s = analyse(a.csv, a.eye, a.out or os.path.join(here, "out"))
    for c, r in s.items(): print(f"{c}: bias {r['bias']:+.3f} D, LoA {r['loa'][0]:+.2f} to {r['loa'][1]:+.2f} D (n = {r['n']})")
