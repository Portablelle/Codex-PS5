#!/usr/bin/env bash
# Docker runs only on the remote build host. No console deployment.
set -euo pipefail
cd "$(dirname "$0")/.."
host=${PS5_BUILD_HOST:-vps}
remote=${PS5_BUILD_DIRECTORY:-/home/ubuntu/codex-ps5}
[[ $remote =~ ^/[A-Za-z0-9_/-]+$ && $remote != / ]]
# --delete mirrors the checkout: only sync into an empty, marked or earlier Codex PS5 directory.
ssh "$host" "mkdir -p '$remote' && cd '$remote' && { [ -e .codex-ps5-build ] || [ -e tools/build-native.sh ] || [ -z \"\$(ls -A)\" ] || exit 7; } && touch .codex-ps5-build && mkdir -p .deps dist" \
  || { status=$?; [ $status -ne 7 ] || echo "$host:$remote is not empty and not a Codex PS5 build directory; set PS5_BUILD_DIRECTORY to a dedicated one" >&2; exit $status; }
rsync -az --delete --exclude=.git --exclude=.deps --exclude=build --exclude=dist --exclude=/.codex-ps5-build ./ "$host:$remote/"
ssh "$host" "cd '$remote' && sudo -n docker run --rm -v '$remote:/work' -w /work opennow-ps5-builder:latest bash tools/build-native.sh"
mkdir -p dist
rsync -az "$host:$remote/dist/" dist/
