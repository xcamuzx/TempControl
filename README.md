# TempControl

Ice-bath–style challenge dashboard for a Raspberry Pi 5 in kiosk mode. Reads temperature + humidity from an I2C sensor (SHT3x at `0x44`) and runs a configurable 1–3 minute countdown with a finish overlay.

See [CLAUDE.md](./CLAUDE.md) for architecture, hardware wiring, and the dev workflow (SSH-into-Pi, then rsync).

## Quick start (local dev host)

```bash
cp .env.example .env   # fill in Pi SSH details
```

The app is intended to run on the Pi, not on a dev host — there's no I2C bus on a regular laptop.

## Milk-V Duo 256M + ST7789 square LCD

A standalone, offline build for a Milk-V Duo 256M driving a 240×240 ST7789
square LCD — no web server, no names, single-button (Start/Stop, double-press to
restart), with a "box breathing" square and the live temperature. It renders
natively to the framebuffer in pure-stdlib Python (the Duo can't run the
Chromium kiosk). See [MILKV_DUO_ST7789.md](./MILKV_DUO_ST7789.md) and
[`milkv_duo/`](./milkv_duo/).

## M5Stack Core2 / ESP32 notes

The Python/FastAPI kiosk app is Linux/Pi software and does not compile directly
as ESP32 firmware. For an M5Stack Core2 + DFRobot SEN0385 sensor wiring guide
and standalone ESP32 sensor probe, see
[ESP32_CORE2_SEN0385.md](./ESP32_CORE2_SEN0385.md).

## Running on the Pi

The dependencies live in a venv at `~/TempControl/.venv`. Don't run `python main.py` — there's no `__main__` block, and you'd hit the system Python which doesn't have the deps.

```bash
ssh <pi>
cd ~/TempControl
./run.sh                                                     # easiest
# or, equivalently:
.venv/bin/uvicorn app.main:app --host 0.0.0.0 --port 8000
```

Then open `http://<pi-ip>:8000/` in a browser (or, in Phase 5, the Pi launches Chromium in kiosk mode pointing at `localhost:8000`).
