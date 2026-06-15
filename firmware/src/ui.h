// On-screen kiosk UI for the Core2 (320x240). Reimplements the Pi web UI as
// native M5GFX draw calls: temperature, timer buttons, live countdown, the
// box-breathing pacer, and the finish overlay — drawn into an off-screen
// canvas and pushed each frame for flicker-free animation.
#pragma once

#include "challenge.h"

class Roster;  // pointer-only; defined in roster.h

namespace ui {

// Everything the renderer needs beyond the challenge state. Bundled so the
// render signature stays small as more is shown (temp, battery, roster, AP).
struct View {
  float tempC = 0.0f;
  bool tempValid = false;   // false → temperature shows "--"
  int battPct = -1;         // < 0 → battery shows "--%"
  const Roster* roster = nullptr;  // may be null / empty
  const char* apSsid = "";  // WiFi AP name, for "how to connect"
  const char* apIp = "";    // WiFi AP IP
};

// Allocates the full-screen canvas. Call once after M5.begin().
void begin();

// Renders the current frame for the given state + view inputs.
void render(const Challenge& ch, const View& v);

// Hit-test for the idle screen's three timer buttons.
// Returns the choice index 0..2 (→ DURATION_CHOICES_S), or -1 if no hit.
int timerButtonAt(int x, int y);

}  // namespace ui
