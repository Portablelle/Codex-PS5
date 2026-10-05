#!/usr/bin/env bash
# Run inside the Linux builder on the build host.
set -euo pipefail
[[ $(uname -s) == Linux ]] || { echo "Run this build on the remote Linux host" >&2; exit 2; }
cd "$(dirname "$0")/.."
root=$PWD
stage="$root/.deps/native-app"
mkdir -p "$stage"
cp -R vendor/native-platform/. "$stage/"
mkdir -p "$root/.deps/native" "$stage/.deps" "$stage/vendor" "$stage/backend"
cp -R src sce_sys assets "$stage/"
cp vendor/json.hpp vendor/ps5-pad.h "$stage/vendor/"
cp backend/audioin-stub.c "$stage/backend/"
# Share only this project's ignored dependency cache with the staged build.
ln -sfn "$root/.deps/native" "$stage/.deps/native"
bash "$stage/tools/setup-native-dependencies.sh"
export PS5_PAYLOAD_SDK="$root/.deps/native/ps5-payload-sdk" PS5_CLANG=clang-18 USE_CCACHE=0
whisper="$root/.deps/whisper"
if [[ ! -d "$whisper" ]]; then
    git clone --depth 1 --branch v1.8.3 https://github.com/ggml-org/whisper.cpp.git "$whisper"
fi
[[ $(git -C "$whisper" rev-parse HEAD) == 2eeeba56e9edd762b4b38467bab96c2517163158 ]]
cat > "$stage/tooling/whisper-cc" <<'WRAPPER'
#!/bin/sh
exec sh "$(dirname "$0")/prospero-clang18" "$@"
WRAPPER
chmod +x "$stage/tooling/whisper-cc"
python3 tools/prepare-whisper.py "$whisper" "$stage/.deps/whisper-source"
cmake -S "$stage/.deps/whisper-source" -B "$stage/.deps/whisper-ps5-build" -G Ninja \
    -DCMAKE_SYSTEM_NAME=FreeBSD -DCMAKE_SYSTEM_PROCESSOR=x86_64 \
    -DCMAKE_C_COMPILER="$stage/tooling/whisper-cc" -DCMAKE_CXX_COMPILER="$stage/tooling/whisper-cc" \
    -DCMAKE_CXX_FLAGS='-fPIC -ffunction-sections -fdata-sections -stdlib=libc++ -fexceptions -fcxx-exceptions' -DCMAKE_C_FLAGS='-fPIC -ffunction-sections -fdata-sections' \
    -DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_SHARED_LIBS=OFF -DGGML_NATIVE=OFF -DGGML_OPENMP=OFF \
    -DGGML_AVX=ON -DGGML_AVX2=ON -DGGML_FMA=ON -DGGML_F16C=ON \
    -DGGML_AVX512=OFF -DGGML_METAL=OFF -DGGML_BLAS=OFF \
    -DWHISPER_BUILD_TESTS=OFF -DWHISPER_BUILD_EXAMPLES=OFF -DWHISPER_BUILD_SERVER=OFF
cmake --build "$stage/.deps/whisper-ps5-build" -j "${BUILD_JOBS:-2}"
"$PS5_PAYLOAD_SDK/bin/prospero-clang" -shared -fPIC -Wl,-soname,libSceAudioIn.prx \
    "$stage/backend/audioin-stub.c" -o "$stage/.deps/audioin.so"
export APP_DEFINITIONS='' APP_INCLUDE_PATHS='.deps/whisper-include'
mkdir -p "$stage/.deps/whisper-include"
cp "$whisper/include/whisper.h" "$whisper"/ggml/include/*.h "$stage/.deps/whisper-include/"
export APP_STATIC_ARCHIVES='.deps/whisper-ps5-build/src/libwhisper.a .deps/whisper-ps5-build/ggml/src/libggml.a .deps/whisper-ps5-build/ggml/src/libggml-cpu.a .deps/whisper-ps5-build/ggml/src/libggml-base.a'
export APP_IMPORT_STUBS=.deps/audioin.so PACBREW_PACKAGES='' PACBREW_INCLUDE_PATHS='' PACBREW_STATIC_ARCHIVES=''
export APP_SOURCE_DIR=src APP_PARAM=sce_sys/param.json APP_SCE_SYS=sce_sys APP_ASSETS=assets
if [[ ! -f "$stage/runtime/libc.prx" ]]; then bash "$stage/tools/rebuild-libc.sh";fi
bash "$stage/tools/build.sh" Folder
mkdir -p dist
cp -R "$stage/dist/." dist/
