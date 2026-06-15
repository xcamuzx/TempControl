// TempControl ice-bath kiosk — M5Stack Core2 firmware (standalone phase).
//
// Live SHT3x temperature + a 1/2/3-minute countdown with a box-breathing
// pacer, the finish overlay, and the Core2's own battery level. Touch only:
// tap a timer button to start, tap the finish overlay to reset. No WiFi yet.
#include <M5Unified.h>

#include "challenge.h"
#include "config.h"
#include "sht3x.h"
#include "ui.h"

namespace {

SHT3x sensor(SHT3X_ADDR);
Challenge challenge;

// Polling cadence mirrors the Pi UI: temperature ~2 s, battery ~30 s.
constexpr uint32_t TEMP_PERIOD_MS = 2000;
constexpr uint32_t BATT_PERIOD_MS = 30000;
// Render policy: the breathing pacer animates while a countdown runs; the
// idle/finished screens are static and only redrawn when something changes.
constexpr uint32_t ANIM_PERIOD_MS = 50;   // ~20 fps while running
constexpr uint32_t STATIC_MIN_MS  = 100;  // debounce static-screen redraws
constexpr uint32_t LOOP_DELAY_MS  = 5;    // keep touch responsive, avoid spin

float    g_tempC = 0.0f;
bool     g_tempValid = false;
int      g_battPct = -1;
uint32_t g_lastTemp = 0;
uint32_t g_lastBatt = 0;
uint32_t g_lastFrame = 0;
bool     g_rendered = false;     // has the first frame been drawn?
bool     g_dirty = true;         // a static-screen input changed → redraw
Status   g_shownStatus = Status::Idle;

void pollSensor() {
  Reading r;
  if (sensor.read(r)) {
    g_tempC = r.temp_c;
    g_tempValid = true;
  } else {
    g_tempValid = false;
    Serial.println("sensor read failed after retries");
  }
  g_dirty = true;  // temperature readout may have changed
}

void handleTap(int x, int y) {
  switch (challenge.status()) {
    case Status::Idle: {
      int idx = ui::timerButtonAt(x, y);
      if (idx >= 0) challenge.start(DURATION_CHOICES_S[idx]);
      break;
    }
    case Status::Finished:
      challenge.ack();
      break;
    case Status::Running:
      break;  // countdown runs to completion; no cancel (matches the kiosk)
  }
}

// Decide whether this loop iteration needs to repaint the screen.
bool needsRender(uint32_t now) {
  if (!g_rendered) return true;
  if (challenge.status() != g_shownStatus) return true;  // state transition
  if (challenge.status() == Status::Running)
    return now - g_lastFrame >= ANIM_PERIOD_MS;          // animate the pacer
  return g_dirty && (now - g_lastFrame >= STATIC_MIN_MS); // temp/battery change
}

}  // namespace

void setup() {
  auto cfg = M5.config();
  M5.begin(cfg);
  M5.Display.setRotation(1);  // landscape, 320x240
  M5.Display.setBrightness(160);

  Serial.begin(115200);
  sensor.begin();
  ui::begin();

  pollSensor();
  g_battPct = M5.Power.getBatteryLevel();
  g_lastTemp = g_lastBatt = millis();
}

void loop() {
  M5.update();

  auto t = M5.Touch.getDetail();
  if (t.wasPressed()) handleTap(t.x, t.y);

  uint32_t now = millis();
  if (now - g_lastTemp >= TEMP_PERIOD_MS) {
    pollSensor();
    g_lastTemp = now;
  }
  if (now - g_lastBatt >= BATT_PERIOD_MS) {
    g_battPct = M5.Power.getBatteryLevel();
    g_lastBatt = now;
    g_dirty = true;
  }

  challenge.tick();  // lazy running → finished transition

  if (needsRender(now)) {
    ui::render(challenge, g_tempC, g_tempValid, g_battPct);
    g_lastFrame = now;
    g_shownStatus = challenge.status();
    g_rendered = true;
    g_dirty = false;
  }

  delay(LOOP_DELAY_MS);
}
