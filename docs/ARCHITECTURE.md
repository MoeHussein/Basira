# BASIRA architecture

```
 Clinician app (browser: tablet or laptop)                  Device (ESP32-S3)
 ┌──────────────────────────────────────┐  protocol v1   ┌───────────────────────────────────────────┐
 │ exam workflow: R/L, SPH/CYL/AXIS,    │  JSON lines    │ protocol.cpp  parse / emit                │
 │ JCC, fog, occlusion, max-plus        │ ◄────────────► │ device.cpp    state machine, faults,      │
 │ chart-distance + vertex conversions  │  USB 460800    │               calibration (flash)         │
 │ exam record (JSON)                   │  or Wi-Fi /ws  │ optics.cpp    Rx → positions → power       │
 │ transports: simulator | USB | Wi-Fi  │                │ axis.cpp      closed-loop drive ×6        │
 └──────────────────────────────────────┘                │ hal_esp32.h   DRV8833 ×3, AS5048A ×6,     │
                                                         │               servos ×2                   │
                                                         └───────────────────────────────────────────┘
```

## Who owns what (single source of truth)

| Information | Owner | Others |
|---|---|---|
| Optics model (math) | `physics/basira_optics.py` | C++ and JS ports, checked against `tests/golden_vectors.json` |
| Element positions, motion, confirmation | device (firmware) | app displays `status`; never computes positions for the hardware |
| Calibration of a headset | device flash (NVS), written by `calib_set` | fitted offline by `calibration/fit_calibration.py` |
| Clinical steps: refinement, chart distance, vertex, BVD/PD, saving | app | the device never sees BVD or PD |
| Exam record | app (downloaded JSON) | |
| Message format | `docs/PROTOCOL.md` | firmware and simulated device implement it |

## Safety rules (blueprint rev 9, section 7)

1. A value is reported only from **sensed** positions, after the drives settle and a 150 ms verification with the motors braked.
2. Any fault stops all motors and marks **both** corrections unknown. Leaving FAULT needs `reset`, and `reset` succeeds only with fresh good sensor reads and no driver fault.
3. Motion needs a live app: no message for 1.5 s during motion gives `LINK`.
4. The app refuses to save an eye unless that eye is CONFIRMED for the last command sent, BVD and PD are entered, and JCC and fog are off.
5. Nominal (unfitted) calibration is allowed only by a build flag. It is always visible: `calibrated:false` in every confirmation, and a banner in the app.

## Control details

- Each drive is a gearmotor with an AS5048A on the driven shaft. Rings use a 1:1 sense gear; the plate sensor sits on the pinion.
- **Control loop:** proportional control at 500 Hz, with a minimum duty to overcome stiction and a settle window.
- **Backlash:** the final approach always comes **from below**. Moving down, the drive overshoots by 2° (rings) or 0.10 mm (plate), then approaches upward, so gear backlash is taken up the same way every time.
- **Stall detection:** the remaining error must shrink by a minimum amount every 300 ms. Near the target, the required progress drops to half the remaining distance.
- **Ring solution:** the two ring angles are found by the law of cosines on the J vectors, so unequal lens powers and fixed J offsets are handled. Of the two solutions, the one with the least ring travel is used.
- **Mismatched pair:** if the cylinder pair is mismatched (|Pp + Pm| > 0), cylinders below that value cannot be reached. The device then gives the smallest possible residual. Use a matched pair (|Pp + Pm| ≤ 0.12 D).

## Not yet decided or built

- Battery and patient isolation. The bench prototype runs from a current-limited bench supply.
- A native tablet app. The browser app covers USB (Chrome/Edge) and Wi-Fi (any browser, including iPad).
- Flashing the app over Wi-Fi, and authentication on the Wi-Fi link. The access point password is set in `config.h`.
