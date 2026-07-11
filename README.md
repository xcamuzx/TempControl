# TempControl

Ice-bath–style challenge dashboard for a Raspberry Pi 5 in kiosk mode. Reads temperature + humidity from an I2C sensor (SHT3x at `0x44`) and runs a configurable 1–3 minute countdown with a finish overlay.

See [CLAUDE.md](./CLAUDE.md) for architecture, hardware wiring, and the dev workflow (SSH-into-Pi, then rsync).

## Quick start (local dev host)

```bash
cp .env.example .env   # fill in Pi SSH details
```

The app is intended to run on the Pi, not on a dev host — there's no I2C bus on a regular laptop.

## M5Stack Core2 / ESP32 notes

The Python/FastAPI kiosk app is Linux/Pi software and does not compile directly
as ESP32 firmware. For an M5Stack Core2 + DFRobot SEN0385 sensor wiring guide
and standalone ESP32 capture/SD logging firmware, see
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
