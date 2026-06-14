// On-screen kiosk UI for the Core2 (320x240). Reimplements the Pi web UI as
// native M5GFX draw calls: temperature, timer buttons, live countdown, the
// box-breathing pacer, and the finish overlay — drawn into an off-screen
// canvas and pushed each frame for flicker-free animation.
#pragma once

#include "challenge.h"

namespace ui {

// Allocates the full-screen canvas. Call once after M5.begin().
void begin();

// Renders the current frame for the given state.
//   tempValid = false → temperature shows "--" (sensor unavailable).
//   battPct   < 0      → battery shows "--%".
void render(const Challenge& ch, float tempC, bool tempValid, int battPct);

// Hit-test for the idle screen's three timer buttons.
// Returns the choice index 0..2 (→ DURATION_CHOICES_S), or -1 if no hit.
int timerButtonAt(int x, int y);

}  // namespace ui
