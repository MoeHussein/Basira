# Wiring (placeholder pin map, ESP32-S3-DevKitC-1 N8R8)

The pins below are defined in `firmware/basira_fw/config.h`. They are placeholders: wire them this way, or change `config.h` to match your harness.

- GPIO 35–37 are used by the octal PSRAM; don't use them.
- Avoid the strapping pins 0, 3, 45 and 46.
- GPIO 19 and 20 are USB.

| Axis | Element | DRV8833 board / channel | IN1 = PWM | IN2 = DIR | AS5048A CS |
|---|---|---|---|---|---|
| 0 | R Alvarez plate (pinion) | A / 1 | 4 | 5 | 10 |
| 1 | R ring +3.00 | A / 2 | 6 | 7 | 38 |
| 2 | R ring −3.00 | B / 1 | 15 | 16 | 39 |
| 3 | L Alvarez plate (pinion) | B / 2 | 17 | 18 | 40 |
| 4 | L ring +3.00 | C / 1 | 8 | 9 | 41 |
| 5 | L ring −3.00 | C / 2 | 1 | 2 | 42 |

| Signal | GPIO | Notes |
|---|---|---|
| DRV8833 nSLEEP (all three boards) | 14 | HIGH = enabled |
| DRV8833 nFAULT (all three boards, wired together) | 21 | Open drain, internal pull-up; LOW = fault |
| SPI SCK / MISO / MOSI | 12 / 13 / 11 | All six AS5048A share the bus, each with its own CS. SPI mode 1, 4 MHz. |
| Occluder servo R / L | 47 / 48 | 50 Hz, 1000 µs open / 2000 µs closed (`config.h`). GPIO 48 also drives the DevKitC v1.0 RGB LED. |

## Sensors

- **Power:** the AS5048A runs at 3.3 V (VDD3V3 and VDD5V tied to 3.3 V, per the datasheet's 3.3 V operation).
- **Magnet:** use a 6 mm diametric magnet, on axis, at the air gap the AS5048A datasheet specifies (check it for your magnet). The firmware checks the magnet-strength flags (COMP high/low) every 50 reads and faults if the magnet is out of range.
- **Rings:** each ring drives its sensor through a 1:1 spring-loaded sense gear.
- **Plate:** the sensor reads the pinion shaft directly (pinion radius 5 mm, so ±5 mm of travel is ±57° of pinion).

## Motor drivers

- **Supply:** DRV8833 VM is the motor supply (Pololu 298:1 HPCB, 6 V). Logic is 3.3 V.
- **Common ground:** ESP32, drivers and sensors.
- **Current limit:** use a current-limited bench supply; battery and patient isolation are not designed yet.
- **Drive scheme:** one PWM pin per motor (6 LEDC channels) plus two servo channels, which uses all 8 LEDC channels of the ESP32-S3.
  - Forward is driven in fast decay, reverse in slow decay. Speed versus duty differs between directions, so measure `dmin` in **both** directions and use the larger value.
