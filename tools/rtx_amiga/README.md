# rtx_amiga — nativer Amiga-68k-Player (Phase 1 + 2a)

Ziel: RetroTrax-Songs auf echter Amiga-68k-Hardware abspielen, nicht nur im
Plugin/Browser. Motiv und Rahmenbedingungen stehen im Plan
(`~/.claude/plans/expressive-toasting-willow.md`). Phase 1 (Toolchain,
Emulator-Verifikation, ein Sample abspielen, Synth-Freezer) ist komplett.
Phase 2a (PC-seitiges Export-Tool) ist komplett — als Nächstes kommt Phase 2b,
der echte 4-Kanal-68k-Pattern-Player, der `export/`s `.mod`-Dateien liest.

## Phase 2a — Export-Tool (`export/rtx_to_mod.cpp`)

**Bewusste Design-Entscheidung:** kein neues eigenes Amiga-Binärformat, sondern
Export als klassisches **4-Kanal-31-Sample-ProTracker-`.MOD`** — das uralte,
extrem gut dokumentierte Amiga-Standardformat. Vorteile: die Amiga-Szene kennt
`.MOD` bereits (passt zum Ziel, Format+Player frei an die Szene zu geben), und
der bestehende Importer (`ModImport.h`) kann den Export sofort wieder einlesen
— ein Rundreise-Test ist damit möglich, ganz ohne 68k-Code.

```sh
g++ -std=c++17 -O2 -DRETROTRAX_NO_JUCE -DHAVE_CXX17 -I src -I libs/residfp \
    tools/rtx_amiga/export/rtx_to_mod.cpp build/libresidfp.a -lpthread -lz \
    -o build/rtx_to_mod
./build/rtx_to_mod song.retrotrax song.mod
```

**Ablauf:** Song laden (.retrotrax oder .rtx) → alle Synth-Instrumente
automatisch einfrieren (`rt_freeze.h`) → höchstens 4 gleichzeitig belegte
Spuren prüfen (Paula hat nur 4 DMA-Kanäle — mehr führt zu einer klaren
Fehlermeldung, kein stilles Kappen) → als `.mod` schreiben → **Selbstprüfung:**
Datei mit dem bestehenden Importer wieder einlesen, Original und Reimport
rendern und über den Pegel (RMS) vergleichen (kein Sample-für-Sample-Diff,
das wäre blind für den Phasenversatz zwischen Live-Synthese und
Sample-Wiedergabe-Loop — s. Kommentar im Tool).

**Drei echte Bugs beim ersten Durchlauf gefunden und gefixt** (nicht nur
Theorie — jeder einzelne hätte beim Export lautlos falsch geklungen):
1. **Kanal-Kennung vs. tatsächlich geschriebene Kanaldaten:** die `"M.K."`-
   Signatur sagt jedem Leser fest 4 Kanäle zu — wurden nur so viele
   Kanal-Bytes geschrieben wie Spuren belegt waren (z. B. 1), verschob sich
   beim Wiedereinlesen alles. Fix: **immer 4 Kanäle schreiben**, ungenutzte
   bleiben leer (robuster als eine variable Kanalzahl-Kennung).
2. **Instrument-`gain` fehlte:** `Instrument::gain` ist im Plugin ein reiner
   Playback-Multiplikator (nicht in den rohen Sample-Daten enthalten) —
   `rt_freeze.h`s `calibrateGain()` setzt dort oft einen deutlichen Faktor.
   Ohne ihn in die PCM-Daten einzurechnen, klangen gefrorene Instrumente
   beim Export viel zu leise.
3. **MOD-Loop-Punkte wurden von `ModImport.h` nie gelesen** (bestehende
   Lücke, nicht durch den Export verursacht) — jedes gefrorene
   Synth-Instrument ist aber genau eine Loop (`rt_freeze.h` liefert immer
   `Loop::Forward` mit `loopStart=0`), ohne die klang jede gehaltene Note nur
   wie ein kurzer Klick statt eines gehaltenen Tons. Gefixt in `ModImport.h`
   (liest jetzt Loop-Start/-Länge) + `XmImport.h` (nur das Feld ergänzt,
   XM-Loop-Auswertung selbst bleibt offen) + `rt_mod.h` (übernimmt die
   Loop-Felder in die Engine) — nützt jetzt auch echten MOD-Importen im
   Plugin/CLI/Web-Player, nicht nur diesem Export-Tool.

**Effekt-Abdeckung:** nur Codes mit 1:1-MOD-Äquivalent (Arpeggio, Slides,
Vibrato, Sample-Offset, Vol-Slide, Position-Jump, Set-Volume, Pattern-Break,
Speed/Tempo) werden übernommen — alles andere landet als Warnung im Report,
nicht stillschweigend verworfen. Eine Zellen-Lautstaerke ohne eigenen Effekt
wird zum `0xC`-Effekt; kollidiert sie mit einem echten Effekt in derselben
Zelle, wird sie verworfen (klassisches MOD hat nur eine Effekt-Spalte pro
Zelle) und das steht im Report.

