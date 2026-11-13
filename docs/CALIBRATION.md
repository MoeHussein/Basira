# Bring-up and calibration

Blueprint rev 9, section 8: the model is invalid after any module swap or failed check. Repeat steps 3–5 whenever that happens.

## 1. Bring-up (each of the 6 drives)

Use the app's engineering box, or a serial monitor at 460800 baud.

1. Send `{"t":"hello"}`. The device should report `READY`. If it reports `SENSOR`, check the magnet, CS line and SPI wiring for the axis named in the message.
2. **Direction.** Send `{"t":"jog","id":1,"axis":0,"duty":0.4,"ms":300}` and watch `status.R.pos[0]`.
   - If the position goes down, set `mech.motor_dir` for that axis to −1.
   - If you want the physical direction reversed, change `sign` instead (raw angle direction).
3. **Minimum duty (`dmin`).** Raise `duty` in steps of 0.05 until the drive moves reliably in **both** directions. Use that value plus 0.05. A `dmin` set too low gives `STALL` instead of a hang.
4. **Zero.** Move the element to its mechanical reference: plate pair centred, ring index marks at 0°. Read `status.<eye>.raw[j]` and send it as `mech.zero_deg[j]`.
5. Send `{"t":"calib_set","id":2,"eye":"R","mech":{"motor_dir":[1,1,1],"sign":[1,1,-1],"dmin":[0.32,0.30,0.30],"zero_deg":[120.0,4.0,178.0],"pinion_r_mm":5.0}}`, using your own values.

## 2. Lens check before assembly

- Measure each ±3.00 D cylinder lens on the lensmeter.
- The pair must match: |P₊ + P₋| ≤ 0.12 D. Otherwise cylinders below that value cannot be produced (`docs/ARCHITECTURE.md`).

## 3. Capture (per eye)

```
pip install pyserial
python3 calibration/capture_session.py --port /dev/ttyUSB0 --eye R --fit 40 --holdout 20 --out readings_R.csv
```

- The device moves to 60 random settings and confirms each one from its sensors.
- At each setting, read the lensmeter through the whole optical stack from the eye side, and type `S C AX`.
- The fitter wraps the raw encoder difference at 0°/360° to match the firmware's initial reference. The plate must stay within half a shaft turn of its zero. Whole turns of either cylinder ring are optically equivalent.
- Blueprint: 30–60 fit readings and at least 20 held-out readings per eye.

## 4. Fit and validate

```
python3 calibration/fit_calibration.py readings_R.csv --out calib_out --model-id BASIRA01-2026-12
```

- **What is fitted:** P0, k, Pp, Pm, both ring zeros and constant J0/J45 offsets, by least squares in power-vector space. The report gives standard errors.
- **Pass criteria:** on the held-out readings,
  - the 95th percentile of |ΔM| ≤ 0.12 D;
  - the 95th percentile of |ΔC| ≤ 0.12 D;
  - the 95th percentile of the combined cylinder-vector error, 2·hypot(ΔJ0, ΔJ45), ≤ 0.12 D;
  - at least 95 % of axes within asin(0.12 / (2|C|)), expressed in degrees, for settings with measured |C| ≥ 0.12 D.
- **Lensmeter capability:** at 6 D the axis limit is approximately 0.57°. Record the reference instrument's resolution and repeatability. If it cannot resolve the limit, record the result as unverified and use a finer reference measurement.
- **Output:** `calib_set_R.json`, with `"fitted": true` only on PASS.
- **Check the fit itself:** `python3 calibration/fit_calibration.py --selftest` verifies recovery of known synthetic parameters and checks that noiseless holdouts pass. The supplied noisy holdouts and a planted sphere bias are rejected.

## 5. Upload

After a physical holdout passes, send the content of `calib_set_R.json` (one line) through the app's engineering box or a serial monitor. The device saves it to flash and echoes `calib`. The `confirmed.calibrated` field reflects the stored `fitted` flag and `model` carries the model ID.

## Periodic check

- Before each session day, capture 10 settings, then evaluate the existing model on them (nothing is fitted):

```
python3 calibration/capture_session.py --port /dev/ttyUSB0 --eye R --fit 0 --holdout 10 --out check_R.csv
python3 calibration/fit_calibration.py check_R.csv --check calib_out/calib_set_R.json
```

- The periodic check applies the same optical limits to at least 10 readings. If it fails, invalidate the model and refit. Recheck models accepted under the earlier 2° floor against the current criteria before relying on their fitted status.
