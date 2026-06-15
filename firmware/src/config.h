// Shared configuration: hardware pins, colour palette, challenge limits.
//
// Mirrors the constraints from the Raspberry Pi app (app/main.py):
//   - 1..4 participants, 60..180 s countdown.
// Names are not entered on-device in this standalone phase (no keyboard);
// they arrive via the WiFi web roster in phase 2.
#pragma once

#include <cstdint>

// ─── Sensor wiring ──────────────────────────────────────────────────────────
// SHT3x on the Core2 Grove Port A (red). Port A is the *external* I2C bus —
// the internal bus (GPIO21/22) is reserved for the AXP192 PMU + touch
// controller, so we never share it. Port A: SDA = GPIO32, SCL = GPIO33.
constexpr int   PIN_I2C_SDA   = 32;
constexpr int   PIN_I2C_SCL   = 33;
constexpr uint8_t SHT3X_ADDR  = 0x44;   // SHT30/31/35 default address
constexpr uint32_t I2C_HZ     = 100000; // 100 kHz is plenty for this sensor

// ─── Challenge limits (hard caps, matching the Pi backend) ──────────────────
constexpr uint32_t MIN_DURATION_S = 60;
constexpr uint32_t MAX_DURATION_S = 180;

// The three selectable countdowns shown as buttons.
constexpr uint32_t DURATION_CHOICES_S[3] = {60, 120, 180};

// ─── Box-breathing pacer ────────────────────────────────────────────────────
// One 4 s phase per square edge → 16 s loop, matching the Pi UI's border
// comet. Order BL → TL → TR → BR → BL = Inhale, Hold, Exhale, Hold.
constexpr uint32_t BREATH_PHASE_MS = 4000;
constexpr uint32_t BREATH_LOOP_MS  = BREATH_PHASE_MS * 4;

// ─── Colour palette (locked, from CLAUDE.md), as RGB565 ─────────────────────
constexpr uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
  return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}
constexpr uint16_t C_DEEP  = rgb565(0x2e, 0x5d, 0x74); // deep base
constexpr uint16_t C_MID   = rgb565(0x2f, 0x89, 0xb9); // blue accent
constexpr uint16_t C_BLUE  = rgb565(0x0f, 0x75, 0xa8); // blue accent (darker)
constexpr uint16_t C_WHITE = rgb565(0xff, 0xff, 0xff);
constexpr uint16_t C_PALE  = rgb565(0xdd, 0xdd, 0xdd);
constexpr uint16_t C_MUTE  = rgb565(0x6e, 0x6f, 0x72);

// Battery dot status colours (from the Pi UI thresholds).
constexpr uint16_t C_OK    = rgb565(0x5d, 0xab, 0x6a);
constexpr uint16_t C_LOW   = rgb565(0xd4, 0x81, 0x3a);
constexpr uint16_t C_CRIT  = rgb565(0xc9, 0x4a, 0x4a);

// ─── WiFi access point (phase 2 — name-roster upload) ───────────────────────
// The Core2 hosts its own SoftAP; an operator joins it and opens the roster
// page at http://192.168.4.1/. WPA2 needs a password of >= 8 chars; set to
// nullptr for an open network.
constexpr const char* AP_SSID     = "NewyIceBaths";
constexpr const char* AP_PASSWORD = "icebath2026";

// ─── Screen ─────────────────────────────────────────────────────────────────
constexpr int SCREEN_W = 320;
constexpr int SCREEN_H = 240;
