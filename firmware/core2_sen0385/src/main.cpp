#include <Arduino.h>
#include <SD.h>
#include <SPI.h>
#include <WebServer.h>
#include <WiFi.h>
#include <Wire.h>
#include <Adafruit_NeoPixel.h>

namespace {
constexpr int kI2cSdaPin = 32;  // M5Stack Core2 Port A yellow wire.
constexpr int kI2cSclPin = 33;  // M5Stack Core2 Port A white wire.
constexpr uint32_t kI2cFrequencyHz = 100000;
constexpr uint8_t kPrimarySensorAddress = 0x44;
constexpr uint8_t kAlternateSensorAddress = 0x45;
constexpr uint8_t kSingleShotHighRepeatability[] = {0x2C, 0x06};
constexpr uint32_t kSensorPollMs = 2000;

constexpr int kSdSckPin = 18;
constexpr int kSdMisoPin = 38;
constexpr int kSdMosiPin = 23;
constexpr int kSdCsPin = 4;

constexpr int kNeoPixelPin = 27;
constexpr uint16_t kNeoPixelCount = 24;
constexpr uint8_t kNeoPixelBrightness = 72;
constexpr uint32_t kRingFrameMs = 33;
constexpr uint32_t kBreathingCountdownMs = 180000;
constexpr uint32_t kBreathPhaseMs = 4000;
constexpr uint8_t kFinishBlinkCount = 3;
constexpr uint32_t kBlinkHalfPeriodMs = 250;

constexpr char kApSsid[] = "TempControl-Core2";
constexpr char kApPassword[] = "tempcontrol";
constexpr char kLoginCsvPath[] = "/logins.csv";

WebServer server(80);
Adafruit_NeoPixel ring(kNeoPixelCount, kNeoPixelPin, NEO_GRB + NEO_KHZ800);

enum class RingMode {
  Idle,
  Prestart,
  Running,
  FinishBlink,
};

uint8_t activeSensorAddress = 0;
bool sdReady = false;
bool lastSensorOk = false;
float lastTemperatureC = NAN;
float lastHumidityRh = NAN;
uint32_t lastSensorPollMs = 0;
RingMode ringMode = RingMode::Idle;
uint32_t ringPrestartStartedMs = 0;
uint32_t ringCountdownStartedMs = 0;
uint32_t ringFinishStartedMs = 0;
uint32_t lastRingFrameMs = 0;

const char kIndexHtml[] PROGMEM = R"rawliteral(
<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>TempControl Core2 Photo Proof</title>
<style>
body{margin:0;padding:20px;background:#2e5d74;color:#fff;font-family:Arial,sans-serif}
.wrap{max-width:860px;margin:0 auto}
h1{font-size:34px;letter-spacing:.08em;text-transform:uppercase}
.card{background:rgba(255,255,255,.08);border:1px solid rgba(255,255,255,.22);padding:18px;margin:14px 0}
label{display:block;margin:12px 0 6px;text-transform:uppercase;letter-spacing:.08em;font-size:13px}
input{box-sizing:border-box;width:100%;padding:12px;background:rgba(255,255,255,.1);border:1px solid rgba(255,255,255,.28);color:#fff;font-size:16px}
input::placeholder{color:rgba(255,255,255,.55)}
button,a{display:inline-block;margin:12px 8px 0 0;padding:12px 14px;background:#0f75a8;border:1px solid rgba(255,255,255,.25);color:#fff;text-decoration:none;text-transform:uppercase;letter-spacing:.08em}
.timer{display:grid;grid-template-columns:repeat(3,1fr);gap:10px;margin-top:12px}
.timer div{background:rgba(255,255,255,.08);padding:12px;border:1px solid rgba(255,255,255,.16)}
.timer strong{display:block;font-size:24px;margin-top:4px}
.grid{display:grid;grid-template-columns:1fr 1fr;gap:12px}
.status{min-height:24px;color:#ddd;margin-top:12px}
canvas{display:block;max-width:100%;margin-top:14px;border:1px solid rgba(255,255,255,.24)}
.hidden{display:none}
@media(max-width:700px){.grid,.timer{grid-template-columns:1fr}h1{font-size:26px}}
</style>
</head>
<body>
<div class="wrap">
  <h1>TempControl Photo Proof</h1>
  <div class="card">
    <div>Sensor: <strong id="temp">-- C</strong> | Humidity: <strong id="humidity">-- %RH</strong></div>
    <div>Core2 SD login logging: <strong>/logins.csv</strong></div>
  </div>
  <div class="card">
    <h2>LED Ring Breathing Timer</h2>
    <div class="timer">
      <div>Status<strong id="ring_status">idle</strong></div>
      <div>Remaining<strong id="ring_remaining">3:00</strong></div>
      <div>Phase<strong id="ring_phase">ready</strong></div>
    </div>
    <button id="timer_start">Start 3 Min</button>
    <button id="timer_stop">Stop</button>
    <div class="status">Ring: 3 start blinks, green inhale 4s, amber hold 4s, red exhale 4s, then 3 red finish blinks.</div>
  </div>
  <div class="card">
    <div class="grid">
      <div><label>Name</label><input id="name" maxlength="32" placeholder="Participant name"></div>
      <div><label>Badges</label><input id="badges" maxlength="96" placeholder="Badge names"></div>
      <div><label>Week</label><input id="week" inputmode="numeric" maxlength="3" placeholder="xxx"></div>
      <div><label>Of</label><input id="week_total" inputmode="numeric" maxlength="3" value="999"></div>
      <div><label>Picture</label><input id="photo" type="file" accept="image/*" capture="environment"></div>
      <div><label>GPS</label><input id="gps_text" readonly placeholder="not requested"></div>
    </div>
    <button id="gps">Request GPS</button>
    <button id="save">Save Picture + Log</button>
    <a id="download" class="hidden" download="tempcontrol-proof.png">Download Again</a>
    <div id="status" class="status">Open this page, take a picture, request GPS, then save.</div>
    <canvas id="canvas" class="hidden"></canvas>
  </div>
</div>
<script>
const $=s=>document.querySelector(s);
let latestTemp=null,latestHumidity=null,photoImg=null,loc=null;
function fmtSeconds(s){const m=Math.floor(s/60),sec=s%60;return m+':'+String(sec).padStart(2,'0')}
async function pollTemp(){
  try{
    const r=await fetch('/api/temp');
    const d=await r.json();
    if(d.ok){latestTemp=d.temp_c;latestHumidity=d.humidity;$('#temp').textContent=d.temp_c.toFixed(1)+' C';$('#humidity').textContent=d.humidity.toFixed(1)+' %RH';}
  }catch(_){}
}
async function pollTimer(){
  try{
    const r=await fetch('/api/timer/status');
    const d=await r.json();
    $('#ring_status').textContent=d.mode||'unknown';
    $('#ring_remaining').textContent=fmtSeconds(d.remaining_seconds||0);
    $('#ring_phase').textContent=d.phase||'ready';
  }catch(_){}
}
function status(t){$('#status').textContent=t}
$('#photo').addEventListener('change',()=>{
  const f=$('#photo').files[0]; if(!f)return;
  const img=new Image();
  img.onload=()=>{photoImg=img;URL.revokeObjectURL(img.src);status('Picture loaded.');};
  img.onerror=()=>status('Could not load picture.');
  img.src=URL.createObjectURL(f);
});
$('#gps').addEventListener('click',()=>{
  if(!navigator.geolocation){status('GPS is not available.');return;}
  status('Requesting GPS permission...');
  navigator.geolocation.getCurrentPosition(p=>{
    loc={lat:p.coords.latitude,lon:p.coords.longitude,acc:p.coords.accuracy};
    $('#gps_text').value=loc.lat.toFixed(6)+', '+loc.lon.toFixed(6)+' +/- '+Math.round(loc.acc)+'m';
    status('GPS captured.');
  },e=>{loc=null;$('#gps_text').value='unavailable';status('GPS unavailable: '+e.message);},{enableHighAccuracy:true,timeout:12000,maximumAge:30000});
});
function meta(){
  const now=new Date();
  return {
    name:($('#name').value||'Participant').trim(),
    badges:($('#badges').value||'None').trim(),
    week:($('#week').value||'xxx').trim(),
    week_total:($('#week_total').value||'999').trim(),
    temp:latestTemp===null?'-- C':latestTemp.toFixed(1)+' C',
    humidity:latestHumidity===null?'-- %RH':latestHumidity.toFixed(1)+' %RH',
    when:now.toLocaleString(),
    iso:now.toISOString(),
    location:loc?loc.lat.toFixed(6)+', '+loc.lon.toFixed(6):($('#gps_text').value||'not captured')
  };
}
function draw(){
  if(!photoImg){status('Choose or take a picture first.');return null;}
  const c=$('#canvas'),scale=Math.min(1,1600/photoImg.naturalWidth);
  c.width=Math.round(photoImg.naturalWidth*scale);c.height=Math.round(photoImg.naturalHeight*scale);
  const ctx=c.getContext('2d');ctx.drawImage(photoImg,0,0,c.width,c.height);
  const m=meta(),pad=Math.max(24,Math.round(c.width*.025)),lh=Math.max(28,Math.round(c.width*.028)),fs=Math.max(22,Math.round(c.width*.024));
  const lines=['Name: '+m.name,'Week: '+m.week+'/'+m.week_total,'Badges: '+m.badges,'Temperature: '+m.temp+' | Humidity: '+m.humidity,'Date/Time: '+m.when,'GPS: '+m.location];
  const h=pad*2+lh*lines.length;ctx.fillStyle='rgba(46,93,116,.86)';ctx.fillRect(0,c.height-h,c.width,h);ctx.fillStyle='#fff';ctx.font='600 '+fs+'px Arial';ctx.textBaseline='top';
  lines.forEach((line,i)=>ctx.fillText(line,pad,c.height-h+pad+i*lh));c.classList.remove('hidden');
  return {m,data:c.toDataURL('image/png')};
}
async function log(m){
  const body=new URLSearchParams({name:m.name,badges:m.badges,week:m.week,week_total:m.week_total,temp:m.temp,humidity:m.humidity,captured_at:m.iso,location:m.location,latitude:loc?String(loc.lat):'',longitude:loc?String(loc.lon):''});
  const r=await fetch('/api/login',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body});
  return r.ok;
}
$('#save').addEventListener('click',async()=>{
  const p=draw(); if(!p)return;
  const safe=p.m.name.toLowerCase().replace(/[^a-z0-9]+/g,'-').replace(/^-|-$/g,'')||'participant';
  const a=$('#download');a.href=p.data;a.download='tempcontrol-'+safe+'-'+Date.now()+'.png';a.classList.remove('hidden');a.click();
  try{status(await log(p.m)?'Picture saved and login logged to SD.':'Picture saved. SD logging failed.');}catch(e){status('Picture saved. SD logging failed: '+e.message);}
});
$('#timer_start').addEventListener('click',async()=>{try{await fetch('/api/timer/start',{method:'POST'});pollTimer();}catch(_){}});
$('#timer_stop').addEventListener('click',async()=>{try{await fetch('/api/timer/stop',{method:'POST'});pollTimer();}catch(_){}});
pollTemp();setInterval(pollTemp,2000);pollTimer();setInterval(pollTimer,1000);
</script>
</body>
</html>
)rawliteral";

uint8_t crc8(const uint8_t *data, size_t length) {
  uint8_t crc = 0xFF;
  for (size_t i = 0; i < length; ++i) {
    crc ^= data[i];
    for (uint8_t bit = 0; bit < 8; ++bit) {
      crc = (crc & 0x80) ? static_cast<uint8_t>((crc << 1) ^ 0x31)
                         : static_cast<uint8_t>(crc << 1);
    }
  }
  return crc;
}

bool probeAddress(uint8_t address) {
  Wire.beginTransmission(address);
  return Wire.endTransmission() == 0;
}

uint8_t detectSensorAddress() {
  if (probeAddress(kPrimarySensorAddress)) {
    return kPrimarySensorAddress;
  }
  if (probeAddress(kAlternateSensorAddress)) {
    return kAlternateSensorAddress;
  }
  return 0;
}

bool readSht31(float &temperatureC, float &humidityRh) {
  if (activeSensorAddress == 0) {
    return false;
  }

  Wire.beginTransmission(activeSensorAddress);
  Wire.write(kSingleShotHighRepeatability[0]);
  Wire.write(kSingleShotHighRepeatability[1]);
  if (Wire.endTransmission() != 0) {
    return false;
  }

  delay(20);

  uint8_t data[6] = {};
  const size_t received = Wire.requestFrom(static_cast<int>(activeSensorAddress), 6);
  if (received != sizeof(data)) {
    while (Wire.available()) {
      Wire.read();
    }
    return false;
  }

  for (uint8_t &byte : data) {
    byte = static_cast<uint8_t>(Wire.read());
  }

  if (crc8(&data[0], 2) != data[2] || crc8(&data[3], 2) != data[5]) {
    return false;
  }

  const uint16_t temperatureRaw = (static_cast<uint16_t>(data[0]) << 8) | data[1];
  const uint16_t humidityRaw = (static_cast<uint16_t>(data[3]) << 8) | data[4];
  temperatureC = -45.0f + (175.0f * static_cast<float>(temperatureRaw) / 65535.0f);
  humidityRh = 100.0f * static_cast<float>(humidityRaw) / 65535.0f;
  return true;
}

void sendCors() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "GET,POST,OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
}

void sendOptions() {
  sendCors();
  server.send(204);
}

String jsonEscape(const String &value) {
  String out;
  out.reserve(value.length() + 8);
  for (size_t i = 0; i < value.length(); ++i) {
    const char c = value[i];
    if (c == '"' || c == '\\') {
      out += '\\';
      out += c;
    } else if (c == '\n') {
      out += "\\n";
    } else if (c == '\r') {
      out += "\\r";
    } else {
      out += c;
    }
  }
  return out;
}

String csvEscape(String value) {
  value.replace("\"", "\"\"");
  return "\"" + value + "\"";
}

uint32_t scaledColor(uint8_t red, uint8_t green, uint8_t blue, uint8_t scale) {
  return ring.Color((static_cast<uint16_t>(red) * scale) / 255,
                    (static_cast<uint16_t>(green) * scale) / 255,
                    (static_cast<uint16_t>(blue) * scale) / 255);
}

void clearRing() {
  ring.clear();
  ring.show();
}

void setAllPixels(uint32_t color) {
  for (uint16_t i = 0; i < kNeoPixelCount; ++i) {
    ring.setPixelColor(i, color);
  }
  ring.show();
}

void setThreeBlinkPixels(uint32_t color) {
  ring.clear();
  ring.setPixelColor(0, color);
  ring.setPixelColor(kNeoPixelCount / 3, color);
  ring.setPixelColor((2 * kNeoPixelCount) / 3, color);
  ring.show();
}

const char *ringModeName() {
  switch (ringMode) {
    case RingMode::Idle:
      return "idle";
    case RingMode::Prestart:
      return "prestart";
    case RingMode::Running:
      return "running";
    case RingMode::FinishBlink:
      return "finished";
  }
  return "unknown";
}

uint32_t ringRemainingMs(uint32_t now) {
  if (ringMode == RingMode::Running) {
    const uint32_t elapsed = now - ringCountdownStartedMs;
    return elapsed >= kBreathingCountdownMs ? 0 : kBreathingCountdownMs - elapsed;
  }
  if (ringMode == RingMode::Prestart) {
    return kBreathingCountdownMs;
  }
  return 0;
}

const char *breathPhaseName(uint32_t elapsedMs) {
  const uint8_t phase = (elapsedMs / kBreathPhaseMs) % 3;
  if (phase == 0) {
    return "inhale";
  }
  if (phase == 1) {
    return "hold";
  }
  return "exhale";
}

uint32_t breathPhaseColor(uint32_t elapsedMs) {
  const uint8_t phase = (elapsedMs / kBreathPhaseMs) % 3;
  const uint32_t phaseElapsed = elapsedMs % kBreathPhaseMs;
  if (phase == 0) {
    const uint8_t level = 40 + (phaseElapsed * 215UL) / kBreathPhaseMs;
    return scaledColor(110, 255, 110, level);
  }
  if (phase == 1) {
    return scaledColor(255, 170, 0, 220);
  }
  const uint8_t level = 255 - (phaseElapsed * 175UL) / kBreathPhaseMs;
  return scaledColor(255, 0, 0, level);
}

void drawCountdownRing(uint32_t now) {
  const uint32_t elapsed = now - ringCountdownStartedMs;
  const uint32_t remaining = ringRemainingMs(now);
  if (remaining == 0) {
    ringMode = RingMode::FinishBlink;
    ringFinishStartedMs = now;
    setAllPixels(ring.Color(255, 0, 0));
    return;
  }

  const uint16_t litPixels =
      static_cast<uint16_t>(((uint64_t)remaining * kNeoPixelCount + kBreathingCountdownMs - 1) /
                            kBreathingCountdownMs);
  const uint32_t color = breathPhaseColor(elapsed);
  ring.clear();
  for (uint16_t i = 0; i < litPixels && i < kNeoPixelCount; ++i) {
    ring.setPixelColor(i, color);
  }
  ring.show();
}

void startRingTimer() {
  ringMode = RingMode::Prestart;
  ringPrestartStartedMs = millis();
  ringCountdownStartedMs = 0;
  ringFinishStartedMs = 0;
  lastRingFrameMs = 0;
  setThreeBlinkPixels(ring.Color(255, 255, 255));
}

void stopRingTimer() {
  ringMode = RingMode::Idle;
  ringCountdownStartedMs = 0;
  ringFinishStartedMs = 0;
  clearRing();
}

void updateRing() {
  const uint32_t now = millis();
  if (now - lastRingFrameMs < kRingFrameMs) {
    return;
  }
  lastRingFrameMs = now;

  if (ringMode == RingMode::Idle) {
    return;
  }

  if (ringMode == RingMode::Prestart) {
    const uint32_t elapsed = now - ringPrestartStartedMs;
    const uint8_t step = elapsed / kBlinkHalfPeriodMs;
    if (step >= kFinishBlinkCount * 2) {
      ringMode = RingMode::Running;
      ringCountdownStartedMs = now;
      drawCountdownRing(now);
      return;
    }
    if (step % 2 == 0) {
      setThreeBlinkPixels(ring.Color(255, 255, 255));
    } else {
      clearRing();
    }
    return;
  }

  if (ringMode == RingMode::Running) {
    drawCountdownRing(now);
    return;
  }

  if (ringMode == RingMode::FinishBlink) {
    const uint32_t elapsed = now - ringFinishStartedMs;
    const uint8_t step = elapsed / kBlinkHalfPeriodMs;
    if (step >= kFinishBlinkCount * 2) {
      stopRingTimer();
      return;
    }
    if (step % 2 == 0) {
      setAllPixels(ring.Color(255, 0, 0));
    } else {
      clearRing();
    }
  }
}

void pollSensorIfDue(bool force = false) {
  const uint32_t now = millis();
  if (!force && now - lastSensorPollMs < kSensorPollMs) {
    return;
  }
  lastSensorPollMs = now;

  if (activeSensorAddress == 0) {
    activeSensorAddress = detectSensorAddress();
  }

  float temperatureC = 0.0f;
  float humidityRh = 0.0f;
  lastSensorOk = readSht31(temperatureC, humidityRh);
  if (lastSensorOk) {
    lastTemperatureC = temperatureC;
    lastHumidityRh = humidityRh;
  }
}

bool appendLoginCsv() {
  if (!sdReady) {
    return false;
  }

  const bool needsHeader = !SD.exists(kLoginCsvPath);
  File file = SD.open(kLoginCsvPath, FILE_APPEND);
  if (!file) {
    return false;
  }

  if (needsHeader) {
    file.println("millis,captured_at,name,week,week_total,badges,temp,humidity,location,latitude,longitude");
  }

  file.print(millis());
  file.print(',');
  file.print(csvEscape(server.arg("captured_at")));
  file.print(',');
  file.print(csvEscape(server.arg("name")));
  file.print(',');
  file.print(csvEscape(server.arg("week")));
  file.print(',');
  file.print(csvEscape(server.arg("week_total")));
  file.print(',');
  file.print(csvEscape(server.arg("badges")));
  file.print(',');
  file.print(csvEscape(server.arg("temp")));
  file.print(',');
  file.print(csvEscape(server.arg("humidity")));
  file.print(',');
  file.print(csvEscape(server.arg("location")));
  file.print(',');
  file.print(csvEscape(server.arg("latitude")));
  file.print(',');
  file.println(csvEscape(server.arg("longitude")));
  file.close();
  return true;
}

void handleIndex() {
  sendCors();
  server.send_P(200, "text/html", kIndexHtml);
}

void handleTemp() {
  pollSensorIfDue(true);
  sendCors();
  if (!lastSensorOk) {
    server.send(503, "application/json", "{\"ok\":false,\"error\":\"sensor read failed\"}");
    return;
  }
  String json = "{\"ok\":true,\"temp_c\":";
  json += String(lastTemperatureC, 2);
  json += ",\"humidity\":";
  json += String(lastHumidityRh, 2);
  json += ",\"ts_ms\":";
  json += String(millis());
  json += "}";
  server.send(200, "application/json", json);
}

void handleLogin() {
  const bool saved = appendLoginCsv();
  sendCors();
  if (!saved) {
    server.send(500, "application/json", "{\"ok\":false,\"error\":\"sd write failed\"}");
    return;
  }
  String json = "{\"ok\":true,\"path\":\"";
  json += jsonEscape(kLoginCsvPath);
  json += "\"}";
  server.send(200, "application/json", json);
}

void handleTimerStatus() {
  const uint32_t now = millis();
  const uint32_t remainingMs = ringRemainingMs(now);
  const uint32_t remainingSeconds = (remainingMs + 999) / 1000;
  const char *phase = "ready";
  if (ringMode == RingMode::Prestart) {
    phase = "3 blink start";
  } else if (ringMode == RingMode::Running) {
    phase = breathPhaseName(now - ringCountdownStartedMs);
  } else if (ringMode == RingMode::FinishBlink) {
    phase = "3 red blinks";
  }

  String json = "{\"ok\":true,\"mode\":\"";
  json += ringModeName();
  json += "\",\"phase\":\"";
  json += phase;
  json += "\",\"remaining_seconds\":";
  json += String(remainingSeconds);
  json += ",\"duration_seconds\":180,\"led_count\":";
  json += String(kNeoPixelCount);
  json += "}";
  sendCors();
  server.send(200, "application/json", json);
}

void handleTimerStart() {
  startRingTimer();
  handleTimerStatus();
}

void handleTimerStop() {
  stopRingTimer();
  handleTimerStatus();
}
}  // namespace

void setup() {
  Serial.begin(115200);
  delay(1000);

  Wire.begin(kI2cSdaPin, kI2cSclPin, kI2cFrequencyHz);
  SPI.begin(kSdSckPin, kSdMisoPin, kSdMosiPin, kSdCsPin);
  sdReady = SD.begin(kSdCsPin, SPI);
  ring.begin();
  ring.setBrightness(kNeoPixelBrightness);
  clearRing();

  Serial.println();
  Serial.println("M5Stack Core2 TempControl capture server");
  Serial.printf("I2C: SDA GPIO%d, SCL GPIO%d, %lu Hz\n", kI2cSdaPin, kI2cSclPin,
                static_cast<unsigned long>(kI2cFrequencyHz));
  Serial.printf("NeoPixel ring: DI GPIO%d, LEDs %u\n", kNeoPixelPin, kNeoPixelCount);
  Serial.printf("SD: %s\n", sdReady ? "ready" : "not mounted");

  activeSensorAddress = detectSensorAddress();
  if (activeSensorAddress != 0) {
    Serial.printf("Detected SEN0385/SHT31 at I2C address 0x%02X\n", activeSensorAddress);
  } else {
    Serial.println("No SEN0385/SHT31 found at 0x44 or 0x45. Check wiring and power.");
  }

  WiFi.mode(WIFI_AP);
  WiFi.softAP(kApSsid, kApPassword);
  Serial.printf("Wi-Fi AP: %s / password: %s\n", kApSsid, kApPassword);
  Serial.printf("Open: http://%s/\n", WiFi.softAPIP().toString().c_str());

  server.on("/", HTTP_GET, handleIndex);
  server.on("/api/temp", HTTP_GET, handleTemp);
  server.on("/api/login", HTTP_POST, handleLogin);
  server.on("/api/login", HTTP_OPTIONS, sendOptions);
  server.on("/api/timer/status", HTTP_GET, handleTimerStatus);
  server.on("/api/timer/start", HTTP_POST, handleTimerStart);
  server.on("/api/timer/start", HTTP_OPTIONS, sendOptions);
  server.on("/api/timer/stop", HTTP_POST, handleTimerStop);
  server.on("/api/timer/stop", HTTP_OPTIONS, sendOptions);
  server.onNotFound([]() {
    sendCors();
    server.send(404, "application/json", "{\"ok\":false,\"error\":\"not found\"}");
  });
  server.begin();
  pollSensorIfDue(true);
}

void loop() {
  server.handleClient();
  pollSensorIfDue();
  updateRing();
  static uint32_t lastPrintMs = 0;
  if (millis() - lastPrintMs > 10000) {
    lastPrintMs = millis();
    if (lastSensorOk) {
      Serial.printf("Temperature: %.2f C | Humidity: %.2f %%RH | SD: %s\n", lastTemperatureC,
                    lastHumidityRh, sdReady ? "ready" : "not mounted");
    } else {
      Serial.printf("Sensor read failed | SD: %s\n", sdReady ? "ready" : "not mounted");
    }
  }
}
