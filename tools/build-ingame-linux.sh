#!/usr/bin/env bash
# Run in the existing backend Docker image, whose SDK has a matching LLVM.
set -euo pipefail
[[ $(uname -s) == Linux ]] || { echo 'Run in the Linux backend builder' >&2; exit 2; }
cd "$(dirname "$0")/.."
sdk=${PS5_PAYLOAD_SDK:-/opt/ps5-payload-sdk}
mkdir -p dist
"$sdk/bin/prospero-clang++" -std=c++20 -O2 -Wall -Wextra -Werror -pthread \
    src/protocol.cpp src/ingame.cpp backend/ingame-service.cpp -o dist/ingame-service.elf
# Compile-check the updated backend without relinking/rebuilding the Codex engine.
"$sdk/bin/prospero-clang" -O2 -Wall -Wextra -Werror -Ibackend/compile-input \
    -c backend/assistant-service.c -o dist/assistant-service-check.o
