# rtx_amiga — nativer Amiga-68k-Player (Phase 1 + 2a + 2b-Grundstein)

Ziel: RetroTrax-Songs auf echter Amiga-68k-Hardware abspielen, nicht nur im
Plugin/Browser. Motiv und Rahmenbedingungen stehen im Plan
(`~/.claude/plans/expressive-toasting-willow.md`). Phase 1 (Toolchain,
Emulator-Verifikation, ein Sample abspielen, Synth-Freezer) ist komplett.
Phase 2a (PC-seitiges Export-Tool) ist komplett. Phase 2b (der echte
4-Kanal-68k-Pattern-Player) hat jetzt einen ersten funktionierenden, aber
bewusst noch eingeschränkten Grundstein - Details unten unter "Phase 2b".
Das ist ein mehrwöchiges Vorhaben laut Plan; das hier ist der erste
Ausschnitt, nicht der fertige Player.

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

**Bekannter Fehler entdeckt (05.09., beim Bau von Phase 2b aufgefallen,
NICHT von Phase 2b verursacht):** `c64-daydream.retrotrax` (3 Spuren, sollte
also exportierbar sein) besteht die Rundreise-Selbstprüfung NICHT - Original
2117120 Frames vs. Reimport 1693696 Frames (Längen-Toleranz gerissen).
Pegel/Verhältnis sind ok, nur die Länge weicht deutlich ab. Vermutung
ungeprüft: irgendein Effekt/Pattern-Konstrukt in diesem Song, das beim
Reimport eine andere Songlänge ergibt als beim Original-Rendern - noch nicht
eingegrenzt, welcher. Betrifft nur den Amiga-Export, NICHT den Web-Player
(dort läuft der Song normal). Separater Fixpunkt, nicht Teil dieser Session.

## Phase 2b — der echte Pattern-Player (`player/mod_core.h` + `mod_player.c`)

