#!/usr/bin/env bash
# PS5 compile check (prospero-clang -Werror) for CI on the self-hosted runner, which
# has the codex-ps5-rust image. Same inputs as tools/build-ingame-remote.sh.
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p platform backend/compile-input
cp vendor/ps5-ai-cli/platform/sdk-spawn.h platform/
cp vendor/ps5-ai-cli/app/config.h backend/compile-input/
printf '#define CODEX_PS5_VERSION "compile-check"\n#define CODEX_BUILD_ID "compile-check"\n' \
    > backend/compile-input/service-version.h
docker run --rm --network none --user "$(id -u):$(id -g)" -v "$PWD:/work" -w /work \
    codex-ps5-rust:0.1 tools/build-ingame-linux.sh
