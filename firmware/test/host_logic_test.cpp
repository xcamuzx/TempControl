// Host-side unit tests for the firmware's portable logic — the parts that do
// not depend on the M5/Arduino hardware libraries: the SHT3x CRC-8, the
// challenge state machine, and the box-breathing geometry.
//
// Build & run:  ./run.sh   (or see that script for the g++ invocation)
#include <cstdint>
#include <cstdio>
#include <cmath>
#include <string>

#include <vector>

#include "../src/config.h"
#include "../src/challenge.h"
#include "../src/breathing.h"
#include "../src/roster.h"

// CRC-8 copy of SHT3x::crc8 (poly 0x31, init 0xFF) to check driver parity.
static uint8_t crc8(const uint8_t* d, int n) {
  uint8_t c = 0xFF;
  for (int i = 0; i < n; i++) {
    c ^= d[i];
    for (int b = 0; b < 8; b++)
      c = (c & 0x80) ? (uint8_t)((c << 1) ^ 0x31) : (uint8_t)(c << 1);
  }
  return c;
}

static int fails = 0;
static void chk(const char* n, bool ok) {
  printf("  %-42s %s\n", n, ok ? "OK" : "FAIL");
  if (!ok) fails++;
}
static bool feq(float a, float b) { return std::fabs(a - b) < 0.01f; }

int main() {
  printf("CRC-8 (driver parity):\n");
  uint8_t v[2] = {0xBE, 0xEF};
  chk("CRC(0xBEEF)==0x92 (datasheet vector)", crc8(v, 2) == 0x92);

  printf("Challenge clamps + transitions:\n");
  Challenge ch;
  __t = 1000; ch.start(180);
  chk("start(180): running", ch.status() == Status::Running);
  chk("start(180): dur==180", ch.durationS() == 180);
  ch.start(999); chk("clamp 999 -> 180", ch.durationS() == 180);
  ch.reset(); ch.start(10); chk("clamp 10 -> 60", ch.durationS() == 60);

  printf("remainingS rounding (ceil):\n");
  __t = 1000; ch.reset(); ch.start(60);
  __t = 1000 + 0;     chk("t=0s     -> 60", ch.remainingS() == 60);
  __t = 1000 + 1;     chk("t=1ms    -> 60", ch.remainingS() == 60);
  __t = 1000 + 59001; chk("t=59.001 -> 1",  ch.remainingS() == 1);
  __t = 1000 + 59999; chk("t=59.999 -> 1",  ch.remainingS() == 1);
  __t = 1000 + 60000; chk("t=60s    -> 0",  ch.remainingS() == 0);

  printf("Lazy finish + ack:\n");
  __t = 1000 + 59999; ch.tick(); chk("pre-deadline: running", ch.status() == Status::Running);
  __t = 1000 + 60000; ch.tick(); chk("at deadline: finished", ch.status() == Status::Finished);
  ch.ack(); chk("ack -> idle", ch.status() == Status::Idle);
  chk("remaining when idle == 0", ch.remainingS() == 0);

  printf("Box-breathing geometry (cx=160, cy=150, h=50):\n");
  const float cx = 160, cy = 150, h = 50;
  BreathPoint a = breathAt(0, cx, cy, h);
  chk("t=0 at BL (110,200)", a.phase == 0 && feq(a.x, 110) && feq(a.y, 200));
  BreathPoint e0 = breathAt(3999, cx, cy, h), s1 = breathAt(4000, cx, cy, h);
  chk("phase0 end meets phase1 start (TL)",
      s1.phase == 1 && feq(s1.x, 110) && feq(s1.y, 100) &&
      std::fabs(e0.x - s1.x) < 0.1f && std::fabs(e0.y - s1.y) < 0.1f);
  BreathPoint s2 = breathAt(8000, cx, cy, h);
  chk("phase2 start at TR (210,100)", s2.phase == 2 && feq(s2.x, 210) && feq(s2.y, 100));
  BreathPoint s3 = breathAt(12000, cx, cy, h);
  chk("phase3 start at BR (210,200)", s3.phase == 3 && feq(s3.x, 210) && feq(s3.y, 200));
  BreathPoint loop = breathAt(16000, cx, cy, h);
  chk("loop closes back to BL", loop.phase == 0 && feq(loop.x, 110) && feq(loop.y, 200));
  chk("phase0 label INHALE", std::string(breathAt(0, cx, cy, h).label) == "INHALE");
  chk("phase2 label EXHALE", std::string(breathAt(8000, cx, cy, h).label) == "EXHALE");

  printf("Roster (phase 2 — name upload cleaning):\n");
  Roster r;
  chk("empty by default", r.empty() && r.size() == 0);
  // trims, drops empties/whitespace, keeps order, caps at MAX (4)
  r.setFrom({" Alice ", "", "Bob", "   ", "Carl", "Dave", "Eve"});
  chk("keeps 4 (over-MAX dropped)", r.size() == 4);
  chk("trimmed first name == 'Alice'", r[0] == "Alice");
  chk("order preserved (Bob, Carl, Dave)",
      r[1] == "Bob" && r[2] == "Carl" && r[3] == "Dave");
  // length cap at 32
  r.setFrom({std::string(40, 'x')});
  chk("name truncated to 32", r.size() == 1 && r[0].size() == 32);
  // revision increments on each mutation
  uint32_t before = r.revision();
  r.setFrom({"Sam"}); chk("revision bumps on setFrom", r.revision() == before + 1);
  r.clear(); chk("clear empties + bumps revision",
                 r.empty() && r.revision() == before + 2);
  // all-empty input yields an empty roster
  r.setFrom({"", "  ", "\t"}); chk("all-whitespace -> empty", r.empty());

  printf("\n%s (%d failure%s)\n", fails ? "*** FAILURES ***" : "ALL PASS",
         fails, fails == 1 ? "" : "s");
  return fails ? 1 : 0;
}
