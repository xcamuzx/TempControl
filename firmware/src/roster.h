// Participant roster — pure, header-only, host-testable (no Arduino/M5 deps).
//
// Holds up to MAX names uploaded via the phase-2 WiFi page. Cleaning mirrors
// the Pi backend's StartRequest validation: trim surrounding whitespace, drop
// empties, cap length, keep at most MAX. A monotonic revision() lets the UI
// know when to repaint without diffing the contents.
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

class Roster {
public:
  static constexpr std::size_t MAX = 4;
  static constexpr std::size_t MAX_LEN = 32;

  // Replace the roster from raw inputs (e.g. web form fields).
  void setFrom(const std::vector<std::string>& raw) {
    count_ = 0;
    for (const std::string& r : raw) {
      if (count_ >= MAX) break;
      std::string n = trim(r);
      if (n.empty()) continue;
      if (n.size() > MAX_LEN) n = n.substr(0, MAX_LEN);
      names_[count_++] = n;
    }
    ++rev_;
  }

  void clear() {
    count_ = 0;
    ++rev_;
  }

  std::size_t size() const { return count_; }
  bool empty() const { return count_ == 0; }
  const std::string& operator[](std::size_t i) const { return names_[i]; }

  // Increments on every mutation; used by the render loop as a change flag.
  std::uint32_t revision() const { return rev_; }

private:
  static std::string trim(const std::string& s) {
    const char* ws = " \t\r\n";
    const std::size_t a = s.find_first_not_of(ws);
    if (a == std::string::npos) return "";
    const std::size_t b = s.find_last_not_of(ws);
    return s.substr(a, b - a + 1);
  }

  std::array<std::string, MAX> names_{};
  std::size_t count_ = 0;
  std::uint32_t rev_ = 0;
};
