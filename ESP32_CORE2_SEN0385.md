# M5Stack Core2 + DFRobot SEN0385 Guide

This repository's kiosk dashboard is a Python/FastAPI app for Raspberry Pi Linux.
It uses `uvicorn`, Jinja templates, and `smbus2` against `/dev/i2c-1`, so the
dashboard itself does not compile as ESP32 firmware.

For the M5Stack Core2 target, this repo includes a separate Arduino/PlatformIO
sensor probe at `firmware/core2_sen0385/`. It validates the DFRobot SEN0385
(SHT31-class) temperature/humidity sensor wiring and reads the same 6-byte SHT3x
single-shot measurement frame that the Pi driver uses.

## Sandbox check results

Commands run successfully in the local sandbox:

```bash
python3 -m compileall app scripts
python3 -m uvicorn app.main:app --host 127.0.0.1 --port 8000
```

Verified endpoints:

- `GET /` returned `200`.
- `GET /api/challenge/state` returned `200`.
- `POST /api/challenge/start` returned `200`.
- `GET /api/temp` returned `503` in the sandbox because there is no
  `/dev/i2c-1`.
- `GET /api/battery` returned `503` in the sandbox because there is no
  `/dev/i2c-1`.

Those `503` responses are expected off-hardware.

## Build the Core2 SEN0385 probe

Install PlatformIO, then build the firmware:

```bash
cd firmware/core2_sen0385
python3 -m platformio run -e m5stack-core2
```

Flash and monitor over USB:

```bash
python3 -m platformio run -e m5stack-core2 -t upload
python3 -m platformio device monitor -b 115200
```

The probe prints temperature and humidity to the serial monitor every 2 seconds.

## Core2 GPIO connection guide

Use the M5Stack Core2 external I2C Port A. The Core2 Port A wiring is:

| Core2 Port A wire | Core2 signal | SEN0385 wire |
| --- | --- | --- |
| Black | GND | GND |
| Red | 5V | VCC |
| Yellow | GPIO32 / SDA | SDA |
| White | GPIO33 / SCL | SCL |

Notes:

- Power off the Core2 before wiring.
- SEN0385 accepts `3.3V` to `5V` power.
- Core2 ESP32 GPIO logic is `3.3V`. If using a bare breakout that pulls SDA/SCL
  up to its VCC and does not include level shifting, power the sensor from a
  `3.3V` pin instead of 5V so the I2C lines stay at 3.3V.
- The probe uses `Wire.begin(32, 33, 100000)` for Core2 Port A.
- The probe checks I2C address `0x44` first, then `0x45`. DFRobot documents
  SEN0385 at `0x44`; SHT31 modules may expose `0x44` or `0x45` depending on the
  address configuration.
- Do not use the Core2 internal I2C pins (`GPIO21`/`GPIO22`) for this external
  sensor unless you intentionally wire through the M-BUS. Core2 Port A is
  `GPIO32`/`GPIO33`.

## Porting note

To run the full kiosk workflow on Core2, the frontend/backend must be rewritten
as ESP32 firmware or split into an ESP32 sensor node plus a separate web server.
The existing Python FastAPI dashboard is suitable for Raspberry Pi Linux, not
Arduino, ESP-IDF, or MicroPython on ESP32.
