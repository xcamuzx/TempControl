// Minimal Arduino.h stub for *host* compilation of the portable logic
// (challenge.h, breathing.h). The real Arduino core is only needed on the
// device; this lets the pure logic be unit-tested without it.
#pragma once
#include <cstdint>

static uint32_t __t = 0;            // fake monotonic clock for tests
inline uint32_t millis() { return __t; }
inline void delay(uint32_t) {}