**Getestet:** Ein-Spur-Song (Classic-Synth) und Zwei-Spur-Song
(nicht-fortlaufende Instrument-Slots, zwei Instrumente) — beide Rundreisen
bestehen. Songs mit mehr als 4 gleichzeitig belegten Spuren (z. B.
"Neon Drift", "Startrekker Groove") liefern korrekt die Fehlermeldung statt
falsch zu exportieren — die Web-Player-Demos sind bewusst NICHT auf
Amiga-Hardware-Limits ausgelegt.

## Stand

- [x] Cross-Toolchain installiert und geprüft (Hello-World compiliert zu
      echtem AmigaOS-Binary).
- [x] FS-UAE + interne AROS-ROM booten headless (Xvfb).
- [x] Minimales Programm, das Paula-Kanal 0 direkt anspricht und ein
      eingebettetes 8-bit-Sample abspielt (`player/tone_test.c`).
- [x] Bootfähige Diskette (ADF) + headless Verifikation stehen und laufen
      automatisiert durch (`player/build.sh` + `player/verify.sh`) — echter,
      nicht-stiller Ton nachgewiesen (Peak ~-18 dB, sauberer ~1 kHz-Ton
      gemessen, `player/tone_clip.wav`).
- [x] Synth-Freezer (`src/rt_freeze.h`, PC-seitig, C++): SID/Classic/FM ->
      Sample-Instrument, per Autokorrelation phasentreu geloopt, Pegel per
      Selbstkalibrierung angeglichen. Verifiziert mit `rtx_cli freeze`
      (Live- vs. gefrorene Wiedergabe, Tonhöhe + Pegel innerhalb Toleranz) —
      damit ist **Phase 1 komplett**.
- [ ] Echter Mehrkanal-Pattern-Player (4 Kanäle, zeilengetaktet) — Phase 2.

## src/rt_freeze.h — Synth-Instrument einfrieren

`rtfreeze::freezeInstrument(synthInstrument, sampleRate, referenceNote, loopCycles)`
gibt ein normales Sample-Instrument zurück, das über den bestehenden
Sample-Wiedergabepfad läuft (`renderSample()` in `TrackerEngine.h`) — keine
neue Wiedergabe-Logik im Player nötig. Kurzfassung des Wegs (Details/Grenzen
als Kommentar im Header): Huellkurve beim Aufnehmen künstlich flach halten
(Attack~0, Sustain voll) -> Periode per Autokorrelation finden (mit
Oktav-Fallensicherung, s. Kommentar bei `detectPeriod`) -> phasentreuen
Loop-Ausschnitt an einem Nulldurchgang entnehmen -> Original-ADSR wieder
aufsetzen -> Pegel-Unterschied zwischen Synth- und Sample-Gain-Kette per
kurzem Probehören selbst kalibrieren (`detail::calibrateGain`).

**Selbsttest**: `rtx_cli freeze` baut drei eingebaute Testinstrumente
(Classic/SID/FM), friert jedes ein und vergleicht Live- gegen gefrorene
Wiedergabe (Tonhöhe per Autokorrelation, Pegel per RMS). Aktueller Stand:
alle drei innerhalb der Toleranz (Tonhöhe exakt, Pegel < 1 % Abweichung).
Schreibt `freeze_<name>_live.wav`/`freeze_<name>_frozen.wav` zum Nachhören.

## player/ — Testprogramm + Verifikation

```sh
cd tools/rtx_amiga/player
./build.sh    # compiliert tone_test.c, baut tone_disk.adf (bootfaehig, OFS)
./verify.sh   # startet FS-UAE headless, prueft ob echtes Audiosignal rauskommt
```

**Zwei Anlaeufe, ein wichtiger Fund unterwegs:** Der erste Versuch war ein
roher, klassischer Amiga-Bootblock (`tone_test.s` + `make_adf.py`, kein
AmigaOS/Exec noetig, direkter Sprung ins Programm nach dem ROM-Bootstrap —
die klassische Demoszene-Technik). Der blieb aber in FS-UAEs eingebauter
AROS-ROM-Ersatzvariante haengen: `dosboot.resource` kam laut Log nie zu einem
tatsaechlichen Disk-Read fuer einen rohen, nicht-dateisystem-basierten
Bootblock (moeglicherweise ein Kompatibilitaets-Limit dieser ROM-Variante,
nicht zwingend unseres echten Zielsystems mit echtem Kickstart). Funktioniert
hat stattdessen der Standard-Weg: `tone_test.c` als normales
AmigaOS-CLI-Executable (`vc +aos68k`), per `amitools`/`xdftool` auf eine
OFS-formatierte Diskette mit Standard-AmigaDOS-Bootblock (`boot install`) und
`S/startup-sequence`, die es beim Booten automatisch startet. `tone_test.s`
bleibt im Repo (funktionierender, korrekt kodierter Code — geprueft per
vasm-Listing gegen die Amiga-Hardware-Referenz), falls er spaeter auf echter
Hardware/echtem Kickstart nochmal relevant wird.

