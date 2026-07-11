# M5Stack Core2 + DFRobot SEN0385 Guide

This repository's kiosk dashboard is a Python/FastAPI app for Raspberry Pi Linux.
It uses `uvicorn`, Jinja templates, and `smbus2` against `/dev/i2c-1`, so the
dashboard itself does not compile as ESP32 firmware.

For the M5Stack Core2 target, this repo includes a separate Arduino/PlatformIO
firmware app at `firmware/core2_sen0385/`. It validates the DFRobot SEN0385
(SHT31-class) temperature/humidity sensor wiring, hosts a browser photo-proof
page over Wi-Fi, saves submitted user login metadata to the Core2 SD card, and
drives a Duinotech WS2812/NeoPixel-style LED ring for the breathing countdown.

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

## Build the Core2 SEN0385 firmware

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

The firmware starts a Wi-Fi access point, hosts the capture page, and prints
sensor/SD status to the serial monitor.

## Core2 browser and SD-card workflow

1. Insert a FAT32-formatted microSD card into the Core2.
2. Flash the firmware.
3. Power the Core2 and connect your phone/laptop to Wi-Fi:
   - SSID: `TempControl-Core2`
   - Password: `tempcontrol`
4. Open `http://192.168.4.1/`.
5. Use `Start 3 Min` to run the LED ring breathing countdown.
6. Enter:
   - `Name`
   - `Week` as `xxx/999`
   - `Badges`
7. Take or choose a picture.
8. Tap `Request GPS` and approve location permission.
9. Tap `Save Picture + Log`.

The browser downloads a PNG photo proof with name, week, badges, temperature,
humidity, date/time, and GPS printed into the image. The ESP32 appends the login
metadata to:

```text
/logins.csv
```

on the Core2 SD card.

Browser note: the photo uses a camera file input for compatibility. GPS still
depends on the browser allowing location permission for the page; if the browser
blocks GPS on local HTTP pages, the image and CSV will record location as
unavailable.

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

## Duinotech circular NeoPixel / WS2812 LED ring

The firmware defaults to the common Duinotech 24-LED circular WS2812 board. If
your ring has a different LED count, change `kNeoPixelCount` in
`firmware/core2_sen0385/src/main.cpp`.

Recommended wiring:

| Core2 / supply | NeoPixel ring pad | Notes |
| --- | --- | --- |
| GPIO27 | DI | Data input. Add a 330-470 ohm series resistor if available. |
| 5V | V+ | Use a separate 5V supply for higher brightness or multiple rings. |
| GND | GND | Must share ground with the Core2. |

Power notes:

- Keep brightness modest when powering the ring from the Core2 5V rail.
- For best reliability with a 5V-powered WS2812 ring, use a 3.3V-to-5V logic
  level shifter on the DI line.
- A capacitor across V+ and GND near the ring is recommended for LED inrush
  current.

LED behavior:

1. `Start 3 Min` flashes three white cue lights.
2. A 3-minute countdown begins. The number of lit LEDs shrinks as time runs out.
3. Breathing phases repeat:
   - light green inhale for 4 seconds
   - amber hold for 4 seconds
   - red exhale for 4 seconds
4. At zero, the whole ring blinks red three times and then turns off.

## Porting note

To run the full kiosk workflow on Core2, the frontend/backend must be rewritten
as ESP32 firmware or split into an ESP32 sensor node plus a separate web server.
The existing Python FastAPI dashboard is suitable for Raspberry Pi Linux, not
Arduino, ESP-IDF, or MicroPython on ESP32.
