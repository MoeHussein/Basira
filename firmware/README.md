# BASIRA firmware (ESP32-S3-DevKitC-1)

| File | Role |
|---|---|
| `basira_fw/basira_fw.ino` | Setup and main loop: USB serial, Wi-Fi, 500 Hz control tick, status stream, calibration in flash |
| `basira_fw/config.h` | **Placeholder** pin map, timing and tolerances. Edit this to match your wiring. |
| `basira_fw/optics.*` | Optics model (port of `physics/basira_optics.py`) |
| `basira_fw/axis.*` | One closed-loop drive (motor + AS5048A), hardware-independent |
| `basira_fw/device.*` | State machine, faults, calibration, commands, hardware-independent |
| `basira_fw/protocol.*` | JSON protocol v1 (`docs/PROTOCOL.md`) |
| `basira_fw/hal_esp32.h` | DRV8833, AS5048A (SPI with parity, error-flag and magnet checks) and servos |
| `basira_fw/net_esp32.h` | Wi-Fi access point, HTTP server for the app, WebSocket `/ws` (ESP-IDF `esp_http_server`) |
| `basira_fw/app_bundle.h` | Generated from `app/` by `tools/build_app_bundle.py`. Do not edit. |
| `test_host/` | The same core compiled for a PC and run against simulated drives (`make -C firmware/test_host`) |

## Build and flash

**Arduino IDE 2:**

1. Install the board package "esp32 by Espressif" (3.0.x) and the library "ArduinoJson" (7.x).
2. Open `basira_fw/basira_fw.ino`.
3. Select the board "ESP32S3 Dev Module", PSRAM: "OPI PSRAM", Flash Size: 8 MB.
4. Upload.

**arduino-cli:**

```
arduino-cli compile --fqbn esp32:esp32:esp32s3:PSRAM=opi,FlashSize=8M firmware/basira_fw
arduino-cli upload  --fqbn esp32:esp32:esp32s3:PSRAM=opi,FlashSize=8M -p /dev/ttyUSB0 firmware/basira_fw
```

Built with ESP32 Arduino core 3.0.7: 980 KB flash (74 %), 43 KB RAM. Not yet run on a board.

After changing anything in `app/`, run `python3 tools/build_app_bundle.py` before compiling, so the device serves the current app.

## First power-up

1. **Supply.** Use a current-limited bench supply for the motors, with the drives mechanically free (no lenses fitted yet).
2. **Serial monitor at 460800.** The device prints `info`, then `status` twice a second. All six `ok` flags should be 1; otherwise the device reports `SENSOR` and names the axis.
3. **Bring-up, then calibration:** follow `docs/CALIBRATION.md`.
4. **Wi-Fi.** Join `BASIRA-01` (password in `config.h`, change it) and open `http://192.168.4.1`. Choose "الجهاز عبر Wi-Fi" (device over Wi-Fi) and connect. Over USB, use the app in Chrome or Edge, choose "الجهاز عبر USB" (device over USB) and select the port.

`BASIRA_ALLOW_NOMINAL_CALIB` is 1 for bench work. Set it to 0 before any test on a person, so that only a fitted, validated calibration can confirm.
