# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project

TempControl is an ice-bath–style challenge dashboard running on a Raspberry Pi 5 in kiosk mode on an attached HDMI screen (mouse + keyboard).

UX flow (cycles):
1. **Enter names** — operator types 1 to 4 participant names.
2. **Pick countdown** — anywhere from 1 to 3 minutes (hard cap 3 min).
3. **Single click to start** — countdown runs visibly; live temperature from the I2C sensor is always shown.
4. **Finish alert** — at 0:00, a full-screen visual overlay shows `Congratulations - Challenge Finished`. No audio, no browser notifications — overlay only.
5. **Acknowledge click** — dismisses the alert and returns to step 1.

Hard constraints (validate on backend AND frontend): max 4 names, max 3-minute countdown. The temperature readout is visible at all times, regardless of timer state. The dashboard is intentionally single-purpose — don't add multi-timer, history, auth, or other scope without checking with the user.

Stack:
- Backend: **FastAPI** (Python)
- Frontend: **HTMX + Tailwind** (no build step)
- Sensor: I2C SHT3x at `0x44` via `smbus2` or `adafruit-circuitpython-sht31d`
- Display: **Chromium `--kiosk`** autostarted on the Pi desktop session, pointing at `http://localhost:<port>`
- Process supervision: `systemd` user/system unit for the FastAPI app

Logo asset: `newyicebaths_logo.png` (provided by the user, served as a static file).

Public repo: https://github.com/xcamuzx/TempControl

## Deployment target

All code runs on a **Raspberry Pi 5** (aarch64 Linux). The local working directory on WSL is a staging area only — it has no I2C bus and cannot execute sensor code.

## Development workflow (always)

1. **SSH into the Pi 5** and run/test commands directly there. SSH credentials are in `.env` (gitignored); `.env.example` documents the required fields.
2. **Verify on hardware** before considering anything "done" — type checks and unit tests don't exercise the sensor path.
3. **`rsync` the Pi's working copy back** to `/home/fabricio/TempControl` after the change is verified.
4. Commit from the local copy after rsync.

Do not commit code that has only been verified locally. Do not try to mock the I2C bus to "test" on WSL — that defeats the purpose.

## Hardware

I2C temperature + humidity sensor wired directly to the Pi 5 GPIO header:

| Pin | Signal |
|-----|--------|
| GPIO2 | SDA |
| GPIO3 | SCL |
| 3V3 | Vcc (sensor accepts 3.3–5 V) |
| GND | GND |

Sensor specs (matches SHT3x family — SHT30/31/35):
- I2C address: **`0x44`**
- Vcc: 3.3–5 V, < 1.5 mA
- Humidity: 0–100 %RH, ±2 %RH
- Temperature: −40 °C to +125 °C, ±0.2 °C
- Cable: ~1 m

To enable I2C on the Pi: `sudo raspi-config` → Interface Options → I2C → Enable, then verify with `i2cdetect -y 1` (should show `44`).

## Secrets

`.env` holds Pi SSH credentials for the local dev host (used to SSH and rsync into the Pi) — never committed. `.env.example` is the canonical schema; keep it in sync whenever a new field is introduced.

The `.env` format is **`KEY:VALUE`** (colon-delimited, not `=`), with a project header on line 1:

```
RPI5 - TEMP CONTROL PROJECT
IP_ADDRESS:10.175.168.xxx
USER:xxxx
PASSWORD:xxxxxxx
```

This is the user's chosen format — do not "fix" it to standard dotenv syntax. `.env` is consumed only by SSH/rsync commands from the local host; the Pi runtime does not read it. No `python-dotenv` dependency.
