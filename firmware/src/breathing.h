// Box-breathing pacer geometry — pure, no drawing, so it can be unit-tested
// on the host. Given the elapsed time, returns where the pacer dot sits on the
// square and which phase label to show.
//
// One 4 s phase per edge → 16 s loop, travelling BL → TL → TR → BR → BL:
//   phase 0 Inhale (up the left edge)
//   phase 1 Hold   (along the top)
//   phase 2 Exhale (down the right edge)
//   phase 3 Hold   (along the bottom)
#pragma once

#include <cstdint>

#include "config.h"

struct BreathPoint {
  int phase;          // 0..3
  float x;
  float y;
  const char* label;  // "INHALE" / "HOLD" / "EXHALE" / "HOLD"
};

// (cx, cy) = square centre, h = half-edge length.
inline BreathPoint breathAt(uint32_t elapsed_ms, float cx, float cy, float h) {
  const uint32_t in_loop = elapsed_ms % BREATH_LOOP_MS;
  const int phase = static_cast<int>(in_loop / BREATH_PHASE_MS);  // 0..3
  const float f =
      (in_loop % BREATH_PHASE_MS) / static_cast<float>(BREATH_PHASE_MS);
  const float side = 2.0f * h;

  BreathPoint p{phase, cx, cy, "HOLD"};
  switch (phase) {
    case 0: p.x = cx - h;          p.y = (cy + h) - f * side; p.label = "INHALE"; break;
    case 1: p.x = (cx - h) + f * side; p.y = cy - h;          p.label = "HOLD";   break;
    case 2: p.x = cx + h;          p.y = (cy - h) + f * side; p.label = "EXHALE"; break;
    default: p.x = (cx + h) - f * side; p.y = cy + h;         p.label = "HOLD";   break;
  }
  return p;
}
