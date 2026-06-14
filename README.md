# Mukkemann RetroTrax

**The tracker with heart: Amiga feeling + C64 SID, built for today.**

🇩🇪 [Deutsche Version / German version](README.de.md)

**[⬇ Download (Windows / macOS / Linux)](https://github.com/Mukkemann1972/retrotrax/releases/latest)**

A VST3 plugin and standalone app in the spirit of ProTracker, FastTracker II and
OctaMED — but modern, beginner-friendly, and available for Windows, macOS and Linux.
A project from the Mukkemann universe.

## Status: Stage 1 (v0.5) — Sampler

- 8 tracks, 64-row patterns (track count becomes flexible later)
- **Amiga-style stereo:** tracks are spread across the stereo field automatically
  (LRRL like ProTracker) — your beat sounds wide and lively on its own; every
  note gets a tiny fade in/out so nothing clicks
- **Undo/redo** (Ctrl+Z / Ctrl+Y, up to 64 steps) and
  **copy/paste/cut track** (Ctrl+C / V / X)
- 16 instrument slots, loads WAV / AIFF / FLAC / OGG / MP3 plus the native
  Amiga format **8SVX / IFF** (openly documented)
- **Built-in ST-XX sample disks:** browse the legendary Amiga sample disks
  (ST-01 through ST-XX, 96 disks / ~5,900 sounds, public domain) and load any
  sound with one click — samples are fetched individually from the
  [Internet Archive](https://archive.org/details/AmigaSTXX_originals_plus_conversions)
  and cached locally, so the plugin itself stays small
- **Sample browser with search:** one search box finds a sound across *all*
  disks and your own folders at once; a click previews it instantly
- **Your own folders & "My Sounds" collection:** add sample folders from your
  hard drive (they survive restarts), and **REMEMBER** favourite sounds into a
  personal collection — inside the collection that button becomes **FORGET** and
  sends a sound back to the trash
- **Save & open songs:** store your work as a `.retrotrax` file and reopen it
  anytime — missing samples are reported instead of silently failing
- **Built-in help (`?` button):** topics for every feature, in your language,
  growing with the app
- **Bilingual (German / English):** switch any time with the DE/EN button;
  follows your system language on first start
- **Instrument colors:** every instrument gets a fixed color from a 16-color
  retro palette — notes light up in their instrument's color in the pattern
  grid, and a color dot next to the instrument selector shows your current "paint"
- Pattern grid in classic ProTracker style: the cursor row stays centered, the pattern scrolls
- Computer keyboard as piano
- MIDI input for previewing notes
- Runs as a VST3 inside any DAW **and** as a standalone program

## Roadmap

- **Stage 2:** Real C64 SID emulation (reSIDfp), classic effects
  (arpeggio, portamento, vibrato, sample offset …), MOD/XM import, multiple patterns + song order,
  **CLAP format** (in addition to VST3, via clap-juce-extensions)
- **Stage 3:** Unlimited tracks, filters of the classic samplers
  (Akai S900/S950/S1000 incl. 12-bit character, Emulator, Ensoniq …),
  16-pad drum grid, **Fairlight-style drawing tool** (paint waveforms with the mouse, light-pen style),
  beginner mode

## Controls

| Key | Function |
|---|---|
| `Z X C V B N M` row (+ `S D G H J` for semitones) | Notes, current octave |
| `Q 2 W 3 E R 5 T 6 Y 7 U …` | Notes, one octave up |
| Arrow keys / PgUp/PgDn / Home / End | Move cursor |
| `Tab` / `Shift+Tab` | Next / previous track |
| `Space` | Play / stop |
| `Del` / `Backspace` | Clear cell |
| `+` / `-` | Change octave |
| Digits (on instrument/volume column) | Type a value |
| `Ctrl+Z` / `Ctrl+Y` | Undo / redo |
| `Ctrl+C` / `Ctrl+V` / `Ctrl+X` | Copy / paste / cut track |

## Building from source

Requires: CMake ≥ 3.22, a C++17 compiler, and on Linux the usual
audio/X11 development packages (`libasound2-dev`, `libx11-dev`, `libfreetype-dev`, …).

```bash
git clone --depth 1 --branch 8.0.8 https://github.com/juce-framework/JUCE.git libs/JUCE
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j2
```

Results end up in `build/RetroTrax_artefacts/Release/`:
`Standalone/` (app) and `VST3/` (plugin for your DAW).

## Support ❤️

RetroTrax is free and always will be. If it brings you joy, you can
[buy me a coffee on Ko-fi](https://ko-fi.com/mukkemann) — every cup keeps the Mukkemann universe running.

## License

GPL-3.0 — free for everyone.
