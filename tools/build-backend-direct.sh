#!/usr/bin/env bash
set -euo pipefail
[[ $(uname -s) == Linux ]] || { echo 'Run in the Linux PS5 toolchain runner' >&2; exit 2; }
cd "$(dirname "$0")/.."
python3 tools/prepare-backend.py --direct
cd .deps/backend
python3 tools/fetch-codex.py
python3 tools/fetch-loader.py
python3 tools/fetch-shell.py
python3 tools/prepare-mio.py
npm ci --prefix web --ignore-scripts --no-audit --no-fund
python3 tools/embed-ui.py
python3 tools/embed-launcher.py
export CARGO_BUILD_JOBS=${CARGO_BUILD_JOBS:-2}
export CARGO_TARGET_X86_64_UNKNOWN_LINUX_GNU_LINKER=clang
bash tools/check-codex.sh terminal-release
cp build/ps5-ai-cli.elf ../../dist/assistant-service.elf
