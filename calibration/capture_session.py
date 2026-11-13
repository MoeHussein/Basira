"""Calibration capture over USB: drives one eye through random settings and records the lensmeter reading at each.
    pip install pyserial
    python3 capture_session.py --port /dev/ttyUSB0 --eye R --fit 40 --holdout 20 --out readings_R.csv
For each setting the device moves (set_pos), confirms from its sensors, and the raw sensor angles are stored; the
operator reads the lensmeter (back vertex power of the eye's optical stack, measured from the eye side) and types
S C AX. Type 'r' to repeat a setting, 'q' to stop (rows so far are kept). Then run fit_calibration.py on the CSV.
Not yet run against hardware: check the first few rows by hand."""
import argparse, csv, json, random, sys, time
from calib_model import FIELDS

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", required=True); ap.add_argument("--baud", type=int, default=460800)
    ap.add_argument("--eye", choices=["R", "L"], required=True)
    ap.add_argument("--fit", type=int, default=40); ap.add_argument("--holdout", type=int, default=20)
    ap.add_argument("--travel", type=float, default=4.5, help="plate shift range +/- mm")
    ap.add_argument("--seed", type=int, default=None); ap.add_argument("--out", required=True)
    a = ap.parse_args()
    import serial
    sp = serial.Serial(a.port, a.baud, timeout=0.1)
    nid = [100]

    def send(o):
        nid[0] += 1; o["id"] = nid[0]; sp.write((json.dumps(o) + "\n").encode()); return nid[0]

    def wait(pred, timeout=8.0):
        t0 = time.time()
        while time.time() - t0 < timeout:
            line = sp.readline().decode(errors="ignore").strip()
            if line.startswith("{"):
                try: m = json.loads(line)
                except ValueError: continue
                if m.get("t") == "fault": raise RuntimeError(f"device fault {m}")
                if pred(m): return m
            sp.write(b'{"t":"ping"}\n')
        raise TimeoutError("no answer from the device")

    send({"t": "calib_get", "eye": a.eye})
    cal = wait(lambda m: m.get("t") == "calib" and m.get("eye") == a.eye)
    mech = cal["mech"]
    rnd = random.Random(a.seed)
    plan = [("fit" if i < a.fit else "holdout", rnd.uniform(-a.travel, a.travel), rnd.uniform(0, 180), rnd.uniform(0, 180))
            for i in range(a.fit + a.holdout)]
    with open(a.out, "w", newline="") as f:
        f.write(f"# BASIRA calibration readings, eye {a.eye}, {time.strftime('%Y-%m-%d %H:%M')}, device model {cal['cal'].get('model_id')}\n")
        f.write(f"# {a.eye}.pinion_r_mm={mech['pinion_r_mm']}\n# {a.eye}.zero_plate={mech['zero_deg'][0]}\n")
        f.write(f"# {a.eye}.sign_plate={mech['sign'][0]}\n# {a.eye}.sign_ringP={mech['sign'][1]}\n# {a.eye}.sign_ringM={mech['sign'][2]}\n")
        w = csv.DictWriter(f, FIELDS); w.writeheader()
        i = 0
        while i < len(plan):
            st, d, al, be = plan[i]
            mid = send({"t": "set_pos", "eye": a.eye, "delta": round(d, 4), "alpha": round(al, 2), "beta": round(be, 2)})
            wait(lambda m: m.get("t") == "confirmed" and m.get("id") == mid)
            s = wait(lambda m: m.get("t") == "status" and m.get("st") == "CONFIRMED")
            raw = s[a.eye]["raw"]
            ans = input(f"[{i + 1}/{len(plan)} {st}] lensmeter S C AX (e.g. -1.25 -0.75 92), r = repeat, q = quit: ").strip()
            if ans == "q": break
            if ans == "r": continue
            try: S, C, AX = map(float, ans.replace(",", " ").split())
            except ValueError: print("  expected three numbers"); continue
            if C > 0: S, C, AX = S + C, -C, (AX + 90) % 180 or 180      # store minus cylinder
            w.writerow({"point": f"{a.eye}{i:02d}", "set": st, "eye": a.eye, "raw_plate": raw[0], "raw_ringP": raw[1],
                        "raw_ringM": raw[2], "lens_S": S, "lens_C": C, "lens_ax": AX})
            f.flush(); i += 1
    print("saved", a.out)

if __name__ == "__main__":
    main()
