# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project

TempControl is an ice-bath challenge dashboard running on a Raspberry Pi 5 in kiosk mode (HDMI screen, mouse + keyboard).

UX flow (cycles):
1. **Enter names** — operator types 1 to 4 participant names.
2. **Pick countdown** — 1, 2, or 3 minutes (hard cap 3 min).
3. **Single click to start** — countdown runs visibly; live temperature always shown.
4. **Finish alert** — at 0:00, full-screen overlay: `Congratulations - Challenge Finished`. No audio, no browser notifications — overlay only.
5. **Acknowledge click** — dismisses overlay, returns to step 1.

Hard constraints (enforced backend AND frontend): max 4 names, max 3-minute countdown (60–180 s). Temperature readout visible at all times. Single-purpose kiosk — don't add multi-timer, history, auth, or other scope without checking with the user.

## Commands

All commands run **on the Pi** over SSH (see Development Workflow below). The local WSL directory is a staging/commit area only — no I2C bus, no sensor.

```bash
# Run the server (Pi only)
./run.sh
# equivalent to: .venv/bin/uvicorn app.main:app --host 0.0.0.0 --port 8000

# Install deps into Pi venv
.venv/bin/pip install -r requirements.txt

# One-shot sensor probe (Phase 1 diagnostic)
.venv/bin/python scripts/sensor_probe.py

# Verify I2C bus (should show 0x44 sensor + 0x36 UPS)
i2cdetect -y 1
```

There is no `__main__` block in `main.py` — do not run `python main.py`. No test suite or linter is configured.

## Architecture

**Backend:** FastAPI (`app/main.py`) — state machine in a module-level `ChallengeState` dataclass (single-user kiosk, no DB). No background tasks or timers — state transitions (`running→finished`) happen lazily via `tick()` called at the top of every API handler. Sensor driver in `app/sensor.py` using `smbus2` against `/dev/i2c-1`; `read()` retries 3× with incremental backoff (50 ms, 100 ms, 150 ms) before raising `SensorError`. UPS battery driver in `app/ups.py` reads the MAX17040 fuel gauge (same bus, `0x36`) with the same retry pattern.

**Frontend:** Single-file Jinja template (`app/templates/index.html`, ~790 lines) — all HTML, CSS, and JS inlined. No build step, no bundler, no Tailwind. Rough layout: CSS custom properties & styles (~lines 1–330), HTML structure (~330–460), `<script>` with polling + UI logic (~460–790). Vanilla CSS with Google Fonts (Bebas Neue / Fraunces / Italianno / Source Sans 3, fallbacks: Clear Sans + Monotype Corsiva). The frontend polls the backend: temperature every 2s (`/api/temp`), challenge state every 1s (`/api/challenge/state`), battery every 30s (`/api/battery`). Countdown display uses `requestAnimationFrame` against the server-provided `ends_at` timestamp for drift-free rendering.

**Visual effects:** Border-orbit comet animation during countdown (BL → TL → TR → BR → BL, 4s/edge, 16s loop, lead + two trailing halos). Finish overlay with frosted-glass backdrop.

Color palette (locked): `#2e5d74` deep base · `#2f89b9` and `#0f75a8` blue accents · `#ffffff` and `#dddddd` light text · `#6e6f72` mute.

## API surface

| Method | Path | Purpose |
|---|---|---|
| GET | `/` | Jinja-rendered kiosk dashboard |
| GET | `/api/temp` | Latest sensor reading (`temp_c`, `humidity`, `ts`) — 503 on sensor failure |
| GET | `/api/battery` | UPS battery level (`percent`, `voltage`, `ts`) — 503 on read failure |
| GET | `/api/challenge/state` | Current state machine (`idle` / `running` / `finished`); auto-transitions running→finished via lazy `tick()` |
| POST | `/api/challenge/start` | Body `{names: [1..4], duration_seconds: 60..180}`. 422 on bad input, 409 if not idle |
| POST | `/api/challenge/ack` | Dismiss finished overlay, reset to idle. 409 if not finished |
| GET | `/static/...` | Logo + (eventually) QR |

