#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build
${CXX:-clang++} -std=c++20 -O1 -Wall -Wextra -Wpedantic \
    src/protocol.cpp src/ingame.cpp tools/ingame-probe.cpp -o build/ingame-probe
exec build/ingame-probe
