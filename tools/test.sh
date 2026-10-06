#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build/tests
${CXX:-clang++} -std=c++20 -O1 -Wall -Wextra -Wpedantic src/protocol.cpp tests/protocol_test.cpp -o build/tests/protocol
build/tests/protocol
${CXX:-clang++} -std=c++20 -O1 -Wall -Wextra -Wpedantic tests/markdown_test.cpp -o build/tests/markdown
build/tests/markdown
${CXX:-clang++} -std=c++20 -O1 -Wall -Wextra -Wpedantic src/protocol.cpp tests/language_test.cpp -o build/tests/language
build/tests/language
${CXX:-clang++} -std=c++20 -O1 -Wall -Wextra -Wpedantic tests/keyboard_test.cpp -o build/tests/keyboard
build/tests/keyboard
${CXX:-clang++} -std=c++20 -O1 -Wall -Wextra -Wpedantic src/protocol.cpp src/ingame.cpp tests/ingame_test.cpp -o build/tests/ingame
build/tests/ingame
bash tools/build-ingame-host.sh
python3 tests/resident_service_test.py
