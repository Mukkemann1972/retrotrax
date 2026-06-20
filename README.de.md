# Mukkemann RetroTrax

**Der Tracker mit Herz: Amiga-Gefühl + C64-SID, für heute gebaut.**

🇬🇧 [English version](README.md)

**[⬇ Download (Windows / macOS / Linux)](https://github.com/Mukkemann1972/retrotrax/releases/latest)**

Ein VST3-/CLAP-Plugin und Standalone-Programm im Stil von ProTracker, FastTracker II und OctaMED —
aber modern, anfängerfreundlich und für Windows, macOS und Linux.
Ein Projekt aus dem Mukkemann-Universum.

## Stand: v0.59 — Sampler + SID-Synthesizer (echter reSIDfp-Chip)

- **🎹 SID-Synthesizer:** jeder Slot kann statt eines Samples ein selbst
  erzeugter C64-SID-Klang sein — Wellenformen (Dreieck / Säge / Puls mit
  Pulsweite / Rauschen), **ADSR-Hüllkurve** und ein Multimode-**Filter**
  (Tief-/Hoch-/Bandpass mit Cutoff + Resonanz). Sample und SID lassen sich frei
  in einem Song mischen; jede Änderung spielt sofort einen Probe-Ton an
- **Umschaltbarer Klang-Motor:** pro Instrument zwischen **Klassisch** (der
  eingebaute Synth, der vertraute RetroTrax-Klang) und **Echter Chip** wählen —
  einer originalgetreuen **reSIDfp**-Emulation des MOS 6581. Gleiche Regler,
  deine Wahl des Klangs
- **PWM, Ring-Modulation & Hard-Sync:** der klassische SID-Modulationskasten —
  wabernde Pulsweite, metallische Ring-Töne und schreiende Sync-Leads
- **Unisono-Stack:** 1–3 leicht verstimmte Stimmen pro Note stapeln für einen
  fetten, breiten Klang (Super-Saw / Multi-SID) — beim echten Chip nutzt das die
  3 echten SID-Hardware-Stimmen
- **Akkord aus einer Note:** ein einziger Tastendruck spielt einen ganzen
  Akkord — **Dur**, **Moll**, **Sus4**, **Quinte** (Powerchord) oder **Oktave**.
  Nutzt dieselben Stapel-Stimmen wie Unisono, die Verstimmung verbreitert den
  Akkord zusätzlich
- **Werks-Presets & eigene Sounds:** ein Klick lädt einen fertigen SID-Klang
  (Bass, Lead, Glocke, Drums, Pad, Sync-Lead, Blip); **MERKEN** speichert deine
  eigenen Klänge in eine persönliche SID-Liste, die in jedem Song bereitsteht
- **Effekt-Spalte:** Arpeggio, Slides, Tone-Portamento, Vibrato, Lautstärke und
  Tempo (klassische Tracker-Effekte als Hex-Eingabe) mit kontextsensitiver Live-Hilfe
- **Song-Modus:** mehrere Patterns (bis zu 64) zu einem ganzen Lied verketten
- **Block-Bearbeitung:** Bereiche markieren, kopieren/ausschneiden und per
  Alt+Pfeil direkt verschieben (gehörgenaues Nudgen)
- **Note-Aus (Taste 1):** lässt SID-Stimmen sauber ausklingen
- 16 Spuren, 64 Zeilen Pattern — passt nicht alles ins Fenster, das Grid scrollt
  seitlich mit dem Cursor mit (Pfeil ‹/› im Kopf = mehr Spuren da)
- **Stumm & Solo pro Spur:** kleine **M**- (stumm) und **S**- (solo) Knöpfe unter
  jedem Spurnamen — per Mausklick, auch während der Wiedergabe. Solo lässt nur die
  gewählten Spuren hören; so findest du sofort, was im Mix nicht passt
- **WAV-Export:** der **WAV**-Knopf rendert deinen ganzen Song in eine Stereo-WAV
  — fertig zum Teilen, Hochladen (Ko-fi/YouTube) oder Weiterverarbeiten
- **Spektrum-Anzeige:** der **SPEKTRUM**-Knopf öffnet die Frequenzbalken — die
  „tanzenden Balken" aus den alten Trackern/Demos. Reine Optik, zeigt auf einen
  Blick, ob Bass drückt und Höhen da sind (links tief, rechts hoch)
- **Ein LADEN-Menü:** ein einziger **LADEN**-Knopf bündelt alles — Sample laden,
  Sample-Browser, Song öffnen und ein Untermenü **Importieren** (MOD/XM/TFMX).
  Hält den Streifen aufgeräumt, statt vieler Knöpfe nebeneinander
- **IT/S3M-Import:** auch **Impulse Tracker** (`.it`) und **Scream Tracker 3**
  (`.s3m`) lassen sich importieren (LADEN ▸ Importieren) — Samples, Patterns und
  Reihenfolge. Gepackte IT-Samples/Hüllkurven bleiben (noch) außen vor
- **Module originalgetreu** ✅: MOD/XM/S3M/IT spielen mit **Pattern-Break** und
  **Position-Jump** ab und kürzere Patterns (<64 Zeilen) laufen richtig — die
  Songs klingen so, wie vom Original gedacht
- **MOD-Import:** klassische Amiga-`.mod`-Songs laden (LADEN ▸ Importieren) — Samples
  wandern in die Slots (bis zu 31), Patterns + Reihenfolge werden übernommen.
  Sofort abspielbar, weiterbaubar und als `.retrotrax` sicherbar
- **Live-Aufnahme:** Play drücken und auf der Tastatur mitspielen — die Noten
  werden direkt an der laufenden Stelle aufgenommen (in der Cursor-Spur), wie ein
  mitgeschnittenes Klavierspiel. Im Stopp tippst du wie gewohnt Schritt für Schritt
- **Stereo-Klang im Amiga-Stil:** die Spuren werden automatisch im Stereobild
  verteilt (LRRL wie ProTracker) — der Beat klingt von allein breit und lebendig;
  jede Note wird kurz ein-/ausgeblendet, damit nichts knackt
- **Rückgängig/Wiederholen** (Strg+Z / Strg+Y, bis zu 64 Schritte) und
  **Spur kopieren/einfügen/ausschneiden** (Strg+C / V / X)
- 16 Instrument-Slots, lädt WAV / AIFF / FLAC / OGG / MP3 sowie das native
  Amiga-Format **8SVX / IFF** (offen dokumentiert)
- **SoundFonts (SF2):** eine `.sf2`-Bank einbinden (über + ORDNER), ihre Sounds
  durchstöbern, vorhören und laden — das gewählte Sample wird als WAV
  herausgezogen, lässt sich also auch merken und mit dem Song speichern
- **ST-XX Sample-Disks eingebaut:** die legendären Amiga-Sample-Disketten
  (ST-01 bis ST-XX, 96 Disketten / ~5.900 Sounds, Public Domain) durchstöbern
  und per Klick laden — die Sounds kommen einzeln vom
  [Internet Archive](https://archive.org/details/AmigaSTXX_originals_plus_conversions)
  und werden lokal gecacht, das Plugin selbst bleibt klein
- **Sample-Browser mit Suche:** ein Suchfeld findet einen Sound über *alle*
  Disks und deine eigenen Ordner zugleich; ein Klick spielt ihn sofort vor
- **Eigene Ordner & Sammlung „Meine Sounds":** eigene Sample-Ordner von der
  Festplatte einbinden (bleiben nach Neustart erhalten) und Lieblings-Sounds
  per **MERKEN** in einer persönlichen Sammlung sammeln — in der Sammlung wird
  der Knopf zu **VERGESSEN** und wirft einen Sound wieder in den Papierkorb
- **Songs speichern & öffnen:** deine Arbeit als `.retrotrax`-Datei sichern und
  jederzeit wieder öffnen. **Selbst-enthaltend:** alle Samples werden komprimiert
  in die Datei eingebettet — eine portable Mini-Datei (auch gezeichnete/gegrabbte
  Klänge bleiben erhalten); SID-Songs brauchen gar keine Sampledaten
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
- Läuft als VST3 **und** CLAP in jeder DAW **und** als eigenständiges Programm

## Fahrplan

- **In Etappe 2 erledigt:** echte C64-SID-Emulation (reSIDfp) neben dem
  eingebauten Synth, Ring-Modulation, Hard-Sync, PWM, Werks-Presets, eigene
  SID-Sounds, der Unisono-Stack ✅, der **Akkord aus einer Note** ✅ und **16 Spuren
  mit Seiten-Scrollen** ✅ (vorher 8), **MOD-Import** ✅, **XM-Import** ✅
  (FastTracker 2: mehr Kanäle, 16-Bit-Samples, Finetune), das
  **CLAP-Format** ✅ (zusätzlich zu VST3, via clap-juce-extensions) und der
  **Akai-Sampler-Filter** ✅ (resonanter Tiefpass im Stil S900/S950/S1000 +
  12-Bit-Crunch, pro Sample), **Sampler-Effekte** ✅ (Reverse +
  Körnung/Sample-Rate-Reduktion), **Loop** ✅ (Vorwärts + Ping-Pong) und
  **analoge Wärme** ✅ (Drive-Sättigung + Vintage-Pitch wie die alten Sampler)
  und **Pattern-Quantisierung** ✅ (Live-Noten aufs Raster schnappen)
- **TFMX-Wiedergabe** ✅ (Chris Hülsbeck — Turrican, Apidya …): .mdat/.smpl laden
  und die Originale hören. Nutzt einen eingebundenen offenen (GPL) TFMX-Replayer,
  von RetroTrax umhüllt; Datei-Leser, Diagnose und Bedienung sind unser eigener Code
- **TFMX-Grabber** ✅: einzelne Klänge aus einem TFMX-Modul entnehmen und als WAVs
  speichern (Renoise-Plugin-Grabber-Idee). LADEN-Menü → *TFMX-Samples entnehmen*;
  der Ordner öffnet gleich im Sample-Browser zum Vorhören/Laden/Merken
- **Loop-Crossfade** ✅: der Regler **GLÄTTEN** im Akai-Panel blendet beim Vorwärts-
  Loop das Ende sanft in den Anfang über — kurze Samples loopen smooth statt
  abgehackt (Fairlight-Gefühl); Standard 0 = harter Sprung wie bisher
- **Drum-Kit (16 Pads)** ✅: der **KIT**-Knopf öffnet ein 4×4-Pad-Feld im
  MPC60/SP-1200-Stil mit eigenen Samples — anklicken oder per Tastatur trommeln
  (Pads leuchten beim Anschlag). **→ SLOT** / **SLOT →** schiebt Klänge frei
  zwischen Pad und Spur-Slot; das Kit wird im Song gespeichert. **Anschlagdynamik:**
  wo du aufs Pad klickst zählt — oben = harter/lauter Schlag, unten = leise
- **SP-1200/Emu-Klang pro Pad** ✅: jedes Pad hat eigene **STIMMUNG** (Halbtöne),
  **GRIT** (Sample-Rate-Reduktion), **12-BIT**-Crunch und einen **SP-1200**-Knopf,
  der den klassischen Crunch mit einem Klick auflegt — der dreckige Drum-Charakter
- **Fairlight-Sample-Werkzeug** ✅: der **FAIRLIGHT**-Knopf öffnet einen Wellenform-
  Editor — Bereich markieren + **trimmen**, **normalisieren**, **umkehren**, die
  Welle mit der Maus **zeichnen** (Lichtgriffel-Gefühl) und vor allem **in 16
  Kit-Scheiben choppen** (Break zerlegen → Drum-Pads, MPC/SP-1200-Idee)
- **Time-Stretch** ✅: im Fairlight-Werkzeug ein Sample länger/kürzer machen, ohne
  die Tonhöhe zu ändern (granulares Overlap-Add) — für jedes Sample
- **Slice → Pattern** ✅: einen Break in 16 Scheiben schneiden, in Instrument-Slots
  legen UND automatisch als Noten ins Pattern schreiben — der Break wird wieder
  spielbar und umbaubar (Recycle/Page-R-Idee)
- **Master-FX (Echo & Hall)** ✅: der **FX**-Knopf öffnet Effekte für den ganzen
  Mix (wie Renoises Master-Track) — **Echo** (Delay mit Rückkopplung) und **Hall**
  (Reverb) **und ein 3-Band-EQ** (Bass/Mitten/Höhen), standardmäßig aus/flach, im
  Song gespeichert
- **Wellenform-Generator** ✅: einfache Single-Cycle-Wellen (Sinus/Säge/Rechteck/
  Dreieck/Puls) in einen Slot erzeugen — loopt als Oszillator, ganz ohne Sample
- **Zufallsmelodie (WÜRFEL)** ✅: würfelt eine Moll-Pentatonik-Melodie in die
  Cursor-Spur — sofort Inspiration gegen das leere Blatt (Strg+Z zurück)
- **Swing / Groove** ✅: der **SWING**-Regler (neben BPM) versetzt gerade/ungerade
  Zeilen gegeneinander — der MPC/SP-1200-Shuffle, der den Beat lebendig macht
  (Tempo bleibt gleich, 0 % = gerade)
- **Später:** weitere Sampler-Effekte (Time-Stretch), echte Beaterkennung,
  .sid-Player + .retrotrax-Replayer für eigene Demos, Anfänger-Modus

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
# Optional fuer das CLAP-Format (sonst wird es einfach uebersprungen):
git clone --depth 1 --recurse-submodules --shallow-submodules https://github.com/free-audio/clap-juce-extensions.git libs/clap-juce-extensions
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j2
```

Ergebnisse landen in `build/RetroTrax_artefacts/Release/`:
`Standalone/` (Programm), `VST3/` und `CLAP/` (Plugins für die DAW).

## Unterstützen ❤️

RetroTrax ist und bleibt kostenlos. Wenn es dir Freude macht, kannst du mir
[auf Ko-fi einen Kaffee spendieren](https://ko-fi.com/mukkemann) — jede Tasse hält das Mukkemann-Universum am Laufen.

## Lizenz

GPL-3.0 — frei für alle.
