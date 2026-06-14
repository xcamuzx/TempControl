#include "sht3x.h"

#include <Arduino.h>
#include <Wire.h>

#include "config.h"

namespace {

// CRC-8, polynomial 0x31, init 0xFF — identical to the Python _crc8().
uint8_t crc8(const uint8_t* data, int len) {
  uint8_t crc = 0xFF;
  for (int i = 0; i < len; ++i) {
    crc ^= data[i];
    for (int b = 0; b < 8; ++b) {
      crc = (crc & 0x80) ? static_cast<uint8_t>((crc << 1) ^ 0x31)
                         : static_cast<uint8_t>(crc << 1);
    }
  }
  return crc;
}

}  // namespace

void SHT3x::begin() {
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL, I2C_HZ);
}

bool SHT3x::read(Reading& out) {
  for (uint8_t attempt = 0; attempt < retries_; ++attempt) {
    if (readOnce(out)) return true;
    delay(50 * (attempt + 1));  // incremental backoff: 50, 100, 150 ms
  }
  return false;
}

bool SHT3x::readOnce(Reading& out) {
  // Trigger a single high-repeatability measurement: command 0x2C 0x06.
  Wire.beginTransmission(addr_);
  Wire.write(0x2C);
  Wire.write(0x06);
  if (Wire.endTransmission() != 0) return false;

  delay(20);  // high-rep conversion completes well under 20 ms

  if (Wire.requestFrom(static_cast<int>(addr_), 6) != 6) return false;
  uint8_t d[6];
  for (int i = 0; i < 6; ++i) d[i] = Wire.read();

  // Two CRC-protected words: [temp_hi, temp_lo, crc][hum_hi, hum_lo, crc].
  if (crc8(d, 2) != d[2] || crc8(d + 3, 2) != d[5]) return false;

  const uint16_t t_raw = (static_cast<uint16_t>(d[0]) << 8) | d[1];
  const uint16_t h_raw = (static_cast<uint16_t>(d[3]) << 8) | d[4];
  out.temp_c = -45.0f + 175.0f * t_raw / 65535.0f;
  out.humidity = 100.0f * h_raw / 65535.0f;
  return true;
}
