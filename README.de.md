# Mukkemann RetroTrax

**Der Tracker mit Herz: Amiga-Gefühl + C64-SID, für heute gebaut.**

🇬🇧 [English version](README.md)

**[⬇ Download (Windows / macOS / Linux)](https://github.com/Mukkemann1972/retrotrax/releases/latest)**

Ein VST3-Plugin und Standalone-Programm im Stil von ProTracker, FastTracker II und OctaMED —
aber modern, anfängerfreundlich und für Windows, macOS und Linux.
Ein Projekt aus dem Mukkemann-Universum.

## Stand: Etappe 1 (v0.4) — Sampler

- 8 Spuren, 64 Zeilen Pattern (Spuren-Anzahl wird später frei)
- **Stereo-Klang im Amiga-Stil:** die Spuren werden automatisch im Stereobild
  verteilt (LRRL wie ProTracker) — der Beat klingt von allein breit und lebendig;
  jede Note wird kurz ein-/ausgeblendet, damit nichts knackt
- **Rückgängig/Wiederholen** (Strg+Z / Strg+Y, bis zu 64 Schritte) und
  **Spur kopieren/einfügen/ausschneiden** (Strg+C / V / X)
- 16 Instrument-Slots, lädt WAV / AIFF / FLAC / OGG / MP3 sowie das native
  Amiga-Format **8SVX / IFF** (offen dokumentiert)
- **ST-XX Sample-Disks eingebaut:** die legendären Amiga-Sample-Disketten
  (ST-01 bis ST-XX, 96 Disketten / ~5.900 Sounds, Public Domain) durchstöbern
  und per Klick laden — die Sounds kommen einzeln vom
  [Internet Archive](https://archive.org/details/AmigaSTXX_originals_plus_conversions)
  und werden lokal gecacht, das Plugin selbst bleibt klein
- **Sample-Browser mit Suche:** ein Suchfeld findet einen Sound über *alle*
  Disks und deine eigenen Ordner zugleich; ein Klick spielt ihn sofort vor
- **Eigene Ordner & Sammlung „Meine Sounds":** eigene Sample-Ordner von der
  Festplatte einbinden (bleiben nach Neustart erhalten) und Lieblings-Sounds
  per Knopf in einer persönlichen Sammlung merken
- **Songs speichern & öffnen:** deine Arbeit als `.retrotrax`-Datei sichern und
  jederzeit wieder öffnen — fehlende Samples werden gemeldet statt still zu fehlen
- **Eingebaute Hilfe (`?`-Knopf):** Themen zu jeder Funktion, in deiner Sprache,
  wächst mit dem Programm mit
- **Zweisprachig (Deutsch / Englisch):** jederzeit per DE/EN-Knopf umschaltbar;
  beim ersten Start nach der Systemsprache
- **Instrument-Farben:** jedes Instrument hat eine feste Farbe aus einer
  16er-Retro-Palette — die Noten leuchten im Pattern-Grid in der Farbe ihres
  Instruments, ein Farbpunkt neben der Instrument-Auswahl zeigt die aktuelle "Malfarbe"
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
| `Strg+Z` / `Strg+Y` | Rückgängig / Wiederholen |
| `Strg+C` / `Strg+V` / `Strg+X` | Spur kopieren / einfügen / ausschneiden |

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
