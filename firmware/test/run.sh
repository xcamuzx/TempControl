#!/usr/bin/env bash
# Host-compile and run the portable-logic unit tests. No hardware or M5
# toolchain required — uses the stub Arduino.h in ./stub.
set -euo pipefail
cd "$(dirname "$0")"
g++ -std=c++17 -Wall -Wextra -I stub host_logic_test.cpp -o host_logic_test
./host_logic_test
