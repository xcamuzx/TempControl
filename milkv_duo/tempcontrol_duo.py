#!/usr/bin/env python3
"""TempControl -- Milk-V Duo 256M + ST7789 240x240 square LCD edition.

Standalone, offline, single-button ice-bath companion. Always shows the live
temperature; a "box breathing" marker traces a square 4s per edge
(INHALE / HOLD / EXHALE / HOLD). One GPIO button drives everything:

    single press  -> Start / Stop          double press -> restart (fresh run)

No web server, no names, no network (cf. the Raspberry Pi build in app/).

Run on the Duo:
    python3 tempcontrol_duo.py --i2c-bus 0 --button-gpio <N>

Preview the UI on a host (writes PNGs, no hardware needed):
    python3 tempcontrol_duo.py --sim --sim-out /tmp/preview
"""

from __future__ import annotations

import argparse
import os
import time

import display as d
from display import Canvas

# --- tunables -------------------------------------------------------------
CAP_SECONDS = 180          # hard safety cap (CLAUDE.md: max 3-minute challenge)
EDGE_SECONDS = 4.0         # box-breathing pace: 4s per side -> 16s cycle
SENSOR_PERIOD_S = 1.0      # how often to poll the SHT3x
TARGET_FPS = 15

# Square geometry for the breathing box (top-left x/y and side length).
SQ_X, SQ_Y, SQ_S = 45, 56, 150


def fmt_clock(seconds: float) -> str:
    s = int(seconds)
    return f"{s // 60}:{s % 60:02d}"


def breathing(elapsed: float) -> tuple[int, int, str, float]:
    """Marker (x, y), phase label, and edge progress for box breathing."""
    cycle = EDGE_SECONDS * 4
    p = (elapsed % cycle) / EDGE_SECONDS
    e = int(p) % 4
    f = p - int(p)
    x, y, s = SQ_X, SQ_Y, SQ_S
    if e == 0:        # left edge, bottom -> top
        px, py, label = x, y + s - f * s, "INHALE"
    elif e == 1:      # top edge, left -> right
        px, py, label = x + f * s, y, "HOLD"
    elif e == 2:      # right edge, top -> bottom
        px, py, label = x + s, y + f * s, "EXHALE"
    else:             # bottom edge, right -> left
        px, py, label = x + s - f * s, y + s, "HOLD"
    return int(px), int(py), label, f


