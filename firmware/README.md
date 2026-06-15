# TempControl — M5Stack Core2 firmware

A native port of the ice-bath kiosk to the **M5Stack Core2** (ESP32, 320×240
capacitive touch). This is a **separate codebase** from the Raspberry Pi app in
`../app/` — the FastAPI/Jinja stack does not run on a microcontroller, so the
state machine and SHT3x driver were re-implemented in C++ (Arduino framework)
and the web UI was rebuilt as native draw calls.

## Status

**Phase 1 — standalone (this firmware).** No WiFi. The screen shows:

- live **temperature** from the SHT3x (always visible),
- three **timer buttons** (1 / 2 / 3 min) — tap to start,
- a drift-free **countdown** with a **box-breathing pacer** (a dot travels a
  square, 4 s per edge: Inhale · Hold · Exhale · Hold),
- a full-screen **finish overlay** (“CHALLENGE FINISHED”, tap to reset),
- the Core2's own **battery** level (AXP192) in the header.

Hard caps match the Pi backend: 60–180 s countdown. Rendering is
event-driven — static screens repaint only on change, and the breathing pacer
animates at a capped rate only while a countdown runs (see
[ARCHITECTURE.md](./ARCHITECTURE.md#render-policy-the-main-optimization)).

**Phase 2 — planned (not built).** Boot a WiFi access point + small web page so
an operator can upload a participant **name roster** from a phone; names then
appear on-device during a challenge.

## Wiring

Connect the **SHT3x to Grove Port A** (the red port) — that's the Core2's
*external* I2C bus. Do **not** use the internal bus (GPIO21/22); it's reserved
for the AXP192 PMU and the touch controller.

| SHT3x | Core2 Port A |
|-------|--------------|
| VCC   | 3V3 / 5V     |
| GND   | GND          |
| SDA   | **GPIO32**   |
| SCL   | **GPIO33**   |

Pins live in `src/config.h` (`PIN_I2C_SDA`, `PIN_I2C_SCL`) if your breakout
differs. Sensor address is `0x44` (SHT30/31/35 default).

## Build & flash

Requires [PlatformIO](https://platformio.org/). With the Core2 on USB:

```bash
cd firmware
pio run                 # compile
pio run -t upload       # flash
pio device monitor      # serial @ 115200 (sensor errors print here)
```

> **Note:** the first `pio run` downloads the `espressif32` platform, the
> Xtensa toolchain, and `M5Unified`. This must run somewhere with access to
> `api.registry.platformio.org` and `dl.registry.platformio.org` — it was
> **not** buildable inside the restricted CI/web sandbox, so do the first build
> on a machine with open egress.

## Layout

```
firmware/
  platformio.ini      board = m5stack-core2, framework = arduino, lib M5Unified
  src/
    config.h          pins, colour palette (locked), duration caps, breathing timing
    sht3x.h/.cpp      SHT3x driver — port of ../app/sensor.py (cmd 0x2C06, CRC-8)
    challenge.h       state machine — port of ChallengeState in ../app/main.py
    breathing.h       box-breathing dot geometry (pure, host-testable)
    ui.h/.cpp         native 320×240 UI (canvas-buffered, event-driven repaint)
    main.cpp          setup/loop: poll sensor + battery, handle touch, render policy
  test/               host unit tests for the pure logic (no hardware needed)
  ARCHITECTURE.md     module map, data flow, state machine, render strategy, Pi parity
```

See [ARCHITECTURE.md](./ARCHITECTURE.md) for the full design.

## Tests

The hardware-independent logic (CRC-8, state machine, breathing geometry) is
unit-tested on the host with a stub `Arduino.h` — no device or M5 toolchain
required:

```bash
cd firmware/test
./run.sh
```

This covers: CRC-8 vs the SHT3x datasheet vector (`0xBEEF → 0x92`); duration
clamping (`999→180`, `10→60`); remaining-seconds rounding; the lazy
`running→finished` and `ack→idle` transitions; and box-breathing path
continuity (edges meet, loop closes) with correct phase labels.

## Verified vs. pending

✅ **Host-verified:** all of the above (23 assertions, clean under
`-Wall -Wextra`).

⏳ **Pending on hardware:** the full firmware needs the M5 toolchain to compile
(see the build note above — the PlatformIO registry must be reachable). UI
rendering, touch hit-testing, and the live SHT3x path must still be confirmed
on the Core2 itself.
