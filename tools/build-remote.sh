#!/usr/bin/env bash
# Docker runs only on the remote build host. No console deployment.
set -euo pipefail
cd "$(dirname "$0")/.."
host=${PS5_BUILD_HOST:-vps}
remote=${PS5_BUILD_DIRECTORY:-/home/ubuntu/codex-ps5}
[[ $remote =~ ^/[A-Za-z0-9_/-]+$ && $remote != / ]]
ssh "$host" "mkdir -p '$remote/.deps' '$remote/dist'"
rsync -az --delete --exclude=.git --exclude=.deps --exclude=build --exclude=dist ./ "$host:$remote/"
ssh "$host" "cd '$remote' && sudo -n docker run --rm -v '$remote:/work' -w /work opennow-ps5-builder:latest bash tools/build-native.sh"
mkdir -p dist
rsync -az "$host:$remote/dist/" dist/