# --------------------------------------------------------------------------
# Screens
# --------------------------------------------------------------------------
def draw_idle(c: Canvas, temp: float | None, hum: float | None) -> None:
    c.fill(d.BG)
    c.text_centered("TEMPCONTROL", c.w // 2, 18, 2, d.LIGHT2)
    c.rect_outline(SQ_X, SQ_Y, SQ_S, SQ_S, 2, d.ACCENT2)
    _draw_temp_block(c, temp, hum, cy=SQ_Y + SQ_S // 2)
    c.text_centered("PRESS TO START", c.w // 2, 214, 2, d.ACCENT)


def draw_running(c: Canvas, elapsed: float, temp: float | None,
                 hum: float | None) -> None:
    c.fill(d.BG)
    # elapsed timer up top
    c.seg_text_centered(fmt_clock(elapsed), c.w // 2, 14, 30, d.LIGHT)
    # breathing square + comet
    c.rect_outline(SQ_X, SQ_Y, SQ_S, SQ_S, 2, d.ACCENT2)
    px, py, label, _ = breathing(elapsed)
    # short comet tail (samples slightly behind the head)
    for k, a in ((0.18, 0.25), (0.10, 0.45)):
        tx, ty, _, _ = breathing(elapsed - k)
        c.fill_circle(tx, ty, 4, d.ACCENT, a)
    c.glow_dot(px, py, d.ACCENT, core=4)
    # phase label + temperature inside the square
    c.text_centered(label, c.w // 2, SQ_Y + 34, 3, d.LIGHT)
    _draw_temp_block(c, temp, hum, cy=SQ_Y + SQ_S - 34, accent=True)
    c.text_centered("PRESS TO STOP", c.w // 2, 218, 2, d.MUTE)


def draw_finished(c: Canvas, elapsed: float, temp: float | None) -> None:
    c.fill(d.BASE)
    # frosted panel
    c.fill_rect(18, 44, c.w - 36, c.h - 88, d.BG)
    c.rect_outline(18, 44, c.w - 36, c.h - 88, 2, d.ACCENT2)
    c.text_centered("CHALLENGE", c.w // 2, 66, 3, d.LIGHT)
    c.text_centered("FINISHED", c.w // 2, 100, 3, d.LIGHT)
    c.seg_text_centered(fmt_clock(elapsed), c.w // 2, 134, 30, d.ACCENT)
    if temp is not None:
        c.text_centered(f"{temp:.1f}", c.w // 2 - 8, 176, 2, d.LIGHT2)
        c.text(chr(0xB0) + "C", c.w // 2 + c.text_w(f"{temp:.1f}", 2) // 2 - 4, 176, 2, d.LIGHT2)
    c.text_centered("PRESS OK   DBL RESTART", c.w // 2, 206, 1, d.MUTE)


def _draw_temp_block(c: Canvas, temp: float | None, hum: float | None,
                     cy: int, accent: bool = False) -> None:
    color = d.ACCENT if accent else d.LIGHT
    if temp is None:
        c.text_centered("SENSOR ERR", c.w // 2, cy - 8, 2, d.MUTE)
        return
    s = f"{temp:.1f}"
    th = 46 if not accent else 38
    total = c.seg_text_w(s, th) + 22
    x0 = c.w // 2 - total // 2
    end = c.seg_text(s, x0, cy - th // 2, th, color)
    # degree + C in the small font, baseline-aligned near the top of the digits
    c.text(chr(0xB0) + "C", end + 6, cy - th // 2, 2, color)
    if hum is not None and not accent:
        c.text_centered(f"HUM {hum:.0f}%", c.w // 2, cy + th // 2 + 8, 1, d.MUTE)


# --------------------------------------------------------------------------
# State machine
# --------------------------------------------------------------------------
class App:
    def __init__(self) -> None:
        self.status = "idle"          # idle | running | finished
        self.started_at = 0.0
        self.frozen_elapsed = 0.0

    def elapsed(self, now: float) -> float:
        if self.status == "running":
            return min(CAP_SECONDS, now - self.started_at)
        return self.frozen_elapsed

    def tick(self, now: float) -> None:
        if self.status == "running" and (now - self.started_at) >= CAP_SECONDS:
            self.frozen_elapsed = CAP_SECONDS
            self.status = "finished"

    def on_single(self, now: float) -> None:
        if self.status == "idle":
            self._start(now)
        elif self.status == "running":
            self.frozen_elapsed = self.elapsed(now)
            self.status = "finished"
        else:  # finished -> acknowledge
            self.status = "idle"

    def on_double(self, now: float) -> None:
        self._start(now)              # restart, regardless of current state

    def _start(self, now: float) -> None:
        self.started_at = now
        self.frozen_elapsed = 0.0
        self.status = "running"

    def render(self, c: Canvas, now: float, temp, hum) -> None:
        if self.status == "idle":
            draw_idle(c, temp, hum)
        elif self.status == "running":
            draw_running(c, self.elapsed(now), temp, hum)
        else:
            draw_finished(c, self.frozen_elapsed, temp)


# --------------------------------------------------------------------------
# Runtime
# --------------------------------------------------------------------------
def run_live(args) -> None:
    from sht3x import Sensor, SensorError
    from button import Button

    sensor = Sensor(bus_num=args.i2c_bus)
    button = Button(args.button_gpio, active_low=not args.button_active_high)
    disp = d.open_display(prefer=args.backend)
    canvas = Canvas()
    app = App()

    temp = hum = None
    last_sensor = 0.0
    frame_dt = 1.0 / TARGET_FPS
    try:
        while True:
            now = time.time()
            ev = button.update(now)
            if ev == "single":
                app.on_single(now)
            elif ev == "double":
                app.on_double(now)
            app.tick(now)

            if now - last_sensor >= SENSOR_PERIOD_S:
                last_sensor = now
                try:
                    r = sensor.read()
                    temp, hum = r.temp_c, r.humidity
                except SensorError:
                    temp = hum = None

            app.render(canvas, now, temp, hum)
            disp.flush(canvas)

            time.sleep(max(0.0, frame_dt - (time.time() - now)))
    except KeyboardInterrupt:
        pass
    finally:
        disp.close()
        button.close()


def run_sim(args) -> None:
    """Render representative frames to PNGs for host-side preview."""
    from sht3x import FakeSensor

    os.makedirs(args.sim_out, exist_ok=True)
    sensor = FakeSensor()
    c = Canvas()
    app = App()
    t0 = time.time()

    def snap(name: str) -> None:
        r = sensor.read()
        app.render(c, time.time(), r.temp_c, r.humidity)
        d.write_png(os.path.join(args.sim_out, name), c)
        print("wrote", os.path.join(args.sim_out, name))

    snap("01_idle.png")
    app.on_single(time.time())                 # start
    # capture each breathing phase by overriding started_at
    for name, off in (("02_run_inhale.png", 1.0), ("03_run_hold.png", 6.0),
                      ("04_run_exhale.png", 9.0), ("05_run_midcycle.png", 13.0)):
        app.started_at = time.time() - off
        snap(name)
    app.frozen_elapsed = 132.0
    app.status = "finished"
    snap("06_finished.png")
    print(f"sim done in {time.time() - t0:.2f}s")


def main() -> None:
    ap = argparse.ArgumentParser(description="TempControl Duo ST7789 edition")
    ap.add_argument("--backend", default="auto", choices=["auto", "fb", "sim"])
    ap.add_argument("--i2c-bus", type=int, default=0, help="/dev/i2c-N for the SHT3x")
    ap.add_argument("--button-gpio", type=int, default=0, help="sysfs GPIO number")
    ap.add_argument("--button-active-high", action="store_true",
                    help="button reads 1 when pressed (default: active-low)")
    ap.add_argument("--sim", action="store_true", help="render preview PNGs and exit")
    ap.add_argument("--sim-out", default="preview", help="output dir for --sim")
    args = ap.parse_args()

    if args.sim or args.backend == "sim":
        run_sim(args)
    else:
        run_live(args)


if __name__ == "__main__":
    main()
