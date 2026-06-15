// WiFi SoftAP + web server for the participant-roster upload (phase 2).
//
// Brings up an access point and serves a small form at http://<apIp>/ that
// reads/writes the shared Roster. Device-only (needs the ESP32 WiFi stack);
// the roster *model* it edits lives in the host-testable roster.h.
#pragma once

#include "roster.h"

namespace net {

// Start the SoftAP and the web server, bound to the given roster.
void begin(Roster* roster);

// Service pending web clients — call once per loop().
void loop();

// AP details, for showing "how to connect" on the device screen.
const char* ssid();
const char* ip();

}  // namespace net
