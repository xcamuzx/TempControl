#include "ui.h"

#include <M5Unified.h>

#include <cstdio>

#include "config.h"

namespace ui {

namespace {

M5Canvas canvas(&M5.Display);

// ─── Timer button geometry (idle screen) ────────────────────────────────────
constexpr int BTN_Y = 166;
constexpr int BTN_H = 60;
constexpr int BTN_MARGIN = 12;
constexpr int BTN_GAP = 10;
constexpr int BTN_W = (SCREEN_W - 2 * BTN_MARGIN - 2 * BTN_GAP) / 3;  // 92

int btnX(int i) { return BTN_MARGIN + i * (BTN_W + BTN_GAP); }

void formatClock(uint32_t total_s, char* buf, size_t n) {
  snprintf(buf, n, "%lu:%02lu", (unsigned long)(total_s / 60),
           (unsigned long)(total_s % 60));
}

// Battery dot colour from percentage (same thresholds as the Pi UI).
uint16_t battColor(int pct) {
  if (pct < 0) return C_MUTE;
  if (pct > 25) return C_OK;
  if (pct > 10) return C_LOW;
  return C_CRIT;
}

// ─── Shared header: battery (top-right), and on idle the wordmark ───────────
void drawBattery(int battPct) {
  char buf[8];
  if (battPct < 0) snprintf(buf, sizeof(buf), "--%%");
  else snprintf(buf, sizeof(buf), "%d%%", battPct);
  canvas.setFont(&fonts::FreeSansBold9pt7b);
  canvas.setTextDatum(top_right);
  canvas.setTextColor(C_MUTE);
  canvas.drawString(buf, SCREEN_W - 12, 10);
  // status dot to the left of the percentage
  int tw = canvas.textWidth(buf);
  canvas.fillCircle(SCREEN_W - 12 - tw - 9, 17, 4, battColor(battPct));
}

void drawTempBig(float tempC, bool valid, int cy) {
  char buf[12];
  if (valid) snprintf(buf, sizeof(buf), "%.1f", tempC);
  else snprintf(buf, sizeof(buf), "--");
  canvas.setFont(&fonts::Font7);
  canvas.setTextSize(1);
  canvas.setTextDatum(middle_center);
  canvas.setTextColor(C_WHITE);
  int tw = canvas.textWidth(buf);
  canvas.drawString(buf, SCREEN_W / 2, cy);
  // degree ring at the upper-right of the number
  int dx = SCREEN_W / 2 + tw / 2 + 12;
  int dy = cy - 16;
  canvas.drawCircle(dx, dy, 5, C_MID);
  canvas.drawCircle(dx, dy, 4, C_MID);
}

// ─── Idle: temperature + three timer buttons ────────────────────────────────
void renderIdle(float tempC, bool tempValid, int battPct) {
  canvas.setFont(&fonts::FreeSansBold9pt7b);
  canvas.setTextDatum(top_left);
  canvas.setTextColor(C_PALE);
  canvas.drawString("NEWY ICE BATHS", 12, 10);
  drawBattery(battPct);

  canvas.setFont(&fonts::FreeSans9pt7b);
  canvas.setTextDatum(top_center);
  canvas.setTextColor(C_PALE);
  canvas.drawString("BATH TEMPERATURE", SCREEN_W / 2, 34);

  drawTempBig(tempC, tempValid, 82);

  canvas.setFont(&fonts::FreeSans9pt7b);
  canvas.setTextDatum(top_center);
  canvas.setTextColor(C_MUTE);
  canvas.drawString("CELSIUS  -  LIVE", SCREEN_W / 2, 116);

  canvas.setTextColor(C_PALE);
  canvas.drawString("THE CHALLENGE  -  TAP TO START", SCREEN_W / 2, 148);

  const char* mins[3] = {"1:00", "2:00", "3:00"};
  for (int i = 0; i < 3; ++i) {
    int x = btnX(i);
    canvas.drawRoundRect(x, BTN_Y, BTN_W, BTN_H, 6, C_MID);
    canvas.drawRoundRect(x, BTN_Y, BTN_W, BTN_H, 6, C_MID);  // 2px-ish border
    canvas.fillRect(x, BTN_Y, 3, BTN_H, C_MID);              // accent bar
    canvas.setFont(&fonts::FreeSansBold18pt7b);
    canvas.setTextDatum(middle_center);
    canvas.setTextColor(C_WHITE);
    canvas.drawString(mins[i], x + BTN_W / 2, BTN_Y + 24);
    canvas.setFont(&fonts::FreeSans9pt7b);
    canvas.setTextColor(C_MUTE);
    canvas.drawString("MIN", x + BTN_W / 2, BTN_Y + 46);
  }
}

// ─── Box-breathing pacer: dot travels the square, 4 s per edge ──────────────
void drawBreathing(uint32_t elapsed_ms) {
  const int cx = SCREEN_W / 2;
  const int cy = 150;
  const int h = 50;  // half-edge

  canvas.drawRect(cx - h, cy - h, 2 * h, 2 * h, C_BLUE);

  uint32_t phaseTime = elapsed_ms % BREATH_LOOP_MS;
  int phase = phaseTime / BREATH_PHASE_MS;  // 0..3
  float f = (phaseTime % BREATH_PHASE_MS) / (float)BREATH_PHASE_MS;

  // BL → TL → TR → BR → BL  (Inhale, Hold, Exhale, Hold)
  float px = cx, py = cy;
  const char* label = "HOLD";
  switch (phase) {
    case 0: px = cx - h; py = (cy + h) - f * 2 * h; label = "INHALE"; break;
    case 1: px = (cx - h) + f * 2 * h; py = cy - h; label = "HOLD"; break;
    case 2: px = cx + h; py = (cy - h) + f * 2 * h; label = "EXHALE"; break;
    case 3: px = (cx + h) - f * 2 * h; py = cy + h; label = "HOLD"; break;
  }

  canvas.fillCircle((int)px, (int)py, 11, C_BLUE);   // halo
  canvas.fillCircle((int)px, (int)py, 7, C_MID);     // body
  canvas.fillCircle((int)px, (int)py, 3, C_WHITE);   // core

  canvas.setFont(&fonts::FreeSansBold12pt7b);
  canvas.setTextDatum(middle_center);
  canvas.setTextColor(C_PALE);
  canvas.drawString(label, cx, cy);
}

// ─── Running: countdown + breathing pacer; temperature stays visible ────────
void renderRunning(const Challenge& ch, float tempC, bool tempValid,
                   int battPct) {
  // Temperature must remain visible at all times → top-left readout.
  char tbuf[16];
  if (tempValid) snprintf(tbuf, sizeof(tbuf), "%.1f C", tempC);
  else snprintf(tbuf, sizeof(tbuf), "-- C");
  canvas.setFont(&fonts::FreeSansBold12pt7b);
  canvas.setTextDatum(top_left);
  canvas.setTextColor(C_MID);
  canvas.drawString(tbuf, 12, 8);
  drawBattery(battPct);

  char cbuf[8];
  formatClock(ch.remainingS(), cbuf, sizeof(cbuf));
  canvas.setFont(&fonts::Font7);
  canvas.setTextSize(1);
  canvas.setTextDatum(middle_center);
  canvas.setTextColor(C_WHITE);
  canvas.drawString(cbuf, SCREEN_W / 2, 58);

  drawBreathing(millis() - ch.startedAt());
}

// ─── Finished: full-screen congratulations overlay ──────────────────────────
void renderFinished() {
  canvas.setFont(&fonts::FreeSans12pt7b);
  canvas.setTextDatum(bottom_center);
  canvas.setTextColor(C_PALE);
  canvas.drawString("CONGRATULATIONS", SCREEN_W / 2, 86);

  canvas.setFont(&fonts::FreeSansBold24pt7b);
  canvas.setTextDatum(middle_center);
  canvas.setTextColor(C_WHITE);
  canvas.drawString("CHALLENGE", SCREEN_W / 2, 124);
  canvas.drawString("FINISHED", SCREEN_W / 2, 162);

  canvas.drawFastHLine(SCREEN_W / 2 - 40, 188, 80, C_MUTE);

  canvas.setFont(&fonts::FreeSans9pt7b);
  canvas.setTextDatum(top_center);
  canvas.setTextColor(C_PALE);
  canvas.drawString("tap to continue", SCREEN_W / 2, 206);
}

}  // namespace

void begin() {
  canvas.setColorDepth(16);
  canvas.setPsram(true);
  canvas.createSprite(SCREEN_W, SCREEN_H);
}

void render(const Challenge& ch, float tempC, bool tempValid, int battPct) {
  canvas.fillSprite(C_DEEP);

  switch (ch.status()) {
    case Status::Idle:     renderIdle(tempC, tempValid, battPct); break;
    case Status::Running:  renderRunning(ch, tempC, tempValid, battPct); break;
    case Status::Finished: renderFinished(); break;
  }

  canvas.pushSprite(0, 0);
}

int timerButtonAt(int x, int y) {
  if (y < BTN_Y || y > BTN_Y + BTN_H) return -1;
  for (int i = 0; i < 3; ++i) {
    int bx = btnX(i);
    if (x >= bx && x <= bx + BTN_W) return i;
  }
  return -1;
}

}  // namespace ui