**Audio-Aufnahme, zweiter Fund:** Der erste Ansatz (ALSA-Loopback-Kernelmodul,
s. u.) lieferte durchgehend Stille, obwohl das Programm nachweislich lief
(CLI-Task im Log, Boot-Sequenz durchlaufen). Grund: FS-UAEs Audio laeuft
ueber **OpenAL Soft**, das `ALSA_CARD`-Umbiegen nicht zuverlässig respektiert.
Die robuste Loesung: OpenAL Softs eigenes **Wave-Writer-Backend**
(`ALSOFT_CONF` auf eine Mini-Config mit `drivers=wave` + `[wave] file=...`
zeigen) schreibt die Emulator-Audioausgabe direkt und zuverlässig in eine
WAV-Datei — kein ALSA/Xvfb-Audio-Routing-Bedarf mehr. `verify.sh` nutzt genau
das.

## Toolchain (vbcc + vasm + vlink)

Installiert unter `~/amiga-toolchain/vbcc` (ARM64-Binärpaket von
`ibaug.de/vbcc/vbcc_linux_arm.tar.gz`, läuft auf dem Pi im 32-bit-ARM-
Kompatibilitätsmodus). Vor jedem Build:

```sh
source tools/rtx_amiga/env.sh   # setzt $VBCC und $PATH
vc +aos68k dein_programm.c -o dein_programm
file dein_programm   # -> "AmigaOS loadseg()ble executable/binary"
```

`+aos68k` ist das mitgelieferte Config-Profil für AmigaOS/68k
(`~/amiga-toolchain/vbcc/config/aos68k`) — ruft intern `vbccm68k` (Compiler),
`vasmm68k_mot` (Assembler) und `vlink` (Amiga-Hunk-Linker) auf.

## Verifikation ohne echte Hardware (FS-UAE)

`amitools`/`vamos` emuliert nur AmigaOS-API-Aufrufe (Exec/DOS) — für direkte
Hardware-Register-Ansteuerung (Paula/Copper/CIA) ungeeignet. Stattdessen:

- **FS-UAE** (`apt install fs-uae`, als ARM64-Paket in Debian trixie
  vorhanden) emuliert die Hardware bis auf Registerebene.
- **Keine separate Kickstart-ROM nötig** — FS-UAE hat eine freie AROS-ROM
  fest eingebaut, aktiviert mit `--kickstart_file=internal`.
- **Kein X-Server nötig** — läuft unter `Xvfb` (bereits bekanntes Muster von
  RetroTrax' eigenem Pi-Test-Workflow, `[[retrotrax_pi_headless_test]]`).
- **Audio-Mitschnitt über OpenAL Softs Wave-Writer-Backend** (nicht über
  ALSA — ein erster Versuch mit einem ALSA-Loopback-Kernelmodul lieferte
  zuverlässig Stille, obwohl das Programm nachweislich lief, weil FS-UAEs
  Audio über OpenAL läuft und `ALSA_CARD`-Umbiegen dabei nicht respektiert
  wird). Funktionierende Lösung, s. `player/verify.sh`:

```sh
cat > /tmp/alsoft.conf << EOF
[general]
drivers=wave,
[wave]
file=/pfad/zur/ausgabe.wav
EOF
ALSOFT_CONF=/tmp/alsoft.conf DISPLAY=:89 fs-uae --kickstart_file=internal \
    --amiga_model=A500 --fullscreen=0 --floppy_drive_0=dein_disk.adf
```

Amiga-Modell A500 + `amitools`/`xdftool` fürs ADF-Bauen (`pip3 install
--user --break-system-packages amitools`) haben sich als der zuverlässige
Weg bewährt — s. Fund oben zum rohen Bootblock, der in dieser ROM-Variante
hängenblieb.

## Nächster Schritt

Phase 1 (Toolchain, Emulator-Verifikation, ein Sample abspielen, Synth-
Freezer) und Phase 2a (Export-Tool, s. oben) sind fertig. Weiter geht's mit
**Phase 2b: der echte 4-Kanal-68k-Pattern-Player**, der die von `rtx_to_mod`
erzeugten `.mod`-Dateien liest — Zeilen-getaktet über CIA/VBlank-Interrupt,
alle 4 Paula-Kanäle direkt angesteuert (kein AmigaOS-Sample-Player-Umweg).
