#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build
${CXX:-clang++} -std=c++20 -O1 -DBOTTY_HOST_PREVIEW src/main.cpp src/renderer.cpp src/protocol.cpp src/app_exit.cpp tests/preview.cpp -o build/preview
build/preview
python3 - <<'PY'
from PIL import Image
Image.open('build/preview.ppm').save('build/preview.png')
PY
