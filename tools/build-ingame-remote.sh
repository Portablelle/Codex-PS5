#!/usr/bin/env bash
# Separate build directory; no console deployment or native title build.
set -euo pipefail
cd "$(dirname "$0")/.."
host=${PS5_BUILD_HOST:-vps}
remote=${PS5_INGAME_BUILD_DIRECTORY:-/home/ubuntu/codex-ps5-ingame}
[[ $remote =~ ^/[A-Za-z0-9_/-]+$ && $remote != / ]]
ssh "$host" "mkdir -p '$remote/src' '$remote/backend/compile-input' '$remote/platform' '$remote/vendor' '$remote/tools' '$remote/dist'"
rsync -az src/ "$host:$remote/src/"
rsync -az backend/ingame-service.cpp backend/assistant-service.c "$host:$remote/backend/"
rsync -az vendor/json.hpp "$host:$remote/vendor/"
rsync -az vendor/ps5-ai-cli/platform/sdk-spawn.h "$host:$remote/platform/"
rsync -az vendor/ps5-ai-cli/app/config.h "$host:$remote/backend/compile-input/"
rsync -az tools/build-ingame-linux.sh "$host:$remote/tools/"
mkdir -p build/ingame
printf '#define CODEX_PS5_VERSION "compile-check"\n#define CODEX_BUILD_ID "compile-check"\n' > build/ingame/service-version.h
rsync -az build/ingame/service-version.h "$host:$remote/backend/compile-input/"
ssh "$host" "sudo -n docker run --rm -v '$remote:/work' -w /work codex-ps5-rust:0.1 tools/build-ingame-linux.sh"
mkdir -p dist
rsync -az "$host:$remote/dist/ingame-service.elf" dist/
