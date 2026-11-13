"""Summarise the optometrist usability sessions (submission/BASIRA_usability_test_kit.pdf).
    python3 usability_summary.py usability_results.csv
SUS (Brooke 1996): odd items contribute (answer - 1), even items (5 - answer); sum x 2.5 gives 0-100.
Reports SUS per participant, mean with a 95 % t confidence interval, task success rates and median times."""
import csv, math, statistics as st, sys
from scipy import stats

def sus(row):
    a = [int(row[f"sus{i}"]) for i in range(1, 11)]
    if not all(1 <= x <= 5 for x in a): raise ValueError(f"{row['participant']}: SUS answers must be 1-5")
    return 2.5 * sum((x - 1) if i % 2 == 0 else (5 - x) for i, x in enumerate(a))

def summarise(path):
    rows = list(csv.DictReader(l for l in open(path, encoding="utf-8") if not l.startswith("#")))
    if not rows: return "no participants"
    out = ["# BASIRA usability summary\n", f"Participants: {len(rows)}\n\n| participant | role | years | SUS |\n|---|---|---|---|\n"]
    scores = []
    for r in rows:
        s = sus(r); scores.append(s)
        out.append(f"| {r['participant']} | {r['role']} | {r['years']} | {s:.1f} |\n")
    m = st.mean(scores)
    if len(scores) > 1:
        h = stats.t.ppf(0.975, len(scores) - 1) * st.stdev(scores) / math.sqrt(len(scores))
        out.append(f"\nSUS mean {m:.1f} (95 % CI {m - h:.1f} to {m + h:.1f}), SD {st.stdev(scores):.1f}, n = {len(scores)}. "
                   "Published average across studies is about 68; with small n the interval is wide.\n")
    else:
        out.append(f"\nSUS {m:.1f} (one participant).\n")
    out.append("\n| task | success (1 = unaided, 0.5 = helped) | unaided | median time (s) |\n|---|---|---|---|\n")
    for t in ["T1", "T2", "T3", "T4", "T5"]:
        v = [float(r[t]) for r in rows]
        times = [float(r[t + "_s"]) for r in rows if r.get(t + "_s")]
        out.append(f"| {t} | {100 * st.mean(v):.0f} % | {sum(x == 1 for x in v)}/{len(v)} | {st.median(times):.0f} |\n" if times else f"| {t} | {100 * st.mean(v):.0f} % | {sum(x == 1 for x in v)}/{len(v)} | - |\n")
    issues = [r["top_issue"] for r in rows if r.get("top_issue")]
    if issues: out.append("\nTop issues reported:\n" + "".join(f"- {x}\n" for x in issues))
    return "".join(out)

def selftest():
    row = lambda a: {"participant": "x", **{f"sus{i + 1}": v for i, v in enumerate(a)}}
    cases = [([5, 1] * 5, 100.0), ([1, 5] * 5, 0.0), ([3] * 10, 50.0), ([4, 2] * 5, 75.0)]
    ok = all(sus(row(a)) == want for a, want in cases)
    print("SUS scoring self-test:", "PASS (4 hand-computed cases)" if ok else "FAIL")
    return 0 if ok else 1

if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "--selftest": sys.exit(selftest())
    print(summarise(sys.argv[1] if len(sys.argv) > 1 else "usability_results.csv"))
