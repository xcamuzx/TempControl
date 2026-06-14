// SHT3x temperature/humidity driver (port of app/sensor.py).
//
// Single-shot, high-repeatability read (command 0x2C06), CRC-8 checked, with
// the same 3-attempt retry/backoff as the Python driver. Uses the Arduino
// Wire library on the Core2's external Port A bus.
#pragma once

#include <cstdint>

struct Reading {
  float temp_c;
  float humidity;
};

class SHT3x {
public:
  explicit SHT3x(uint8_t address = 0x44, uint8_t retries = 3)
      : addr_(address), retries_(retries) {}

  // Brings up the Wire bus on the configured Port A pins. Call once in setup().
  void begin();

  // Returns true and fills `out` on success; false after all retries fail.
  bool read(Reading& out);

private:
  bool readOnce(Reading& out);

  uint8_t addr_;
  uint8_t retries_;
};
