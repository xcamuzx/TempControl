# TempControl

Ice-bath–style challenge dashboard for a Raspberry Pi 5 in kiosk mode. Reads temperature + humidity from an I2C sensor (SHT3x at `0x44`) and runs a configurable 1–3 minute countdown with a finish overlay.

See [CLAUDE.md](./CLAUDE.md) for architecture, hardware wiring, and the dev workflow (SSH-into-Pi, then rsync).

## Quick start

```bash
cp .env.example .env   # fill in Pi SSH details
```

The app is intended to run on the Pi, not on a dev host — there's no I2C bus on a regular laptop.