**Architektur (nach Repo-Konvention "erst nativ prüfen, bevor der
Zielcode/68k vertraut wird", wie schon bei `rtx_wasm`):** die eigentliche
Abspiellogik (Order/Pattern/Zeilen/Tick-Fortschritt, Notenauslösung,
Effekte) steckt in `player/mod_core.h` - portables ANSI-C ohne jede
Hardware-/OS-Abhängigkeit. Ein Hardware-Backend (`ModHw`: `trigger`/
`setVolume`/`waitTick`) wird beim Init übergeben. Zwei Backends nutzen
denselben Kern:
- `player/host_test.c`: Software-Paula-Mixer (nicht-interpoliert, wie die
  echte DAC), rendert ein echtes `.mod` komplett zu WAV - kein 68k nötig.
  Bauen: `gcc -std=c89 -O2 -o build/rtx_amiga_host_test player/host_test.c -lm`,
  Nutzung: `build/rtx_amiga_host_test song.mod out.wav`.
- `player/mod_player.c`: echte Paula-Register (68k, AmigaOS-CLI-Programm wie
  Phase 1s `tone_test.c` - der rohe Bootblock-Weg hängte AROS beim Booten
  auf, s. Phase-1-Notiz oben). Der Song steckt eingebettet im Executable
  (`player/mod2c.py song.mod song_data.h`, kein AmigaDOS-Dateizugriff nötig).
  Bauen + auf ADF packen: `player/build_mod.sh song.mod`. Headless testen:
  `player/verify_mod.sh`.

**Ein echter Bug beim Bauen gefunden und gefixt** (nicht nur Theorie - hätte
Pattern-Break/Position-Jump beim ersten echten Einsatz lautlos falsch
gemacht): `mcStep()` hielt das Ziel eines 0xB/0xD-Effekts zunächst in einer
LOKALEN Variable, die aber erst beim LETZTEN Tick der Zeile ausgewertet wird
- also in einem SPÄTEREN `mcStep()`-Aufruf, der die Effekt-Zeile selbst gar
nicht mehr sieht. Der Sprung ging dadurch komplett verloren (Song spielte
einfach stur weiter statt zu springen). Fix: Ziel jetzt in
`pl->pendingPosJump`/`pl->pendingPatBreak` (Player-Zustand statt lokale
Variable). Gefunden über einen selbstgebauten Kontrollfluss-Test
(`trace_test.c`, nicht im Repo - reine Verifikationshilfe): eine handgebaute
Mini-MOD-Datei mit genau diesen beiden Effekten zeigte den Fehler sofort im
Trigger-Log; nach dem Fix läuft die Order-Reihenfolge exakt wie erwartet
(inklusive des Grenzfalls "Pattern-Break landet genau auf dem Songende ->
Order wickelt korrekt um").

**Verifiziert:**
- `host_test.c` gegen `tools/rtx_cli/test_song.retrotrax` (per `rtx_to_mod`
  exportiert, 1 Spur, besteht dessen eigene Rundreise-Prüfung): 338688
  gerenderte Ausgabe-Frames (7,68 s) - nahezu identisch zu den 338944 Frames,
  die die Referenz-Engine beim Rundreise-Test dafür meldet (Differenz < 6 ms,
  reine Rundungstoleranz an den Song-Rändern). Peak-Pegel -1,8 dB, klar
  hörbares Signal.
- `mod_player.c` bootet und läuft headless in FS-UAE (`verify_mod.sh`,
  gleiches Muster wie Phase 1s `verify.sh`): reales, nicht-stilles
  Audiosignal aus echten Paula-Registerschreibzugriffen nachgewiesen.
- **Offene Einschränkung der Verifikation (ehrlich, nicht schöngeredet):**
  die per FS-UAE/Xvfb aufgenommene WAV-Datei zeigt Lücken/Aussetzer statt
  durchgehendem Ton. Nachgeprüft mit einem isolierten Minimaltest (exakt
  Phase 1s Ein-Trigger-Ansatz, ohne jede Player-Logik): **derselbe
  Lücken-Effekt tritt dort genauso auf**, und FS-UAEs eigenes Log zeigt in
  BEIDEN Fällen (altem `tone_test` UND neuem `mod_player`) dieselbe Zeile
  `WARNING: Emulation frame rate may suffer` (dazu `CPU scaling governor is
  'ondemand', not 'performance'`) - der Pi schafft headless (Xvfb, kein
  GPU-Treiber) offenbar nicht durchgehend Echtzeit-Emulationstempo, das reißt
  Löcher in die WAV-Aufnahme. Das ist also eine **vorbestehende Grenze der
  Headless-Verifikationsmethode selbst** (bestand schon bei Phase 1, fiel
  dort nur nicht auf, weil `verify.sh` nur auf "irgendwo Pegel > -60 dB"
  prüft, nicht auf Durchgehendigkeit) - keine neue Regression von Phase 2b.
  **Heisst konkret:** die Abspiellogik selbst ist solide (Host-Test + der
  gefixte Kontrollfluss-Bug beweisen das unabhängig von FS-UAE), aber ob der
  Klang auf echter/besser emulierter Hardware wirklich durchgehend UND im
  richtigen Tempo läuft, ist durch die Headless-Prüfung allein nicht
  abschließend bestätigt - ein echtes Zuhören (oder ein Testlauf mit
  `performance`-CPU-Governor / mit echtem Display statt Xvfb) wäre der
  nächste echte Beleg.

**Bewusst noch nicht in dieser Stufe (siehe Kommentare in `mod_core.h` /
`mod_player.c`):**
- Effekte: nur 0xC (Set-Volume), 0xB (Position-Jump), 0xD (Pattern-Break),
  0xF (Speed/Tempo) - Arpeggio/Slides/Vibrato/Sample-Offset fehlen.
- Sustain-Loop-Nachladen (AUDxIP-Interrupt): Paula wiederholt beim
  Puffer-Ende automatisch DENSELBEN Puffer - das ist bei allen aus
  RetroTrax eingefrorenen Synth-Instrumenten schon exakt richtig (deren
  Loop ist immer der GANZE Sample, s. `rt_freeze.h`), würde aber bei einem
  echten externen MOD mit kurzem Sustain-Loop nach längerem Attack falsch
  loopen (wiederholt dann den ganzen Sample inkl. Attack).
- Tempo: Tick-Dauer wird in ganzen VBlanks (50 Hz PAL) angenähert statt per
  CIA-Timer-Interrupt - bei Standard-Tempo 125 BPM exakt (=1 VBlank/Tick),
  bei anderen Tempi gerundet (und nach oben durch 1 VBlank/Tick gedeckelt -
  sehr hohe BPM laufen dadurch langsamer als am PC/Web-Player).
- Noch kein "richtiger" Demo-Song exportiert - `test_song.retrotrax` (der
  einzige aktuell zuverlässig exportierbare Song) ist ein technischer
  Testsong, kein vorzeigbares Musikstück. Sobald der oben genannte
  `c64-daydream`-Bug gefunden/gefixt ist, wäre der ein guter Kandidat (3
  Spuren, passt unter das 4-Kanal-Limit).

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
- [x] PC-seitiges Export-Tool RetroTrax -> klassisches Amiga-`.mod`
      (`export/rtx_to_mod.cpp`) mit Rundreise-Selbstprüfung — **Phase 2a
      komplett** (bis auf den oben notierten `c64-daydream`-Einzelfall).
- [~] Echter Mehrkanal-Pattern-Player (4 Kanäle, zeilengetaktet) — **Phase
      2b begonnen**: Kern (`player/mod_core.h`) läuft und ist host-verifiziert
      (inkl. eines gefundenen+gefixten Kontrollfluss-Bugs), der echte
      68k-Player (`player/mod_player.c`) bootet und erzeugt echten
      Paula-Ton in FS-UAE. Noch offen: mehr Effekte, Sustain-Loop-Nachladen,
      CIA-genaues Tempo, ein vorzeigbarer Demo-Song, und eine Verifikation
      jenseits der (nachweislich lückenhaften) Headless-Audioaufnahme.

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
