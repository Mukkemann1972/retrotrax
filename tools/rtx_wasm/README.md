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
- **Gepacktes `.rtx`** (siehe `src/rt_rtx.h`): der Player erkennt es an der
  Kennung `RTX1` und lädt es über `rtx_load_rtx` — sonst identischer Weg
  (Streaming inklusive). Der Demo-Song wird als `.rtx` geholt: ~15 statt ~44 KB
  über die Leitung, gleicher Klang. Packen mit
  `./build/rtx_cli pack song.retrotrax song.rtx`.
- Verifiziert: `native_test.cpp` beweist, dass gestreamte Häppchen **bit-genau**
  dem Komplett-Render entsprechen (inkl. Seek); dazu Node-Lauf der echten
  rtx_wasm.js. Die Längen-Schätzung (`rtx_estimate_seconds`) trifft auf ~0,1 s,
  Tempo-Effekte im Song können sie verschieben — der Player korrigiert am Ende.
  Für `.rtx` zusätzlich: `native_test.cpp <song.retrotrax> <song.rtx>` vergleicht
  beide Fassungen Wert für Wert, und der gestreamte `.rtx`-Weg wurde per Node
  gegen den XML-Komplett-Render geprüft — bit-identisch.
- **Demo-Songs** stehen in `songs/demos.json` (`[{ "file": "...", "title": "..." }]`).
  Neuen Song dazulegen = Datei nach `songs/` kopieren und eine Zeile eintragen;
  ab zwei Einträgen zeigt der Player ein Auswahlfeld neben dem Demo-Knopf. Fehlt
  die Liste, bleibt der eine fest verdrahtete Song übrig (Player läuft weiter).
- **MOD / XM / S3M / IT im Browser**: die Importer aus dem Plugin sind jetzt
  auch JUCE-frei baubar (`#ifdef RETROTRAX_NO_JUCE` wie bei TrackerEngine).
  Das Einraeumen in die Engine steht einmal in `src/rt_mod.h` und wird von
  Plugin und Replayer geteilt. C-API: `rtx_load_mod(handle, pfad, kind)` mit
  `kind` 0=automatisch/1=MOD/2=XM/3=S3M/4=IT; die Bytes legt der Player wie bei
  TFMX ins virtuelle Dateisystem. Der Player erkennt das Format an der Kennung
  (XM vorn, S3M "SCRM" bei 44, IT "IMPM" vorn, MOD-Signatur bei 1080).
- Noch offen (Ideen): Rendern in einen Worker auslagern (falls schwache Handys
  beim Nachschub ruckeln).
