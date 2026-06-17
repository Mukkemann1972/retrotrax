# Mukkemann RetroTrax

**The tracker with heart: Amiga feeling + C64 SID, built for today.**

🇩🇪 [Deutsche Version / German version](README.de.md)

**[⬇ Download (Windows / macOS / Linux)](https://github.com/Mukkemann1972/retrotrax/releases/latest)**

A VST3/CLAP plugin and standalone app in the spirit of ProTracker, FastTracker II and
OctaMED — but modern, beginner-friendly, and available for Windows, macOS and Linux.
A project from the Mukkemann universe.

## Status: v0.22 — Sampler + SID synthesizer (real reSIDfp chip)

- **🎹 SID synthesizer:** every slot can be a sound you create yourself instead
  of a sample — a C64 SID-style voice with waveforms (triangle / saw / pulse with
  pulse width / noise), an **ADSR envelope** and a multimode **filter**
  (low-/high-/band-pass with cutoff + resonance). Mix samples and SID freely in
  one song; every change plays a test note right away
- **Switchable sound engine:** choose per instrument between **Classic** (the
  built-in synth, the familiar RetroTrax sound) and **Real Chip** — an authentic
  **reSIDfp** emulation of the MOS 6581. Same controls, your pick of sound
- **PWM, ring modulation & hard sync:** the classic SID modulation toolkit —
  sweeping pulse width, metallic ring tones and screaming sync leads
- **Unison stack:** stack 1–3 slightly detuned voices per note for a fat, wide
  sound (super-saw / multi-SID idea) — on the real chip this drives the 3 actual
  hardware SID voices
- **Chord from one note:** a single key press plays a whole chord — **major**,
  **minor**, **sus4**, **fifth** (power chord) or **octave**. Uses the same stack
  voices as unison, and detune widens the chord further
- **Factory presets & your own sounds:** one click loads a ready-made SID sound
  (bass, lead, bell, drums, pad, sync lead, blip); **REMEMBER** saves your own
  creations into a personal SID list that's ready in every song
- **Effect column:** arpeggio, slides, tone portamento, vibrato, volume and tempo
  (classic tracker effects as hex input) with context-sensitive live help
- **Song mode:** chain several patterns (up to 64) into a whole song
- **Block editing:** select areas, copy/cut and move them directly with Alt+arrow
  (nudge by ear)
- **Note off (key 1):** lets SID voices fade out cleanly
- 16 tracks, 64-row patterns — they don't all fit at once, so the grid
  scrolls sideways with the cursor (a ‹/› arrow in the header means more tracks)
- **MOD import:** load classic Amiga `.mod` songs (LOAD MOD button) — samples go
  into the slots (up to 31), patterns + order are taken over. Play it right away,
  keep building and save as `.retrotrax`
- **Live recording:** hit play and play along on the keyboard — notes are recorded
  straight onto the row that's currently playing (in the cursor's track), like a
  captured piano performance. When stopped you type step by step as before
- **Amiga-style stereo:** tracks are spread across the stereo field automatically
  (LRRL like ProTracker) — your beat sounds wide and lively on its own; every
  note gets a tiny fade in/out so nothing clicks
- **Undo/redo** (Ctrl+Z / Ctrl+Y, up to 64 steps) and
  **copy/paste/cut track** (Ctrl+C / V / X)
- 16 instrument slots, loads WAV / AIFF / FLAC / OGG / MP3 plus the native
  Amiga format **8SVX / IFF** (openly documented)
- **SoundFonts (SF2):** add a `.sf2` bank (via + FOLDER), browse its sounds,
  preview and load them — the chosen sample is extracted to a WAV, so you can
  remember it and save it with your song too
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
- Runs as a VST3 **and** CLAP inside any DAW **and** as a standalone program

## Roadmap

- **Done in stage 2:** real C64 SID emulation (reSIDfp) alongside the built-in
  synth, ring modulation, hard sync, PWM, factory presets, your own SID sounds,
  the unison stack ✅, the **chord from one note** ✅ and **16 tracks with
  side-scrolling** ✅ (was 8), **MOD import** ✅, **XM import** ✅
  (FastTracker 2: more channels, 16-bit samples, finetune), the
  **CLAP format** ✅ (in addition to VST3, via clap-juce-extensions) and the
  **Akai sampler filter** ✅ (resonant low-pass in S900/S950/S1000 style +
  12-bit crunch, per sample), **sampler effects** ✅ (reverse +
  grain/sample-rate reduction), **loop** ✅ (forward + ping-pong) and
  **analog warmth** ✅ (drive saturation + vintage pitch like the old samplers)
- **Bigger goal:** rebuild the open **TFMX** format (Chris Hülsbeck)
- **Later:** 16-pad drum grid, **Fairlight-style drawing tool** (paint waveforms
  with the mouse, light-pen style), more sampler effects (time-stretch),
  .sid player + .retrotrax replayer for your own demos, beginner mode

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
# Optional for the CLAP format (otherwise it is simply skipped):
git clone --depth 1 --recurse-submodules --shallow-submodules https://github.com/free-audio/clap-juce-extensions.git libs/clap-juce-extensions
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j2
```

Results end up in `build/RetroTrax_artefacts/Release/`:
`Standalone/` (app), `VST3/` and `CLAP/` (plugins for your DAW).

## Support ❤️

RetroTrax is free and always will be. If it brings you joy, you can
[buy me a coffee on Ko-fi](https://ko-fi.com/mukkemann) — every cup keeps the Mukkemann universe running.

## License

GPL-3.0 — free for everyone.