## Deployment target

All code runs on a **Raspberry Pi 5** (aarch64 Linux). The local WSL working directory has no I2C bus and cannot execute sensor code.

## Development workflow

1. **SSH into the Pi 5** and run/test commands directly there. SSH credentials are in `.env` (gitignored); `.env.example` documents the required fields (`IP_ADDRESS`, `USER`, `PASSWORD` — colon-delimited).
2. **Verify on hardware** before considering anything "done" — type checks don't exercise the sensor path.
3. **`rsync` the Pi's working copy back** to the local repo after the change is verified:
   ```bash
   rsync -avz <user>@<pi-ip>:~/TempControl/ /path/to/local/IceSense/ --exclude .venv --exclude __pycache__
   ```
4. Commit from the local copy after rsync.

Do not commit code that has only been verified locally. Do not mock the I2C bus to "test" on WSL.

## Hardware

Both devices share **I2C bus 1** (GPIO2 SDA / GPIO3 SCL). To enable: `sudo raspi-config` → Interface Options → I2C → Enable, then **reboot** (bus 1 only appears after reboot).

**Temperature sensor** — SHT3x (SHT30/31/35), address **`0x44`**. Wired to GPIO header: 3V3 Vcc, GND, GPIO2 SDA, GPIO3 SCL. Driver: `app/sensor.py`.

**UPS** — Geekworm X1200 HAT with MAX17040 fuel gauge, address **`0x36`**. Reads battery state-of-charge (SOC) and cell voltage. The `0x36` device visible in `i2cdetect` is the UPS gauge, not an RP1 phantom. Driver: `app/ups.py`. Do not power the Pi through USB-C while the X1200 is installed.

## Secrets

`.env` holds Pi SSH credentials — never committed. Format is **`KEY:VALUE`** (colon-delimited, not `=`), with a project header on line 1. This is the user's chosen format — do not change to standard dotenv syntax. `.env` is consumed only by SSH/rsync from the local host; the Pi runtime does not read it. No `python-dotenv` dependency.

## Pi runtime layout

Working dir on the Pi: `~/TempControl/`
- `.venv/` — venv with `fastapi`, `uvicorn[standard]`, `jinja2`, `smbus2`
- `app/` — `main.py`, `sensor.py`, `ups.py`, `templates/index.html`, `static/`
- `scripts/sensor_probe.py` — one-shot sensor read for bring-up diagnostics
- `run.sh` — launches uvicorn

Pi user is `fabricio`; passwordless sudo is **not** configured.

## Logo and QR

Logo asset: `app/static/newyicebaths_logo.png`. The header has a QR slot — currently a placeholder div. Once the user provides a real `QR.png`: move to `app/static/QR.png` and replace the placeholder in `index.html` with an `<img>`.

## Phase status

Done: Phase 0 (repo bootstrap) · Phase 1 (Pi I2C setup) · Phase 2 (sensor driver with CRC-8) · Phase 3 (FastAPI + state machine) · Phase 4 (kiosk UI).

Pending:
- Real `QR.png` from user (current root-level file is a duplicate of the logo).
- **Phase 5** — kiosk autostart + service: `systemd` unit for FastAPI, Chromium kiosk autostart via `~/.config/wayfire.ini`, README install steps.

## Pitfalls

- **SSH exit code 255** when backgrounded SSH commands get interrupted (e.g. by `pkill` from another SSH) — that's the SSH-disconnect code, not the remote process failing.
- **Starlette `TemplateResponse` API** wants `request` first: `templates.TemplateResponse(request, "index.html")`. The legacy dict form raises `TypeError: unhashable type: 'dict'`.
- **`/dev/i2c-1` only appears after reboot** following `dtparam=i2c_arm=on`. Pre-reboot only RP1 buses (`/dev/i2c-13`, `/dev/i2c-14`) are visible.
- **Pi DNS may be unconfigured** on corporate LANs even when SSH works. If `pip install` fails, check `/etc/resolv.conf` and outbound 53 reachability.
