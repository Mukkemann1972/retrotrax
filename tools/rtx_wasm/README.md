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
    -I libs/tfmxdecoder \
    tools/rtx_wasm/rtx_wasm.cpp tools/rtx_wasm/native_test.cpp \
    build/libresidfp.a build/libtfmxdecoder.a -lpthread -lz -o build/rtx_wasm_test
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
- **`.retrotrax` (Synth + Sample) und TFMX** spielen im Browser (TFMX über
  Emscriptens virtuelles Dateisystem/MEMFS, beide Dateien mdat.* + smpl.*).
- **Streaming per AudioWorklet (Phase 4):** Der Song startet sofort — die
  Engine rendert Häppchen (`rtx_stream_*`-API), ein Worklet-FIFO spielt sie
  lückenlos; gespult wird per Neustart + stummem Vorspulen. Gerendert wird mit
  der Abtastrate des AudioContext (kein Resampling nötig). Browser ohne
  AudioWorklet bekommen automatisch den alten Komplett-Render als Rückfall.
- Verifiziert: `native_test.cpp` beweist, dass gestreamte Häppchen **bit-genau**
  dem Komplett-Render entsprechen (inkl. Seek); dazu Node-Lauf der echten
  rtx_wasm.js. Die Längen-Schätzung (`rtx_estimate_seconds`) trifft auf ~0,1 s,
  Tempo-Effekte im Song können sie verschieben — der Player korrigiert am Ende.
- Noch offen (Ideen): Demo-Songs fest auf der Seite, MOD/XM/S3M-Wiedergabe,
  Rendern in einen Worker auslagern (falls schwache Handys beim Nachschub ruckeln).
