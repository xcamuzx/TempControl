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
- Backend: **FastAPI** (Python) — `app/main.py`
- Frontend: hand-tuned HTML/CSS in `app/templates/index.html` (Google Fonts: Bebas Neue / Fraunces / Italianno / Source Sans 3 with Clear Sans + Monotype Corsiva fallbacks). Tailwind not used in the end — vanilla CSS gave more control over the editorial layout.
- Sensor driver: `app/sensor.py` using `smbus2` against `/dev/i2c-1`
- Display: **Chromium `--kiosk`** autostarted on the Pi desktop session, pointing at `http://localhost:8000`
- Process supervision: `systemd` unit (Phase 5, not yet built)

Color palette (locked): `#2e5d74` deep base · `#2f89b9` and `#0f75a8` blue accents · `#ffffff` and `#dddddd` light text · `#6e6f72` mute. Subtitles use `#dddddd` for contrast (mid-blue subtitles disappeared into the background).

Logo asset: `app/static/newyicebaths_logo.png`. The header also has a QR slot beside the logo — currently a placeholder div until the user supplies the real `QR.png` (the file at repo root is a duplicate of the logo).

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

## Pi runtime layout

Working dir on the Pi is `~/TempControl/`, mirroring the local repo:

- `.venv/` — venv with `fastapi`, `uvicorn[standard]`, `jinja2`, `smbus2` (gitignored)
- `app/` — `main.py`, `sensor.py`, `templates/index.html`, `static/`
- `scripts/sensor_probe.py` — one-shot sensor read used during Phase 1 bring-up
- `run.sh` — `exec .venv/bin/uvicorn app.main:app --host 0.0.0.0 --port 8000`

Launch the server with `./run.sh`. **Do not** run `python main.py` — there's no `__main__` block, and the system Python doesn't have the deps. The Pi user is `fabricio`; passwordless sudo is **not** configured (use `echo "$PASSWORD" | sudo -S …` from the local host when needed).

## API surface

| Method | Path | Purpose |
|---|---|---|
| GET | `/` | Jinja-rendered kiosk dashboard |
| GET | `/api/temp` | Latest sensor reading (`temp_c`, `humidity`, `ts`) — 503 on sensor failure |
| GET | `/api/challenge/state` | Current state machine (`idle` / `running` / `finished`); auto-transitions running→finished on lazy `tick()` |
| POST | `/api/challenge/start` | Body `{names: [1..4], duration_seconds: 60..180}`. 422 on bad input, 409 if not idle |
| POST | `/api/challenge/ack` | Dismiss finished overlay, reset to idle. 409 if not finished |
| GET | `/static/...` | Logo + (eventually) QR |

State is held in a module-level `ChallengeState` dataclass — single-user kiosk, no DB.

## Phase status

Done:
- **Phase 0** — repo bootstrap, `.env.example`, `.gitignore`, GitHub remote (SSH key auth).
- **Phase 1** — Pi prep: I2C bus 1 enabled (required reboot after `dtparam=i2c_arm=on`), `i2c-tools` installed, sensor confirmed at `0x44`. An unrelated device shows up at `0x36` (likely RP1 onboard) — ignore. Probe script `scripts/sensor_probe.py` works.
- **Phase 2** — `app/sensor.py` driver with CRC-8 verification and configurable retries.
- **Phase 3** — FastAPI backend + state machine, all endpoints validated end-to-end on the Pi (including the 60s expiry path).
- **Phase 4** — Kiosk UI: editorial cold-plunge layout, big Bebas Neue temperature, three click-to-start timer cards, finish overlay, border-orbit comet effect during countdown (BL → TL → TR → BR → BL, 4s/edge, 16s loop, with two trailing halos).

Pending:
- Real `QR.png` from the user (current root-level file is a duplicate of the logo). Once provided: move to `app/static/QR.png` and replace the placeholder div in `index.html` with an `<img>`.
- **Phase 5** — kiosk autostart + service:
  - `systemd` unit `tempcontrol.service` for the FastAPI process (Restart=on-failure, depends on the I2C device)
  - Chromium kiosk autostart on Pi desktop login (`~/.config/wayfire.ini` autostart entry, `chromium-browser --kiosk --noerrdialogs --disable-infobars http://localhost:8000`)
  - README install steps

## Pitfalls observed

- **Backgrounded SSH commands return exit code 255 when the SSH session is later interrupted** (e.g. by a `pkill` from another SSH). That's the SSH-disconnect code, not the remote process failing. uvicorn ran fine; only the foreground SSH transport died.
- **Newer Starlette `TemplateResponse` API** wants `request` first: `templates.TemplateResponse(request, "index.html")`. The legacy `("index.html", {"request": request})` form raises `TypeError: unhashable type: 'dict'` (the dict gets used as the cache key).
- **`/dev/i2c-1` only appears after a reboot** following `dtparam=i2c_arm=on` (or `raspi-config nonint do_i2c 0`). Pre-reboot only the internal RP1 buses (`/dev/i2c-13`, `/dev/i2c-14`) are visible.
- **Pi DNS can be unconfigured on this LAN** even when LAN connectivity (SSH) works. If `pip install` fails, check `/etc/resolv.conf` and outbound 53 reachability — corporate networks may strip both.
