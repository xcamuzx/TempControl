"""Pure-stdlib rendering + output backends for the Duo ST7789 240x240 LCD.

No Pillow, numpy or pygame: the stock Milk-V Duo image has none of them and the
device is offline, so everything here uses only the standard library. The Canvas
holds a flat RGB buffer (3 bytes/pixel) which makes alpha blending and PNG
export trivial; it is converted to RGB565 only when flushed to real hardware.

Two output backends, auto-selected by ``open_display()``:
  * FbDisplay  -> /dev/fb0   (the ST7789 driven as a kernel framebuffer via the
                  fbtft / panel-mipi-dbi driver -- see MILKV_DUO_ST7789.md)
  * SimDisplay -> writes PNG frames (host-side preview, no hardware)

Driving the panel through the kernel framebuffer keeps userspace dependency-free
(only mmap + ioctl). A direct-SPI backend is intentionally omitted because the
stock image may lack python3-spidev and the device is offline.
"""

from __future__ import annotations

import os
import struct
import zlib

from font5x7 import GLYPH_H, GLYPH_W, glyph

WIDTH = 240
HEIGHT = 240

# Locked TempControl palette (see CLAUDE.md).
BASE = (0x2E, 0x5D, 0x74)      # deep base
ACCENT = (0x2F, 0x89, 0xB9)    # bright blue accent
ACCENT2 = (0x0F, 0x75, 0xA8)   # deep blue accent
LIGHT = (0xFF, 0xFF, 0xFF)     # primary light text
LIGHT2 = (0xDD, 0xDD, 0xDD)    # secondary light text
MUTE = (0x6E, 0x6F, 0x72)      # muted
BG = (0x12, 0x26, 0x30)        # darker-than-base screen background


