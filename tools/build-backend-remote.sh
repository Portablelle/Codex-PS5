#!/usr/bin/env bash
# The backend toolchain and Cargo build run on the remote Linux build host.
set -euo pipefail
cd "$(dirname "$0")/.."
host=${PS5_BUILD_HOST:-vps}
remote=${PS5_BUILD_DIRECTORY:-/home/ubuntu/codex-ps5}
[[ $remote =~ ^/[A-Za-z0-9_/-]+$ && $remote != / ]]
ssh "$host" "mkdir -p '$remote/.deps' '$remote/dist'"
rsync -az --exclude=.git --exclude=.deps --exclude=build --exclude=dist ./ "$host:$remote/"
ssh "$host" "cd '$remote' && python3 tools/prepare-backend.py && bash tools/build-backend-linux.sh"
mkdir -p dist
rsync -az "$host:$remote/dist/assistant-service.elf" dist/
rsync -az "$host:$remote/dist/service-build.json" dist/
