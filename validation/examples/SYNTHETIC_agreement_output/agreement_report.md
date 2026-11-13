# BASIRA clinical agreement report

Data: `SYNTHETIC_clinical_pairs.csv`, eye: R

**SYNTHETIC DATA - illustrates the analysis only; these are not results.**

## Agreement: device (session 1) - reference

| component | n | bias (95 % CI) | 95 % LoA | LoA 95 % CI | benchmark LoA |
|---|---|---|---|---|---|
| M | 40 | +0.03 (-0.05, +0.11) D | -0.45 to +0.50 D | low -0.58..-0.31, high +0.37..+0.64 | +/-0.64 D |
| J0 | 40 | +0.04 (+0.00, +0.07) D | -0.18 to +0.25 D | low -0.24..-0.12, high +0.19..+0.32 | +/-0.36 D |
| J45 | 40 | +0.00 (-0.03, +0.03) D | -0.19 to +0.19 D | low -0.24..-0.14, high +0.14..+0.24 | +/-0.26 D |

Within +/-0.25 D (M): 85 %; within +/-0.50 D: 98 %.

## Repeatability: device session 2 - session 1

| component | n | mean difference | Sw | repeatability (2.77 Sw) |
|---|---|---|---|---|
| M | 40 | +0.037 D | 0.244 D | 0.67 D |
| J0 | 40 | -0.049 D | 0.144 D | 0.40 D |
| J45 | 40 | -0.022 D | 0.083 D | 0.23 D |

Reading the table: a bias whose CI excludes 0 is a systematic offset (check calibration and the chart-distance correction); limits of agreement are judged against the clinical need, with the 2025 benchmark as context, and the CI of each limit shows how much the sample size limits that judgement.