class Canvas:
    """A WIDTH x HEIGHT RGB888 software framebuffer with drawing primitives."""

    def __init__(self, w: int = WIDTH, h: int = HEIGHT) -> None:
        self.w = w
        self.h = h
        self.buf = bytearray(w * h * 3)

    def fill(self, color: tuple[int, int, int]) -> None:
        r, g, b = color
        self.buf[:] = bytes((r, g, b)) * (self.w * self.h)

    def set_px(self, x: int, y: int, color: tuple[int, int, int]) -> None:
        if 0 <= x < self.w and 0 <= y < self.h:
            i = (y * self.w + x) * 3
            self.buf[i] = color[0]
            self.buf[i + 1] = color[1]
            self.buf[i + 2] = color[2]

    def blend_px(self, x: int, y: int, color: tuple[int, int, int], a: float) -> None:
        """Alpha-blend ``color`` over the existing pixel (a in 0..1)."""
        if a <= 0 or not (0 <= x < self.w and 0 <= y < self.h):
            return
        if a >= 1:
            self.set_px(x, y, color)
            return
        i = (y * self.w + x) * 3
        ia = 1.0 - a
        self.buf[i] = int(color[0] * a + self.buf[i] * ia)
        self.buf[i + 1] = int(color[1] * a + self.buf[i + 1] * ia)
        self.buf[i + 2] = int(color[2] * a + self.buf[i + 2] * ia)

    def fill_rect(self, x: int, y: int, w: int, h: int, color: tuple[int, int, int]) -> None:
        x0, y0 = max(0, x), max(0, y)
        x1, y1 = min(self.w, x + w), min(self.h, y + h)
        if x1 <= x0 or y1 <= y0:
            return
        row = bytes(color) * (x1 - x0)
        for yy in range(y0, y1):
            i = (yy * self.w + x0) * 3
            self.buf[i:i + len(row)] = row

    def rect_outline(self, x: int, y: int, w: int, h: int, t: int,
                     color: tuple[int, int, int]) -> None:
        self.fill_rect(x, y, w, t, color)
        self.fill_rect(x, y + h - t, w, t, color)
        self.fill_rect(x, y, t, h, color)
        self.fill_rect(x + w - t, y, t, h, color)

    def fill_circle(self, cx: int, cy: int, r: int,
                    color: tuple[int, int, int], a: float = 1.0) -> None:
        r2 = r * r
        for dy in range(-r, r + 1):
            for dx in range(-r, r + 1):
                if dx * dx + dy * dy <= r2:
                    self.blend_px(cx + dx, cy + dy, color, a)

    def glow_dot(self, cx: int, cy: int, color: tuple[int, int, int],
                 core: int = 4) -> None:
        """A bright core with two soft halos -- the comet head."""
        self.fill_circle(cx, cy, core + 6, color, 0.10)
        self.fill_circle(cx, cy, core + 3, color, 0.22)
        self.fill_circle(cx, cy, core, color, 1.0)

    # --- text -------------------------------------------------------------
    def text(self, s: str, x: int, y: int, scale: int,
             color: tuple[int, int, int], spacing: int = 1) -> int:
        """Draw 5x7 text; returns the x past the last glyph."""
        cx = x
        for ch in s:
            rows = glyph(ch)
            for ry, row in enumerate(rows):
                for rx, c in enumerate(row):
                    if c == "#":
                        self.fill_rect(cx + rx * scale, y + ry * scale, scale, scale, color)
            cx += (GLYPH_W + spacing) * scale
        return cx

    def text_w(self, s: str, scale: int, spacing: int = 1) -> int:
        return len(s) * (GLYPH_W + spacing) * scale - spacing * scale

    def text_centered(self, s: str, cx: int, y: int, scale: int,
                      color: tuple[int, int, int], spacing: int = 1) -> None:
        self.text(s, cx - self.text_w(s, scale, spacing) // 2, y, scale, color, spacing)

    # --- 7-segment numerics (big temperature + timer) ---------------------
    _SEG = {
        "0": "abcdef", "1": "bc", "2": "abged", "3": "abgcd", "4": "fgbc",
        "5": "afgcd", "6": "afgecd", "7": "abc", "8": "abcdefg", "9": "abcfgd",
        "-": "g", " ": "",
    }

    def seg_digit(self, ch: str, x: int, y: int, w: int, h: int, t: int,
                  color: tuple[int, int, int]) -> None:
        segs = self._SEG.get(ch, "")
        midy = y + (h - t) // 2
        if "a" in segs:
            self.fill_rect(x + t, y, w - 2 * t, t, color)
        if "g" in segs:
            self.fill_rect(x + t, midy, w - 2 * t, t, color)
        if "d" in segs:
            self.fill_rect(x + t, y + h - t, w - 2 * t, t, color)
        if "f" in segs:
            self.fill_rect(x, y + t, t, (h // 2) - t, color)
        if "b" in segs:
            self.fill_rect(x + w - t, y + t, t, (h // 2) - t, color)
        if "e" in segs:
            self.fill_rect(x, y + h // 2, t, (h // 2) - t, color)
        if "c" in segs:
            self.fill_rect(x + w - t, y + h // 2, t, (h // 2) - t, color)

    def seg_text(self, s: str, x: int, y: int, h: int,
                 color: tuple[int, int, int]) -> int:
        """Render a numeric string ('.', ':', '-' and digits). Returns end x."""
        dw = int(h * 0.56)
        t = max(2, h // 9)
        narrow = max(t + 2, dw // 4)
        gap = max(3, h // 12)
        cx = x
        for ch in s:
            if ch == ".":
                self.fill_rect(cx, y + h - t, t, t, color)
                cx += t + gap
            elif ch == ":":
                self.fill_rect(cx, y + h // 3 - t // 2, t, t, color)
                self.fill_rect(cx, y + 2 * h // 3 - t // 2, t, t, color)
                cx += t + gap
            elif ch == " ":
                cx += narrow + gap
            else:
                self.seg_digit(ch, cx, y, dw, h, t, color)
                cx += dw + gap
        return cx - gap

    def seg_text_w(self, s: str, h: int) -> int:
        dw = int(h * 0.56)
        t = max(2, h // 9)
        gap = max(3, h // 12)
        w = 0
        for ch in s:
            if ch in ".: ":
                w += t + gap
            else:
                w += dw + gap
        return w - gap if w else 0

    def seg_text_centered(self, s: str, cx: int, y: int, h: int,
                          color: tuple[int, int, int]) -> None:
        self.seg_text(s, cx - self.seg_text_w(s, h) // 2, y, h, color)


# --------------------------------------------------------------------------
# Output backends
# --------------------------------------------------------------------------
def _rgb_to_565_le(buf: bytearray) -> bytes:
    """Pack an RGB888 buffer into little-endian RGB565 bytes."""
    out = bytearray(len(buf) // 3 * 2)
    j = 0
    for i in range(0, len(buf), 3):
        v = ((buf[i] & 0xF8) << 8) | ((buf[i + 1] & 0xFC) << 3) | (buf[i + 2] >> 3)
        out[j] = v & 0xFF
        out[j + 1] = (v >> 8) & 0xFF
        j += 2
    return bytes(out)


class SimDisplay:
    """Writes each flushed frame to a PNG -- host preview, no hardware."""

    def __init__(self, path: str = "frame.png") -> None:
        self.path = path
        self.frame = 0

    def flush(self, canvas: Canvas) -> None:
        write_png(self.path, canvas)
        self.frame += 1

    def close(self) -> None:
        pass


class FbDisplay:
    """Renders to a Linux framebuffer (ST7789 exposed as /dev/fb0 via fbtft)."""

    FBIOGET_VSCREENINFO = 0x4600

    def __init__(self, dev: str = "/dev/fb0") -> None:
        import fcntl
        import mmap

        self.fd = os.open(dev, os.O_RDWR)
        vinfo = bytearray(160)
        try:
            fcntl.ioctl(self.fd, self.FBIOGET_VSCREENINFO, vinfo)
            xres, yres = struct.unpack_from("<II", vinfo, 0)
            self.bpp = struct.unpack_from("<I", vinfo, 24)[0]
            self.xres, self.yres = xres or WIDTH, yres or HEIGHT
        except OSError:
            self.xres, self.yres, self.bpp = WIDTH, HEIGHT, 16
        if self.bpp not in (16, 32):
            self.bpp = 16
        self.stride = self.xres * (self.bpp // 8)
        self.size = self.stride * self.yres
        self.mm = mmap.mmap(self.fd, self.size, mmap.MAP_SHARED,
                            mmap.PROT_READ | mmap.PROT_WRITE)
        # Centre the 240x240 image if the panel is larger.
        self.ox = max(0, (self.xres - WIDTH) // 2)
        self.oy = max(0, (self.yres - HEIGHT) // 2)

    def flush(self, canvas: Canvas) -> None:
        if self.bpp == 16:
            self._flush16(canvas)
        else:
            self._flush32(canvas)

    def _flush16(self, canvas: Canvas) -> None:
        line565 = WIDTH * 2
        for y in range(HEIGHT):
            row = _rgb_to_565_le(canvas.buf[y * WIDTH * 3:(y + 1) * WIDTH * 3])
            off = (self.oy + y) * self.stride + self.ox * 2
            self.mm[off:off + line565] = row

    def _flush32(self, canvas: Canvas) -> None:
        for y in range(HEIGHT):
            src = canvas.buf[y * WIDTH * 3:(y + 1) * WIDTH * 3]
            row = bytearray(WIDTH * 4)
            for x in range(WIDTH):
                s = x * 3
                d = x * 4
                row[d] = src[s + 2]      # B
                row[d + 1] = src[s + 1]  # G
                row[d + 2] = src[s]      # R
                row[d + 3] = 0xFF        # A/X
            off = (self.oy + y) * self.stride + self.ox * 4
            self.mm[off:off + WIDTH * 4] = bytes(row)

    def close(self) -> None:
        try:
            self.mm.close()
        finally:
            os.close(self.fd)


def open_display(prefer: str = "auto", sim_path: str = "frame.png"):
    """Pick a backend. ``prefer`` in {auto, fb, sim} (spi handled by caller)."""
    if prefer == "sim":
        return SimDisplay(sim_path)
    if prefer in ("auto", "fb"):
        if os.path.exists("/dev/fb0"):
            try:
                return FbDisplay("/dev/fb0")
            except OSError:
                pass
        if prefer == "fb":
            raise RuntimeError("no usable /dev/fb0")
    return SimDisplay(sim_path)


# --------------------------------------------------------------------------
# Minimal stdlib PNG writer (RGB, no inter-row filtering) for sim output.
# --------------------------------------------------------------------------
def write_png(path: str, canvas: Canvas) -> None:
    w, h = canvas.w, canvas.h
    raw = bytearray()
    for y in range(h):
        raw.append(0)  # filter type 0 (None)
        raw.extend(canvas.buf[y * w * 3:(y + 1) * w * 3])

    def chunk(tag: bytes, data: bytes) -> bytes:
        c = tag + data
        return struct.pack(">I", len(data)) + c + struct.pack(">I", zlib.crc32(c) & 0xFFFFFFFF)

    ihdr = struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0)
    png = (b"\x89PNG\r\n\x1a\n"
           + chunk(b"IHDR", ihdr)
           + chunk(b"IDAT", zlib.compress(bytes(raw), 6))
           + chunk(b"IEND", b""))
    with open(path, "wb") as f:
        f.write(png)
