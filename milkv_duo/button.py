"""Single push-button on a sysfs GPIO, with single/double-press detection.

The stock Duo image ships BusyBox + the legacy sysfs GPIO interface, so we use
/sys/class/gpio rather than libgpiod (which may be absent). Wire the button
between the GPIO pin and GND and rely on a pull-up (active-low): pressed = 0.

Poll ``update(now)`` every loop; it returns one of None / "single" / "double".
A single-press is reported only after the double-press window closes, so a
double-press never also fires a single.
"""

from __future__ import annotations

import os
import time

DEBOUNCE_S = 0.030
DOUBLE_S = 0.350


class Button:
    def __init__(self, gpio: int, active_low: bool = True) -> None:
        self._gpio = gpio
        self._active_low = active_low
        self._base = f"/sys/class/gpio/gpio{gpio}"
        self._export()
        self._val_path = f"{self._base}/value"

        self._stable = self._raw()        # debounced level (True = pressed)
        self._last_raw = self._stable
        self._last_change = time.time()
        self._last_press_time = 0.0
        self._pending_single = False

    def _export(self) -> None:
        if not os.path.exists(self._base):
            try:
                with open("/sys/class/gpio/export", "w") as f:
                    f.write(str(self._gpio))
            except OSError:
                pass  # may already be exported by another process
        # Wait for udev to create the node, then set as input.
        for _ in range(50):
            if os.path.exists(f"{self._base}/direction"):
                break
            time.sleep(0.01)
        try:
            with open(f"{self._base}/direction", "w") as f:
                f.write("in")
        except OSError:
            pass

    def _raw(self) -> bool:
        try:
            with open(self._val_path) as f:
                level = f.read().strip()
        except OSError:
            return False
        pressed = level == "0"
        return pressed if self._active_low else (level == "1")

    def update(self, now: float) -> str | None:
        raw = self._raw()
        if raw != self._last_raw:
            self._last_raw = raw
            self._last_change = now
        elif (now - self._last_change) >= DEBOUNCE_S and raw != self._stable:
            self._stable = raw
            if raw:  # debounced press (falling edge)
                return self._on_press(now)

        if self._pending_single and (now - self._last_press_time) > DOUBLE_S:
            self._pending_single = False
            return "single"
        return None

    def _on_press(self, now: float) -> str | None:
        if self._pending_single and (now - self._last_press_time) <= DOUBLE_S:
            self._pending_single = False
            return "double"
        self._pending_single = True
        self._last_press_time = now
        return None

    def close(self) -> None:
        pass
