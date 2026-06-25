# TempControl — Milk-V Duo 256M + ST7789 240×240 Guide

This is a **separate, standalone target** from the Raspberry Pi kiosk in `app/`.
It does **not** use FastAPI, Chromium, names, or the network. The Duo 256M has
only 256 MB RAM and a minimal Buildroot Linux — there is no desktop or browser
to render the Pi's HTML kiosk — so the LCD is drawn natively in pure Python.

Source lives in [`milkv_duo/`](./milkv_duo/).

## What it does

- Always shows the **live temperature** (SHT3x at `0x44`, same sensor as the Pi).
- A **box-breathing** square: a glowing marker traces the perimeter, 4 s per
  edge (16 s cycle), labelling **INHALE → HOLD → EXHALE → HOLD**. This is the
  same BL→TL→TR→BR→BL motion and 4 s/edge timing as the Pi's comet animation.
- **One button** drives the whole device:
  - **single press** → Start / Stop
  - **double press** → restart (fresh run)
- Hard 3-minute safety cap (`CAP_SECONDS`, from CLAUDE.md) auto-finishes a run.

Three screens: **idle** (temp + "PRESS TO START"), **running** (timer + breathing
box + temp), **finished** (`CHALLENGE FINISHED` overlay; single press = OK,
double press = restart).

## Files

| File | Purpose |
|---|---|
| `milkv_duo/tempcontrol_duo.py` | Entry point: state machine, button handling, render loop, `--sim` preview |
| `milkv_duo/display.py` | Pure-stdlib RGB canvas, 5×7 text, 7-segment numerics, breathing box, `/dev/fb0` + PNG-sim backends |
| `milkv_duo/font5x7.py` | Compact 5×7 uppercase bitmap font |
| `milkv_duo/sht3x.py` | SHT3x reader over `/dev/i2c-N` via stdlib ioctl (no smbus2) |
| `milkv_duo/button.py` | sysfs-GPIO button with single/double-press detection |
| `milkv_duo/install/S99tempcontrol` | BusyBox init script for boot autostart |

**Zero pip dependencies** — only the Python 3 standard library, so it runs on the
stock offline image.

## Wiring

The Duo 256M exposes 3V3, GND, and GPIOs on its 26-pin header. Pick pins, then
set them up with `duo-pinmux` (below). Suggested wiring:

**ST7789 240×240 (SPI):**

| LCD pin | Connect to |
|---|---|
| VCC | 3V3 |
| GND | GND |
| SCL/SCK | SPI2 SCK |
| SDA/MOSI | SPI2 MOSI |
| RES | a free GPIO |
| DC | a free GPIO |
| BLK | 3V3 (or a GPIO for backlight control) |
| CS | SPI2 CS |

**SHT3x (I²C, addr `0x44`):** VCC→3V3, GND→GND, SDA→I²C SDA, SCL→I²C SCL.

**Button:** one leg to your chosen GPIO, the other to **GND**. The driver is
active-low and assumes a pull-up (use the internal pull-up or a 10 kΩ to 3V3).

> Use 3V3 for everything — the Duo's GPIO/I²C/SPI logic is 3.3 V.

## Enable the buses (pinmux)

On the Duo, multiplexed pins must be assigned. Check and set with `duo-pinmux`:

```sh
duo-pinmux -p                 # print current pinmux
duo-pinmux -w <PIN>/<FUNC>    # e.g. set a pin to IIC/SPI/GPIO function
```

Then confirm the kernel devices exist:

```sh
ls /dev/i2c-*        # SHT3x bus -> pass its number as --i2c-bus
ls /dev/spidev*      # SPI bus for the panel (if using fbtft)
i2cdetect -y 0       # SHT3x should answer at 0x44 (adjust bus number)
```

## Driving the LCD: kernel framebuffer (recommended)

The app renders to `/dev/fb0`. The cleanest way to put an ST7789 there is the
kernel **fbtft** / **panel-mipi-dbi** driver, configured in the device tree (or
a DT overlay) for the SPI bus and the RES/DC GPIOs you wired. Once loaded you
should see:

```sh
ls /dev/fb0
fbset                 # should report 240x240, 16 bpp (RGB565)
```

Quick sanity check that the panel lights up:

```sh
cat /dev/urandom > /dev/fb0    # should show noise; Ctrl-C to stop
```

If your Duo image doesn't include an ST7789 panel driver, enable `fbtft` /
`fb_st7789v` (or `panel-mipi-dbi`) in the Buildroot kernel config and add the
matching DT node for SPI2 + your RES/DC pins, then rebuild the image. The app
itself needs no changes — it auto-detects `/dev/fb0` and centres the 240×240
image if the panel reports a larger size. 16 bpp and 32 bpp framebuffers are
both supported.

## Find the button's GPIO number

sysfs GPIO numbers are `<gpiochip base> + <line>`. List the chips and bases:

```sh
cat /sys/class/gpio/gpiochip*/base
cat /sys/class/gpio/gpiochip*/label
```

Add the chip base to your pin's line offset to get the number you pass as
`--button-gpio`. Verify by hand:

```sh
echo <N> > /sys/class/gpio/export
echo in > /sys/class/gpio/gpio<N>/direction
cat /sys/class/gpio/gpio<N>/value    # 1 idle, 0 while pressed (active-low)
```

## Run it

```sh
cd ~/TempControl/milkv_duo
python3 tempcontrol_duo.py --i2c-bus 0 --button-gpio <N>
```

Useful flags: `--backend {auto,fb,sim}`, `--button-active-high` (if your button
reads 1 when pressed).

## Autostart at boot

```sh
cp milkv_duo/install/S99tempcontrol /etc/init.d/S99tempcontrol
chmod +x /etc/init.d/S99tempcontrol
# edit APP_DIR / I2C_BUS / BUTTON_GPIO inside the script first
/etc/init.d/S99tempcontrol start
```

BusyBox init runs `/etc/init.d/S*` scripts at boot in order, so `S99…` starts
last. (The Duo image uses BusyBox init, **not** systemd.)

## Preview the UI without hardware

The renderer has a host-side sim backend that writes PNGs — handy for tweaking
layout/colours on a laptop:

```sh
python3 tempcontrol_duo.py --sim --sim-out preview
# -> preview/01_idle.png, 02_run_inhale.png, ... 06_finished.png
```

## Status / caveats

- The **rendering layer is verified** on a host via the PNG sim backend (all
  screens, fonts, 7-segment numerics, and the breathing trace render correctly).
- The **hardware paths** (`/dev/fb0`, `/dev/i2c-N`, sysfs GPIO) are written to
  the standard Linux interfaces but have **not yet been run on a physical Duo**.
  Per the project's hardware-first rule, bring up the panel, sensor, and button
  on the device and verify before considering this "done". The most likely
  things to adjust are the framebuffer's pixel format/stride and the exact
  pinmux/GPIO numbers for your board.
- Palette and the 4 s/edge breathing cadence are kept consistent with the locked
  Pi design.
