# BASIRA protocol v1

One JSON object per line (UTF-8, newline-terminated, at most 511 bytes). The messages are the same over both links:

| Link | Settings |
|---|---|
| USB serial | 460800 baud, 8N1. Every line the device prints is a protocol message. |
| Wi-Fi | The device is an access point (`BASIRA-01`). It serves the app at `http://192.168.4.1/` and a WebSocket at `ws://192.168.4.1/ws`. |

Implementations: `firmware/basira_fw/protocol.cpp` (device) and `app/js/sim_device.js` (simulated device). The app (`app/js/app.js`) is the client.

Conventions:

- **Eyes:** `"R"` and `"L"`.
- **Prescriptions:** minus cylinder (`C <= 0`); axis in degrees, TABO.
- **Positions:** `[delta_mm, alpha_deg, beta_deg]`, where alpha is the plus-cylinder ring and beta the minus-cylinder ring.
- **Power vectors:** `[M, J0, J45]` in dioptres.
- **Request ids:** `id` is chosen by the app and echoed in `ack`, `err` and `confirmed`.

## App to device

| `t` | Fields | Effect |
|---|---|---|
| `hello` | | Device replies with `info` and `status`. |
| `ping` | | Keep-alive. Send every 500 ms. If nothing arrives for 1.5 s during motion, the device faults with `LINK`. |
| `status` | | Device replies with `status`. |
| `set` | `id, eye, S, C, ax` | Device computes element positions with its calibration model, moves, verifies, then sends `confirmed`. Accepted only in `READY` or `CONFIRMED`. |
| `set_pos` | `id, eye, delta, alpha, beta` | Moves to positions directly; for calibration capture. |
| `occlude` | `id, eye, on` | Closes (`true`) or opens the occluder of that eye. Refused in `FAULT`. |
| `stop` | `id` | Commands the six lens drives to brake, cancels any active jog and enters `FAULT` (`USER_STOP`). The occluders retain their last commanded positions. |
| `reset` | `id` | Leaves `FAULT` once every sensor gives a fresh good read and the motor drivers report no fault. The correction is then unknown until the next `set`. |
| `jog` | `id, axis (0-5), duty (-1..1), ms (<= 2000)` | Open-loop drive for bring-up. Accepted in `READY` or `CONFIRMED` with no active jog. Axis order: R plate, R ring+, R ring-, L plate, L ring+, L ring-. |
| `calib_get` | `eye` (optional) | Device replies with `calib` for one eye or both. |
| `calib_set` | `id, eye, cal{P0,k,Pp,Pm,J0off,J45off,delta_max,model_id}, mech{zero_deg[3],sign[3],motor_dir[3],dmin[3],pinion_r_mm}, fitted` | Applies the values and saves them to flash. Missing fields keep their current values. |
| `sim_fault` | `id, kind: sensor, stall, link or clear` | Simulated device only: injects a fault. |

## Device to app

| `t` | Fields | When |
|---|---|---|
| `info` | `fw, proto, allow_nominal, eyes{R,L}{model, fitted}` | After `hello`, at boot, and when a Wi-Fi client connects. |
| `status` | `st, eye, fault?, R{pos[3], raw[3], ok[3], occ, conf}, L{...}` | 20 Hz while moving or verifying, 2 Hz otherwise, and on every state change. |
| `ack` | `id` | A command was accepted. |
| `err` | `id, code, msg` | A command was refused; nothing moved. |
| `confirmed` | `id, eye, pv[3], rx[3], pos[3], calibrated, model` | After verification. The power is computed from the averaged **sensed** positions, not from the command. |
| `fault` | `code, axis, msg` | A fault commanded the lens drives to brake. Both eyes' corrections become unknown. |
| `calib` | `eye, cal{...}, mech{...}, fitted` | Reply to `calib_get` and `calib_set`. |

**States.** The device starts in `PARKED` until every sensor has given a reading, then moves through `READY`, `MOVING`, `VERIFYING` and `CONFIRMED`. Any fault puts it in `FAULT`.

- **CONFIRMED requires** the three drives of the eye to settle, then 150 ms with the motors braked. The averaged sensed positions must be within 0.6° (rings) and 30 µm (plate) of target.
- **While CONFIRMED,** movement beyond 1.0° or 50 µm gives `DRIFT`.

**Error codes (`err`):**

| Code | Meaning |
|---|---|
| `BAD_JSON` | The message is not valid JSON. |
| `UNKNOWN` | The message type is not recognised. |
| `BAD_ARG` | A field is missing or out of range. |
| `BAD_RX` | The cylinder is positive. Send minus cylinder. |
| `BUSY` | A move is in progress. |
| `FAULT` | The device is in FAULT; send `reset` first. |
| `NOT_READY` | The sensors have not been read yet. |
| `UNCALIBRATED` | No fitted calibration, and nominal calibration is not allowed. |
| `RANGE_SPHERE` | The spherical equivalent is outside the Alvarez range. |
| `RANGE_CYL` | The cylinder is larger than the ring pair can make. |
| `RANGE` | A target is outside a drive's travel. |
| `SENSOR` | A sensor still does not read (reply to `reset`). |
| `DRIVER` | A motor driver still reports a fault (reply to `reset`). |

**Fault codes (`fault`):**

| Code | Meaning |
|---|---|
| `SENSOR` | A read failed 5 times in a row: parity, error flag, or magnet out of range. |
| `STALL` | The error is not shrinking while the motor is driven (300 ms window). |
| `TIMEOUT` | The move took longer than 4 s. |
| `DRIVER` | A DRV8833 nFAULT line is asserted. |
| `LINK` | The app went silent or disconnected during motion. |
| `USER_STOP` | The operator pressed stop. |
| `TOLERANCE` | After settling, a position is outside the verify tolerance. |
| `DRIFT` | An element moved after confirmation. |

**Nominal calibration.** When `allow_nominal` is true (a firmware build flag for bench work), the device confirms using the nominal model. In that case `confirmed.calibrated` is `false`, and the app shows a NOT CALIBRATED banner. Build with `BASIRA_ALLOW_NOMINAL_CALIB 0` before any test on a person.

**Example**

```
→ {"t":"set","id":7,"eye":"R","S":-2.25,"C":-0.75,"ax":175}
← {"t":"ack","id":7}
← {"t":"status","st":"MOVING","eye":"R",...}
← {"t":"status","st":"VERIFYING",...}
← {"t":"status","st":"CONFIRMED",...}
← {"t":"confirmed","id":7,"eye":"R","pv":[-2.6256,0.3693,-0.0651],"rx":[-2.2506,-0.75,175.0],"pos":[-1.3128,43.59,36.41],"calibrated":false,"model":"NOMINAL"}
```
