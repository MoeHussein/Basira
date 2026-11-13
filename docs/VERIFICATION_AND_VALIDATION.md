# Verification and validation plan

Verification asks whether the system does what was specified; validation asks whether it gives correct prescriptions for people. "Done" means the test exists and passes now (`./run_tests.sh`). "Hardware" means the method and tooling exist, and the measurement waits for the parts.

| ID | Requirement | Method | Status |
|---|---|---|---|
| V1 | Optics model correct: element positions reproduce the target power vector, including unequal cylinder powers and J offsets | `physics/test_basira_optics.py`: 38,194 prescriptions (nominal) and 2,000 random calibrations, plus the closed-form check | Done |
| V2 | Firmware (C++) and app (JS) implement the same model | `tests/golden_vectors.json` (183 inverse cases, 40 conversion cases) checked in `firmware/test_host` and `tests/app_test.js` | Done: power agrees to about 1e-14 D in both ports |
| V3 | Range limits: M within ±10 D, \|C\| ≤ 6 D, minus-cylinder only | Host and app tests (`RANGE_SPHERE`, `RANGE_CYL`, `BAD_RX`) | Done |
| V4 | A value is reported only from **sensed** positions, after settling and verification | Host test: exam flow on a simulated drive with sensor noise; review of `device.cpp` (VERIFYING) | Done |
| V5 | Any fault stops all motion and makes the correction unknown: sensor, stall, timeout, driver, link, operator stop, drift | Host tests (each fault injected) and browser test (result shown as "غير معروف", meaning unknown) | Done in simulation; **Hardware:** repeat each fault on the bench |
| V6 | Leaving FAULT needs fresh good sensor reads | Host test: reset refused while the sensor is dead | Done |
| V7 | Motion needs a live app link (1.5 s) | Host test and simulated-device test | Done |
| V8 | Nominal calibration is always visible | Host test (`calibrated:false`) and browser test (banner) | Done |
| V9 | Chart-distance, vertex and corneal conversions are correct | Golden conversions; browser test: patient A at 6 m with the max-plus endpoint saves −2.25 / −0.75 × 175 (its true prescription) | Done |
| V10 | Cross-cylinder flips keep M constant | Python test and golden vectors | Done |
| V11 | The exam record holds requested and confirmed values, BVD, PD, chart distance, model id and faults | Browser test (downloaded record) | Done |
| V12 | Serial status stream fits the link | Host test: 239 B per status, against a 2,304 B budget at 20 Hz | Done |
| V13 | Firmware builds for the target | `arduino-cli compile`, ESP32-S3, core 3.0.7: 74 % flash, 13 % RAM | Done (compiled, not yet flashed) |
| V14 | Drive accuracy after calibration: held-out p95 \|ΔM\|, \|ΔC\| ≤ 0.12 D; ≥ 95 % of axes in tolerance | `calibration/capture_session.py`, `fit_calibration.py`; the fit is verified by `--selftest` (recovers known parameters; the negative control is rejected) | Tooling done; **Hardware** |
| V15 | Control loop on real drives: settle time, stall thresholds, `dmin`, backlash approach | `docs/CALIBRATION.md` step 1; host test of the approach-from-below logic | **Hardware** |
| V16 | Real-time behaviour on the ESP32 (500 Hz loop with Wi-Fi active, serial at 460800) | Measure loop period and status jitter on the board | **Hardware** |
| C1 | Agreement with reference refraction (M, J0, J45): bias and 95 % LoA, with CIs; benchmark LoA M ±0.64, J0 ±0.36, J45 ±0.26 D (2025 study) | `validation/clinical_agreement.py`; the analysis is checked against an independent computation | Tooling done; **needs ethics approval, then data** |
| U1 | Optometrists can run a full exam unaided; usability (SUS) | `submission/BASIRA_usability_test_kit.pdf` (5 tasks, observer sheet, SUS); `validation/usability_summary.py` (scoring checked on hand-computed cases) | Kit done; **5 sessions to run** |
| C2 | Repeatability: within-subject SD and 2.77 Sw | Same script (device sessions 1 and 2) | Same as C1 |

**Not covered yet:**

- electrical safety and patient isolation;
- EMC;
- biocompatibility of the parts in contact with the face;
- usability testing with optometrists;
- cybersecurity of the Wi-Fi link (WPA2 password only).

These are needed before any use on people, together with ethics approval (`docs/BASIRA_build_tasklist.md`).
