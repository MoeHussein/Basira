# BASIRA calibration report

Source: `SYNTHETIC_calibration_readings.csv`

Date: 2026-11-14

Acceptance on >= 20 held-out settings: 95th percentiles of |dM|, |dC| and cylinder-vector error 2|dJ| <= 0.12 D. For |C| >= 0.12 D, >= 95 % of axes must be within asin(0.12 / 2|C|).

**SYNTHETIC DATA - not a device calibration.**

## Eye R: FAIL

| parameter | value | standard error |
|---|---|---|
| P0 | 0.1382 | 0.0094 |
| k | 1.9393 | 0.0020 |
| Pp | 3.0602 | 0.0108 |
| Pm | -2.9592 | 0.0108 |
| zero_a | 4.3268 | 0.1009 |
| zero_b | 177.2882 | 0.1044 |
| J0off | 0.0287 | 0.0054 |
| J45off | -0.0280 | 0.0054 |

Fit: 40 readings, residual RMS 0.034 D.

Held-out: 20 readings: p95 |dM| 0.044 D, p95 |dC| 0.059 D, p95 cylinder-vector error 0.298 D, axes within tolerance: 60 % of 20, max |dM| 0.048 D, mean dM -0.007 D.

## Eye L: FAIL

| parameter | value | standard error |
|---|---|---|
| P0 | -0.0936 | 0.0071 |
| k | 2.0512 | 0.0016 |
| Pp | 2.9681 | 0.0086 |
| Pm | -3.0389 | 0.0085 |
| zero_a | 176.8046 | 0.0828 |
| zero_b | 1.8885 | 0.0798 |
| J0off | -0.0314 | 0.0042 |
| J45off | 0.0299 | 0.0042 |

Fit: 40 readings, residual RMS 0.026 D.

Held-out: 20 readings: p95 |dM| 0.037 D, p95 |dC| 0.034 D, p95 cylinder-vector error 0.220 D, axes within tolerance: 80 % of 20, max |dM| 0.038 D, mean dM +0.002 D.
