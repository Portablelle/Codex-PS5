#!/usr/bin/env bash
# The backend toolchain and Cargo build run on the remote Linux build host.
set -euo pipefail
cd "$(dirname "$0")/.."
host=${PS5_BUILD_HOST:-vps}
remote=${PS5_BUILD_DIRECTORY:-/home/ubuntu/codex-ps5}
[[ $remote =~ ^/[A-Za-z0-9_/-]+$ && $remote != / ]]
# --delete mirrors the checkout: only sync into an empty, marked or earlier Codex PS5 directory.
ssh "$host" "mkdir -p '$remote' && cd '$remote' && { [ -e .codex-ps5-build ] || [ -e tools/build-native.sh ] || [ -z \"\$(ls -A)\" ] || exit 7; } && touch .codex-ps5-build && mkdir -p .deps dist" \
  || { status=$?; [ $status -ne 7 ] || echo "$host:$remote is not empty and not a Codex PS5 build directory; set PS5_BUILD_DIRECTORY to a dedicated one" >&2; exit $status; }
rsync -az --delete --exclude=.git --exclude=.deps --exclude=build --exclude=dist --exclude=/.codex-ps5-build ./ "$host:$remote/"
ssh "$host" "cd '$remote' && python3 tools/prepare-backend.py && bash tools/build-backend-linux.sh"
mkdir -p dist
rsync -az "$host:$remote/dist/assistant-service.elf" dist/
rsync -az "$host:$remote/dist/service-build.json" dist/
