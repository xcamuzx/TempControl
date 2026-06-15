# TempControl

Ice-bath–style challenge dashboard for a Raspberry Pi 5 in kiosk mode. Reads temperature + humidity from an I2C sensor (SHT3x at `0x44`) and runs a configurable 1–3 minute countdown with a finish overlay.

See [CLAUDE.md](./CLAUDE.md) for architecture, hardware wiring, and the dev workflow (SSH-into-Pi, then rsync).

## Quick start (local dev host)

```bash
cp .env.example .env   # fill in Pi SSH details
```

The app is intended to run on the Pi, not on a dev host — there's no I2C bus on a regular laptop.

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

## ESP32 / M5Stack Core2 firmware

A separate native port of the kiosk runs on an M5Stack Core2 (ESP32, 320×240
touch) — the FastAPI/Jinja stack can't run on a microcontroller, so the state
machine and SHT3x driver were re-implemented in C++ and the UI rebuilt as native
draw calls. It's standalone (no WiFi): live temperature, a 1/2/3-minute
countdown with a box-breathing pacer, and a finish overlay.

See [`firmware/`](./firmware/) — [README](./firmware/README.md) for build/flash
and wiring, [ARCHITECTURE.md](./firmware/ARCHITECTURE.md) for the design.
