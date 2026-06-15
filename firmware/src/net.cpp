#include "net.h"

#include <WebServer.h>
#include <WiFi.h>

#include "config.h"

namespace net {

namespace {

WebServer server(80);
Roster* g_roster = nullptr;
String g_ip = "0.0.0.0";

// Escape the five HTML-significant characters before echoing user-supplied
// names back into the page (keeps the markup valid and avoids injection on the
// local AP).
String htmlEscape(const std::string& s) {
  String out;
  out.reserve(s.size() + 8);
  for (char c : s) {
    switch (c) {
      case '&': out += "&amp;"; break;
      case '<': out += "&lt;"; break;
      case '>': out += "&gt;"; break;
      case '"': out += "&quot;"; break;
      case '\'': out += "&#39;"; break;
      default: out += c; break;
    }
  }
  return out;
}

String page() {
  String h;
  h.reserve(1600);
  h += F(
      "<!doctype html><html><head><meta charset=utf-8>"
      "<meta name=viewport content=\"width=device-width,initial-scale=1\">"
      "<title>Newy Ice Baths - Roster</title><style>"
      "*{box-sizing:border-box}"
      "body{margin:0;padding:24px;background:#2e5d74;color:#fff;"
      "font-family:system-ui,sans-serif}"
      "h1{font-size:1.3rem;letter-spacing:.12em;margin:0 0 4px}"
      "p{color:#dddddd;margin:.2rem 0 1.2rem}"
      "label{display:block;font-size:.8rem;color:#dddddd;margin:.8rem 0 .2rem;"
      "letter-spacing:.1em}"
      "input{width:100%;padding:12px;border:1px solid #2f89b9;border-radius:6px;"
      "background:#27506420;color:#fff;font-size:1.1rem}"
      "button{margin-top:1.4rem;width:100%;padding:14px;border:0;border-radius:6px;"
      "background:#0f75a8;color:#fff;font-size:1.1rem;letter-spacing:.1em}"
      ".note{margin-top:1.4rem;color:#6e6f72;font-size:.8rem}"
      "</style></head><body>"
      "<h1>NEWY ICE BATHS</h1><p>Participant roster - up to 4 names</p>"
      "<form method=POST action=/save>");
  for (std::size_t i = 0; i < Roster::MAX; ++i) {
    String val;
    if (g_roster && i < g_roster->size()) val = htmlEscape((*g_roster)[i]);
    h += "<label>Participant " + String((int)i + 1) + "</label>";
    h += "<input name=name" + String((int)i) +
         " maxlength=32 autocomplete=off value=\"" + val + "\">";
  }
  h += F(
      "<button>Save roster</button></form>"
      "<p class=note>Start the challenge on the device screen.</p>"
      "</body></html>");
  return h;
}

void handleRoot() { server.send(200, "text/html", page()); }

void handleSave() {
  if (g_roster) {
    std::vector<std::string> raw;
    raw.reserve(Roster::MAX);
    for (std::size_t i = 0; i < Roster::MAX; ++i) {
      String arg = "name" + String((int)i);
      if (server.hasArg(arg)) raw.emplace_back(server.arg(arg).c_str());
    }
    g_roster->setFrom(raw);
  }
  // Redirect back to the form (PRG) so a refresh doesn't re-submit.
  server.sendHeader("Location", "/");
  server.send(303, "text/plain", "");
}

}  // namespace

void begin(Roster* roster) {
  g_roster = roster;
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASSWORD);
  g_ip = WiFi.softAPIP().toString();

  server.on("/", HTTP_GET, handleRoot);
  server.on("/save", HTTP_POST, handleSave);
  server.onNotFound(handleRoot);
  server.begin();
}

void loop() { server.handleClient(); }

const char* ssid() { return AP_SSID; }
const char* ip() { return g_ip.c_str(); }

}  // namespace net
