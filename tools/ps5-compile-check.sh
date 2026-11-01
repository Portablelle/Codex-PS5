#!/usr/bin/env bash
# PS5 compile check (prospero-clang -Werror) for CI on a self-hosted runner. Same inputs
# as tools/build-ingame-remote.sh. PS5_CHECK_DIRECT=1 uses the toolchain runner's SDK in
# /opt/ps5-payload-sdk; otherwise the host needs the codex-ps5-rust image.
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p platform backend/compile-input
cp vendor/ps5-ai-cli/platform/sdk-spawn.h platform/
cp vendor/ps5-ai-cli/app/config.h backend/compile-input/
printf '#define CODEX_PS5_VERSION "compile-check"\n#define CODEX_BUILD_ID "compile-check"\n' \
    > backend/compile-input/service-version.h
if [[ ${PS5_CHECK_DIRECT:-} == 1 ]]; then
    bash tools/build-ingame-linux.sh
else
    docker run --rm --network none --user "$(id -u):$(id -g)" -v "$PWD:/work" -w /work \
        codex-ps5-rust:0.1 tools/build-ingame-linux.sh
fi
