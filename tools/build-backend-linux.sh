#!/usr/bin/env bash
set -euo pipefail
[[ $(uname -s) == Linux ]] || { echo "Run this build on the remote Linux host" >&2; exit 2; }
cd "$(dirname "$0")/../.deps/backend"
sudo -n docker build -f Dockerfile.sdk -t codex-ps5-sdk:0.1 .
sudo -n docker build --build-arg PS5_SDK_IMAGE=codex-ps5-sdk:0.1 -t codex-ps5-rust:0.1 .
python3 tools/fetch-codex.py
python3 tools/fetch-loader.py
python3 tools/fetch-shell.py
python3 tools/prepare-mio.py
npm ci --prefix web --ignore-scripts --no-audit --no-fund
python3 tools/embed-ui.py
python3 tools/embed-launcher.py
sudo -n docker run --rm --memory=6g --memory-swap=6g -v "$PWD:/work" -e CARGO_BUILD_JOBS=2 \
    -e CARGO_TARGET_X86_64_UNKNOWN_LINUX_GNU_LINKER=clang \
    -e GIT_CONFIG_COUNT=3 \
    -e GIT_CONFIG_KEY_0=safe.directory -e GIT_CONFIG_VALUE_0=/work/vendor/dash \
    -e GIT_CONFIG_KEY_1=safe.directory -e GIT_CONFIG_VALUE_1=/work/vendor/sbase \
    -e GIT_CONFIG_KEY_2=safe.directory -e GIT_CONFIG_VALUE_2=/work/vendor/codex codex-ps5-rust:0.1 tools/check-codex.sh terminal-release
mkdir -p ../../dist
cp build/ps5-ai-cli.elf ../../dist/assistant-service.elf
