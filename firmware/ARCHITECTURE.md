# Core2 firmware — architecture

How the standalone M5Stack Core2 firmware is put together, why it's a rewrite
rather than a port, and how it maps back to the Raspberry Pi app in `../app/`.

## Why a rewrite, not a port

The Pi app is **FastAPI + uvicorn + Jinja + an 800-line HTML/CSS/JS frontend**.
It needs Linux, CPython, and a web browser to render. The Core2 is a bare
ESP32 that drives its 320×240 panel directly — no OS, no browser. So none of
the web stack runs. What carries over is the *logic and behaviour*, which were
re-implemented in C++ (Arduino framework):

- the **state machine** (`idle → running → finished`, 60–180 s caps),
- the **SHT3x driver** (single-shot `0x2C06` read, CRC-8, retry/backoff),
- the **UX** (live temperature, timer choices, drift-free countdown, finish
  overlay) — rebuilt as native draw calls, plus a box-breathing pacer in place
  of the web UI's border comet.

The UPS battery (`app/ups.py`, MAX17040 @ `0x36`) is Pi-HAT hardware and has no
analogue here; the Core2's own AXP192 fuel gauge is read instead.

## Module map

| File | Responsibility | Depends on |
|------|----------------|-----------|
| `src/config.h`     | Pins, colour palette, duration caps, breathing timing — all the tunables in one place. | — |
| `src/sht3x.{h,cpp}`| SHT3x I2C driver. Pure hardware I/O; mirrors `app/sensor.py`. | `Wire` |
| `src/challenge.h`  | State machine. Pure timing logic (`millis()`); mirrors `ChallengeState`. | `Arduino` (millis) |
| `src/breathing.h`  | Box-breathing dot geometry. **Pure**, no drawing → host-testable. | `config.h` |
| `src/ui.{h,cpp}`   | All rendering: canvas buffer, screens, touch hit-testing. Stateless. | `M5Unified`, `breathing.h` |
| `src/main.cpp`     | Orchestration: setup, the loop, polling cadence, touch dispatch, render policy. | everything |
| `test/`            | Host unit tests for the pure logic (no hardware needed). | `g++` |

The dependency direction is one-way (`main` → `ui` → `breathing`/`config`;
`main` → `challenge`/`sht3x`). No module reaches back up, and the two pure
headers (`challenge.h`, `breathing.h`) have no M5 dependency, which is what lets
them be unit-tested on a host.

## Data flow — the loop

```
loop():
  M5.update()                      // refresh touch/buttons/power
  touch?  → handleTap(x, y)        // idle: start; finished: ack; running: ignore
  every 2 s  → pollSensor()        // SHT3x read (retry), update temp + dirty flag
  every 30 s → battery level       // AXP192, update + dirty flag
  challenge.tick()                 // lazy running → finished
  needsRender()? → ui::render(...) // see render policy below
  delay(5 ms)                      // keep touch snappy without busy-spinning
```

State lives in two places only: the module-level `Challenge` (the machine) and
a handful of `main.cpp` globals for the latest sensor/battery sample and render
bookkeeping. `ui` holds just its off-screen canvas; it's otherwise a pure
function of `(challenge, temp, battery)`.

## State machine

```
        tap timer button (1/2/3 min)
  idle ─────────────────────────────► running
   ▲                                     │
   │ tap overlay (ack)                   │ countdown reaches 0:00
   │                                     ▼  (lazy, in tick())
  finished ◄──────────────────────────────
```

`tick()` is called once per loop and is the *only* thing that advances
`running → finished`, exactly like the Pi backend's lazy `tick()` — there is no
timer interrupt or background task. `start()` clamps the requested duration to
`[60, 180]` s before arming, so out-of-range input can't escape the caps.

## Render policy (the main optimization)

A full-screen 320×240×16-bit canvas is ~150 KB; pushing it over the panel SPI
bus is the dominant per-frame cost. The first cut re-rendered and pushed every
loop (~60 fps) regardless of state — wasteful, since **idle** and **finished**
are static images.

`main.cpp::needsRender()` makes painting event-driven:

- **state transition** → always repaint;
- **running** → repaint at a capped `ANIM_PERIOD_MS` (~20 fps) so the breathing
  dot animates smoothly;
- **idle / finished** → repaint only when a displayed input changed (a new
  temperature or battery sample sets a `dirty` flag), debounced by
  `STATIC_MIN_MS`.

Net effect: the static screens cost roughly one push every 2 s (a temperature
refresh) instead of 60 per second, and the SPI bus is only worked hard during
the active countdown. `ui::render` stays stateless — all the timing policy
lives in `main`, so the rule is easy to see and change in one spot.

The off-screen canvas is allocated in PSRAM (`setPsram(true)`) and the whole
frame is composited there before a single `pushSprite`, so there's no flicker
or tearing regardless of how much is drawn.

## Parity with the Pi app

| Concern | Pi (`app/`) | Core2 (`firmware/`) |
|--------|-------------|---------------------|
| Sensor read | `sensor.py`: write `0x2C06`, 20 ms, read 6 B, CRC-8 | `sht3x.cpp`: identical sequence over `Wire` |
| CRC-8 | poly `0x31`, init `0xFF` | same (verified vs datasheet `0xBEEF→0x92`) |
| Retry | 3×, 50/100/150 ms backoff | same |
| Caps | 60–180 s, validated in `StartRequest` | `Challenge::start()` clamps |
| Finish transition | lazy `tick()` per request | lazy `tick()` per loop |
| Countdown render | `requestAnimationFrame` vs `ends_at` | `remainingS()` (ceil) vs `millis()` deadline |
| Battery | MAX17040 @ `0x36` | AXP192 via `M5.Power` |
| Names | 1–4, typed in browser | not on-device (no keyboard) — WiFi roster, phase 2 |

## Memory & timing notes

- **PSRAM**: the canvas needs it; the `m5stack-core2` board enables PSRAM by
  default in `platformio.ini`.
- **Blocking sensor reads**: `SHT3x::read()` blocks ~20 ms on success and up to
  ~360 ms across a full failed retry chain. During a failure burst the loop
  (and the breathing animation) stalls briefly. Acceptable for a kiosk; if it
  ever matters, the read can be made non-blocking (issue command, return, read
  on a later loop once 20 ms have elapsed).
- **`millis()` rollover** (~49 days) is handled with signed-difference
  comparisons throughout `challenge.h`.

## Testing

`test/` host-compiles the pure logic with a stub `Arduino.h` (no toolchain, no
device) and asserts:

- SHT3x CRC-8 against the datasheet vector,
- duration clamping, remaining-seconds rounding, and the lazy finish/ack
  transitions,
- box-breathing path continuity (each edge's end meets the next edge's start,
  and the loop closes) and phase labels.

```bash
cd firmware/test && ./run.sh
```

The full firmware (UI, touch, live sensor) still requires the M5 toolchain and
on-hardware verification — see `README.md`.

## Future work

- **Phase 2 — WiFi roster**: boot a SoftAP + a small web page to upload
  participant names; show them during a challenge. This is the one piece of the
  original UX intentionally deferred from the standalone build.
- **Sub-region animation**: if the ~20 fps full-frame push during a countdown
  ever proves too heavy, animate only the breathing-square sub-rect and repaint
  the countdown digits once per second, instead of recompositing the whole
  frame.
- **Non-blocking sensor read** (see above) if UI smoothness during sensor
  faults becomes a concern.
