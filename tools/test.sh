#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build/tests
${CXX:-clang++} -std=c++20 -O1 -Wall -Wextra -Wpedantic src/protocol.cpp tests/protocol_test.cpp -o build/tests/protocol
build/tests/protocol
${CXX:-clang++} -std=c++20 -O1 -Wall -Wextra -Wpedantic tests/markdown_test.cpp -o build/tests/markdown
build/tests/markdown
