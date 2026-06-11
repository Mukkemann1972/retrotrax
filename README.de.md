# Mukkemann RetroTrax

**Der Tracker mit Herz: Amiga-Gefühl + C64-SID, für heute gebaut.**

🇬🇧 [English version](README.md)

**[⬇ Download (Windows / macOS / Linux)](https://github.com/Mukkemann1972/retrotrax/releases/latest)**

Ein VST3-Plugin und Standalone-Programm im Stil von ProTracker, FastTracker II und OctaMED —
aber modern, anfängerfreundlich und für Windows, macOS und Linux.
Ein Projekt aus dem Mukkemann-Universum.

## Stand: Etappe 1 (v0.1) — Sampler

- 8 Spuren, 64 Zeilen Pattern (Spuren-Anzahl wird später frei)
- 16 Instrument-Slots, lädt WAV / AIFF / FLAC / OGG / MP3
- **ST-XX Sample-Disks eingebaut:** die legendären Amiga-Sample-Disketten
  (ST-01 bis ST-XX, 96 Disketten / ~5.900 Sounds, Public Domain) durchstöbern
  und per Klick laden — die Sounds kommen einzeln vom
  [Internet Archive](https://archive.org/details/AmigaSTXX_originals_plus_conversions)
  und werden lokal gecacht, das Plugin selbst bleibt klein
- Pattern-Grid im ProTracker-Look: Cursor-Zeile bleibt in der Mitte, das Pattern scrollt
- Tastatur als Klavier (deutsches QWERTZ-Layout)
- MIDI-Eingang zum Vorhören
- Läuft als VST3 in jeder DAW **und** als eigenständiges Programm

## Fahrplan

- **Etappe 2:** Echte C64-SID-Emulation (reSIDfp), klassische Effekte
  (Arpeggio, Portamento, Vibrato, Sample-Offset …), MOD/XM-Import, mehrere Patterns + Song-Reihenfolge,
  **CLAP-Format** (zusätzlich zu VST3, via clap-juce-extensions)
- **Etappe 3:** Beliebig viele Spuren, die Filter der alten Sampler
  (Akai S900/S950/S1000 inkl. 12-Bit-Charakter, Emulator, Ensoniq …),
  16er-Drumpad, **Fairlight-Zeichentool** (Wellenformen mit der Maus malen wie mit dem Lichtgriffel),
  Anfänger-Modus

## Bedienung

| Taste | Funktion |
|---|---|
| `Y X C V B N M` (+ `S D G H J` für Halbtöne) | Noten, aktuelle Oktave |
| `Q 2 W 3 E R 5 T 6 Z 7 U …` | Noten, eine Oktave höher |
| Pfeiltasten / Bild↑↓ / Pos1 / Ende | Cursor bewegen |
| `Tab` / `Shift+Tab` | Nächste / vorherige Spur |
| `Leertaste` | Play / Stop |
| `Entf` / `Rücktaste` | Zelle löschen |
| `+` / `-` | Oktave wechseln |
| Ziffern (auf Instrument-/Lautstärke-Spalte) | Wert eintippen |

## Selbst bauen

Benötigt: CMake ≥ 3.22, einen C++17-Compiler, unter Linux die üblichen
Audio/X11-Entwicklungspakete (`libasound2-dev`, `libx11-dev`, `libfreetype-dev`, …).

```bash
git clone --depth 1 --branch 8.0.8 https://github.com/juce-framework/JUCE.git libs/JUCE
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j2
```

Ergebnisse landen in `build/RetroTrax_artefacts/Release/`:
`Standalone/` (Programm) und `VST3/` (Plugin für die DAW).

## Unterstützen ❤️

RetroTrax ist und bleibt kostenlos. Wenn es dir Freude macht, kannst du mir
[auf Ko-fi einen Kaffee spendieren](https://ko-fi.com/mukkemann) — jede Tasse hält das Mukkemann-Universum am Laufen.

## Lizenz

GPL-3.0 — frei für alle.
