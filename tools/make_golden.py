"""Generate tests/golden_vectors.json from the reference model (physics/basira_optics.py).
The firmware (C++) and app (JS) test suites must reproduce these numbers. Run from the repo root:
    python3 tools/make_golden.py
"""
import json, os, random, sys
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "physics"))
from basira_optics import Calib, forward, inverse, rx_to_pv, pv_to_rx, vertex_rx, compose_refinement, OutOfRange

random.seed(20261113)
cals = [Calib(),
        Calib(P0=0.12, k=1.93, Pp=3.04, Pm=-2.97, J0off=0.03, J45off=-0.02, model_id="EXAMPLE-A"),
        Calib(P0=-0.2, k=2.08, Pp=2.96, Pm=-3.05, J0off=-0.05, J45off=0.04, model_id="EXAMPLE-B")]
cases = []
for ci, cal in enumerate(cals):
    for _ in range(60):
        C = -round(random.uniform(0, 5.75) * 4) / 4
        ax = random.randint(1, 180)
        S = round((random.uniform(-9.5, 9.5) - C / 2) * 4) / 4
        cur = (round(random.uniform(-3, 3), 3), round(random.uniform(-200, 200), 2), round(random.uniform(-200, 200), 2))
        case = {"cal": ci, "rx": [S, C, ax], "cur": list(cur)}
        try:
            pos = inverse(rx_to_pv(S, C, ax), cal, cur)
            case["pos"] = list(pos)
            case["pv"] = list(forward(pos, cal))
        except OutOfRange as e:
            case["error"] = "SPHERE_RANGE" if "sphere" in str(e) else "CYL_RANGE"
        cases.append(case)
for bad, code in [((-12, 0, 90), "SPHERE_RANGE"), ((0, -6.5, 90), "CYL_RANGE"), ((11, -1, 45), "SPHERE_RANGE")]:
    cases.append({"cal": 0, "rx": list(bad), "cur": [0, 45, 45], "error": code})

conv = []
for _ in range(40):
    S, C, ax = round(random.uniform(-10, 8), 2), -round(random.uniform(0, 6), 2), random.randint(1, 180)
    pv = rx_to_pv(S, C, ax)
    conv.append({"rx": [S, C, ax], "pv": list(pv), "rx_back": list(pv_to_rx(*pv)),
                 "vertex_13_12": list(vertex_rx(S, C, ax, 13, 12)), "vertex_13_0": list(vertex_rx(S, C, ax, 13, 0)),
                 "jcc_plus": list(compose_refinement(S, C, ax, jcc=+1)), "jcc_minus": list(compose_refinement(S, C, ax, jcc=-1)),
                 "fog": list(compose_refinement(S, C, ax, fog=1.0))})

out = {"generator": "tools/make_golden.py", "model": "physics/basira_optics.py",
       "cals": [c.to_dict() for c in cals], "inverse_cases": cases, "conversions": conv}
path = os.path.join(os.path.dirname(__file__), "..", "tests", "golden_vectors.json")
json.dump(out, open(path, "w"), indent=1)
print(f"wrote {os.path.normpath(path)}: {len(cases)} inverse cases ({sum('error' in c for c in cases)} out of range), {len(conv)} conversion cases")
