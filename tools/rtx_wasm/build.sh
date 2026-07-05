#!/usr/bin/env bash
#
# build.sh - RetroTrax-Replayer nach WebAssembly bauen (Phase 3).
#
# Voraussetzung: Emscripten (emsdk) aktiviert, z.B.:
#   source ~/emsdk/emsdk_env.sh
# Danach aus dem Repo-Wurzelverzeichnis ODER von ueberall:
#   tools/rtx_wasm/build.sh
#
# Erzeugt tools/rtx_wasm/rtx_wasm.js (+ .wasm). Der Player tools/rtx_wasm/player.html
# laedt beides. zlib kommt aus Emscriptens Port (-sUSE_ZLIB=1) fuer die
# Sample-Dekompression; reSIDfp wird aus den Quellen mitkompiliert (die .a ist
# nativ und nicht WASM-tauglich).

set -euo pipefail

# Repo-Wurzel = zwei Ebenen ueber diesem Skript.
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"

if ! command -v emcc >/dev/null 2>&1; then
    echo "FEHLER: 'emcc' nicht gefunden. Erst Emscripten aktivieren:" >&2
    echo "        source ~/emsdk/emsdk_env.sh" >&2
    exit 1
fi

# reSIDfp-Quellen (wie im CMake-residfp-Target: *.cpp/*.cc + resample/*.cpp).
RESIDFP=$(find libs/residfp -maxdepth 2 \( -name '*.cpp' -o -name '*.cc' \) | sort)
# TFMX-Decoder-Quellen (vendored, wie im CMake-tfmxdecoder-Target: rekursiv *.cpp).
TFMX=$(find libs/tfmxdecoder -name '*.cpp' | sort)

echo "emcc-Version: $(emcc --version | head -1)"
echo "reSIDfp-Dateien: $(echo "$RESIDFP" | wc -l)  TFMX-Dateien: $(echo "$TFMX" | wc -l)"

emcc -std=c++17 -O2 \
    -DRETROTRAX_NO_JUCE -DHAVE_CXX17 \
    -I src -I libs/residfp -I libs/tfmxdecoder \
    tools/rtx_wasm/rtx_wasm.cpp $RESIDFP $TFMX \
    -sUSE_ZLIB=1 \
    -sMODULARIZE=1 -sEXPORT_NAME=RtxModule \
    -sALLOW_MEMORY_GROWTH=1 \
    -sEXPORTED_FUNCTIONS='["_rtx_create","_rtx_destroy","_rtx_load_retrotrax","_rtx_render","_rtx_buffer","_rtx_frames","_rtx_sample_rate","_rtx_tfmx_render","_rtx_stream_start","_rtx_stream_start_tfmx","_rtx_stream_render","_rtx_stream_seek","_rtx_stream_ended","_rtx_estimate_seconds","_malloc","_free"]' \
    -sEXPORTED_RUNTIME_METHODS='["ccall","cwrap","HEAPF32","HEAPU8","FS"]' \
    -o tools/rtx_wasm/rtx_wasm.js

echo "OK -> tools/rtx_wasm/rtx_wasm.js (+ rtx_wasm.wasm)"
echo "Player: tools/rtx_wasm/player.html (ueber einen lokalen Webserver oeffnen,"
echo "        z.B.  cd tools/rtx_wasm && python3 -m http.server 8099  ->  http://localhost:8099/player.html )"
