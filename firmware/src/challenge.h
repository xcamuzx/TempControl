// Challenge state machine (port of ChallengeState in app/main.py).
//
// idle → running → finished → (ack) → idle. Transitions running→finished
// happen lazily in tick(), exactly like the Pi backend. Time is tracked with
// millis(); the signed-difference comparisons tolerate the ~49-day rollover.
#pragma once

#include <Arduino.h>

#include "config.h"

enum class Status { Idle, Running, Finished };

class Challenge {
public:
  Status status() const { return status_; }
  uint32_t durationS() const { return duration_s_; }
  uint32_t startedAt() const { return started_at_; }

  // Clamp to the hard caps, then start. Mirrors the 60..180 s backend limit.
  void start(uint32_t duration_s) {
    if (duration_s < MIN_DURATION_S) duration_s = MIN_DURATION_S;
    if (duration_s > MAX_DURATION_S) duration_s = MAX_DURATION_S;
    status_ = Status::Running;
    duration_s_ = duration_s;
    started_at_ = millis();
    ends_at_ = started_at_ + duration_s * 1000UL;
  }

  // Lazy transition: flip to finished once the deadline has passed.
  void tick() {
    if (status_ == Status::Running &&
        static_cast<int32_t>(millis() - ends_at_) >= 0) {
      status_ = Status::Finished;
    }
  }

  // Dismiss the finish overlay and return to idle.
  void ack() {
    if (status_ == Status::Finished) reset();
  }

  void reset() {
    status_ = Status::Idle;
    duration_s_ = 0;
    started_at_ = 0;
    ends_at_ = 0;
  }

  // Whole seconds remaining, rounded up (so the display hits 0:00 only at the
  // very end), clamped at 0.
  uint32_t remainingS() const {
    if (status_ != Status::Running) return 0;
    int32_t ms = static_cast<int32_t>(ends_at_ - millis());
    if (ms <= 0) return 0;
    return static_cast<uint32_t>((ms + 999) / 1000);
  }

private:
  Status status_ = Status::Idle;
  uint32_t duration_s_ = 0;
  uint32_t started_at_ = 0;
  uint32_t ends_at_ = 0;
};
