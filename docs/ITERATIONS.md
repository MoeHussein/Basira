# Evaluation and improvement log

Each entry is something we evaluated, what the evidence showed, what we changed, and the test that now guards it.

| # | Evaluated | Finding (evidence) | Change | Guarded by |
|---|---|---|---|---|
| 1 | Pitch deck, source audit | WHO "36 %" figure absent from the current fact sheet; Topcon VT-10 weighs 4.5 kg, not 5 kg; a citation misattributed; novelty overstated (VisionFit SC is a commercial wearable refractor) | Claims corrected, 17 references with DOIs, honest novelty statement | Reference list in the deck |
| 2 | Simulator, frame-by-frame video check | A cylinder added at the default axis 90° ended the exam at ×85° instead of the patient's ×175° | Default axis 180°, demo corrected | Browser test checks the saved Rx |
| 3 | Reference optics model | Rx round-trip test: power vector → Rx returned an axis 90° off | Axis formula fixed | `physics/test_basira_optics.py` (5,000 random Rx) |
| 4 | Optics with real lenses | A mismatched ± cylinder pair cannot produce cylinders below \|Pp + Pm\| | Model returns the minimum residual; lens-matching rule (≤ 0.12 D) added | Python test; `docs/CALIBRATION.md` step 2 |
| 5 | Firmware fault handling | Host test: `reset` re-armed a dead sensor (one bad read is below the error threshold) | `reset` now requires a fresh good read on every sensor | `firmware/test_host` |
| 6 | Firmware motion control | Code review: stall detection could fire during a slow final approach | Required progress capped at half the remaining distance | Host tests (exam flow, backlash approach) |
| 7 | Firmware link | Status stream at 115200 baud would fill the serial buffer and block the control loop | 460800 baud | Host test: status size vs bandwidth budget |
| 8 | Firmware dependencies | A third-party WebSocket library was not needed | ESP-IDF built-in HTTP/WebSocket server; one external library left (ArduinoJson) | ESP32 compile |
| 9 | Clinical correctness (6 m chart) | The demo ended at the true sphere, which the patient only sees clearly by accommodating 0.17 D: the saved Rx would be 0.25 D too minus | Max-plus endpoint in the demo; distance Rx = measured − 1/L; simulated patient accommodates | Browser test: saved Rx equals the true Rx (−2.25 / −0.75 × 175) |
| 10 | Calibration acceptance | A single vector-error metric failed correct fits: 1° of lensmeter axis noise on 5 D cylinder is 0.17 D | Separate criteria: sphere-equivalent and cylinder ≤ 0.12 D; axis tolerance asin(0.12 / 2\|C\|) with the instrument's resolution as floor | `fit_calibration.py --selftest` with a negative control |
| 11 | Reviewer audit | Jog and occluder commands could run after STOP. The fitter mishandled encoder wrap and the 2° floor allowed excessive axis error at high cylinder power. | Latch the actuator interlock through reset, wrap encoder differences and apply the 0.12 D combined cylinder-vector limit with a cylinder-dependent axis limit. | Firmware host and simulator tests, calibration self-test and regenerated synthetic reports. |
| 11 | Usability with optometrists | Not yet evaluated | Test kit ready: `submission/BASIRA_usability_test_kit.pdf`, `validation/usability_summary.py` | Pending: 5 sessions, then fix the top 1-2 issues and re-test |
