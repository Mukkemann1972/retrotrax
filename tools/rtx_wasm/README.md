# RetroTrax Web-Player (WebAssembly) — Phase 3

Spielt `.retrotrax`-Songs **direkt im Browser**, ohne Installation. Die JUCE-freie
Klang-Engine (TrackerEngine + reSIDfp) wird per Emscripten nach WebAssembly
gebaut; `player.html` rendert einen Song und spielt ihn über WebAudio.

## Dateien
- `rtx_wasm.cpp` — schlanke C-API (create / load / render / buffer) um die Engine.
- `native_test.cpp` — prüft dieselbe C-API **nativ mit g++** (ohne Emscripten).
- `build.sh` — baut `rtx_wasm.js` + `rtx_wasm.wasm` mit `emcc`.
- `player.html` — Browser-Player (Datei wählen → abspielen).

## Nativ prüfen (jederzeit, ohne Emscripten)
```bash
g++ -std=c++17 -O2 -DRETROTRAX_NO_JUCE -DHAVE_CXX17 -I src -I libs/residfp \
    tools/rtx_wasm/rtx_wasm.cpp tools/rtx_wasm/native_test.cpp \
    build/libresidfp.a -lpthread -lz -o build/rtx_wasm_test
./build/rtx_wasm_test tools/rtx_cli/test_song.retrotrax
```
(Verifiziert: Synth- und Sample-Songs rendern identisch zur CLI.)

## WASM bauen (Emscripten nötig)
```bash
source ~/emsdk/emsdk_env.sh      # emsdk einmalig installieren + aktivieren
tools/rtx_wasm/build.sh
cd tools/rtx_wasm && python3 -m http.server 8099
# Browser: http://localhost:8099/player.html
```

## Stand / offen
- MVP: **`.retrotrax` (Synth + Sample)** im Browser. Rendert den ganzen Song in
  einen Puffer und spielt ihn (einfach + robust).
- Noch offen: TFMX im Browser (der vendored Decoder lädt per Dateipfad → braucht
  Emscriptens virtuelles Dateisystem/MEMFS), und echtes Streaming per AudioWorklet
  statt Vorab-Render. Beides bewusst als Folgeschritt.
