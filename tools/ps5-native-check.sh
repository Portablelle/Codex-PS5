#!/usr/bin/env bash
# PS5 compile check of the native app (every src/ file, same flags as
# vendor/native-platform/tools/build.sh, plus -Werror) for CI on the self-hosted runner.
# No link: whisper.cpp, libc and the package are only built by tools/build-remote.sh.
set -euo pipefail
cd "$(dirname "$0")/.."
deps=build/ps5-native-check
sdk_url=https://github.com/ps5-payload-dev/sdk/releases/download/v0.42/ps5-payload-sdk.zip
sdk_hash=8cfbc7cd5811e719eb4f0c47eea668d3dc7b40bc8ab11c4a5031d40c23ec02da
whisper=2eeeba56e9edd762b4b38467bab96c2517163158  # v1.8.3, as in tools/build-native.sh
mkdir -p "$deps"
if [[ ! -x $deps/ps5-payload-sdk/bin/prospero-lld ]]; then
    curl -fsSL "$sdk_url" -o "$deps/sdk.zip"
    printf '%s  %s\n' "$sdk_hash" "$deps/sdk.zip" | sha256sum --check --strict --quiet
    unzip -q -o "$deps/sdk.zip" -d "$deps"
fi
if [[ ! -f $deps/whisper-include/whisper.h ]]; then
    curl -fsSL "https://codeload.github.com/ggml-org/whisper.cpp/tar.gz/$whisper" |
        tar -xz -C "$deps" --wildcards '*/include/whisper.h' '*/ggml/include/*.h'
    mkdir -p "$deps/whisper-include"
    cp "$deps/whisper.cpp-$whisper/include/whisper.h" "$deps/whisper.cpp-$whisper"/ggml/include/*.h \
        "$deps/whisper-include/"
fi
docker run --rm --network none --user "$(id -u):$(id -g)" -v "$PWD:/work" -w /work \
    -e PS5_PAYLOAD_SDK="/work/$deps/ps5-payload-sdk" -e PS5_CLANG=clang-18 -e USE_CCACHE=0 \
    -e DEPS="$deps" opennow-ps5-builder:latest bash -c '
status=0
# Same source set as build.sh: every C/C++ file under src/, recursively.
while IFS= read -r -d "" source; do
    if [[ $source == *.c ]]; then flags=(-std=c11); else flags=(-std=c++20 -fno-exceptions -fno-rtti); fi
    sh vendor/native-platform/tooling/prospero-clang18 "${flags[@]}" -O2 -Wall -Wextra -Werror \
        -ffunction-sections -fdata-sections -I"$DEPS/whisper-include" \
        -c "$source" -o /dev/null || { echo "FAILED: $source" >&2; status=1; }
done < <(find src -type f \( -name "*.c" -o -name "*.cc" -o -name "*.cpp" \) -print0 | sort -z)
exit $status'
