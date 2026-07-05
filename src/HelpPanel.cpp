#include "HelpPanel.h"

HelpPanel::HelpPanel()
{
    topicList.setColour (juce::ListBox::backgroundColourId, rt::rowBeat);
    topicList.setColour (juce::ListBox::outlineColourId, rt::steel.withAlpha (0.5f));
    topicList.setOutlineThickness (1);
    topicList.setRowHeight (24);
    addAndMakeVisible (topicList);

    bodyView.setMultiLine (true);
    bodyView.setReadOnly (true);
    bodyView.setScrollbarsShown (true);
    bodyView.setCaretVisible (false);
    bodyView.setFont (rt::mono (14.0f));
    bodyView.setColour (juce::TextEditor::backgroundColourId, rt::rowBeat);
    bodyView.setColour (juce::TextEditor::textColourId, rt::text);
    bodyView.setColour (juce::TextEditor::outlineColourId, rt::steel.withAlpha (0.5f));
    addAndMakeVisible (bodyView);

    closeButton.onClick = [this] { if (onClose) onClose(); };
    addAndMakeVisible (closeButton);

    setWantsKeyboardFocus (true);
    rebuild();
}

void HelpPanel::applyLanguage()
{
    closeButton.setButtonText (loc::t ("SCHLIESSEN", "CLOSE"));
    rebuild();
    repaint();
}

// ---- Inhalte (waechst hier mit jedem neuen Feature mit) -------------------

void HelpPanel::rebuild()
{
    const int keep = currentTopic;
    topics.clearQuick();

    topics.add ({ loc::t ("Willkommen", "Welcome"),
        loc::t (
            "Mukkemann RetroTrax - der Tracker mit Herz: Amiga-Gefuehl + C64-SID,\n"
            "fuer heute gebaut.\n\n"
            "Ein Tracker ist ein Musikprogramm, in dem du Noten in ein Raster (das\n"
            "\"Pattern\") tippst. Jede Spalte ist eine Spur, jede Zeile ein Schritt.\n"
            "Von oben nach unten wird abgespielt - wie Noten auf einer Walze.\n\n"
            "Du brauchst keine Vorkenntnisse. Lade einen Sound, tippe ein paar\n"
            "Noten, druecke die Leertaste - fertig ist dein erster Beat.",
            "Mukkemann RetroTrax - the tracker with heart: Amiga feel + C64 SID,\n"
            "built for today.\n\n"
            "A tracker is a music program where you type notes into a grid (the\n"
            "\"pattern\"). Each column is a track, each line a step. Playback runs\n"
            "top to bottom - like notes on a music box cylinder.\n\n"
            "No prior knowledge needed. Load a sound, type a few notes, hit the\n"
            "space bar - and there's your first beat.") });

    topics.add ({ loc::t ("Ueber RetroTrax", "About RetroTrax"),
        loc::t (
            "Mukkemann RetroTrax - Free Open Source Music Tracker.\n\n"
            "Amiga-Sampling und echter C64-SID-Klang in EINEM Werkzeug, gemischt in\n"
            "einem Song - kostenlos, offen und fuer Windows, macOS und Linux. Gebaut\n"
            "fuer alle, die ohne teure Hardware Musik machen wollen: Ideen von frueher,\n"
            "Komfort von heute.\n\n"
            "Projekt: github.com/Mukkemann1972/retrotrax  -  Ko-fi: ko-fi.com (Mukkemann)\n"
            "Laeuft als VST3, CLAP und als eigenstaendiges Programm.\n"
            "Das Logo erscheint beim Start (Klick/Taste ueberspringt).",
            "Mukkemann RetroTrax - Free Open Source Music Tracker.\n\n"
            "Amiga sampling and real C64 SID sound in ONE tool, mixed in a single\n"
            "song - free, open and for Windows, macOS and Linux. Built for everyone\n"
            "who wants to make music without expensive hardware: ideas from the past,\n"
            "comfort of today.\n\n"
            "Project: github.com/Mukkemann1972/retrotrax  -  Ko-fi: ko-fi.com (Mukkemann)\n"
            "Runs as VST3, CLAP and a standalone app.\n"
            "The logo shows on startup (click/key to skip).") });

    topics.add ({ loc::t ("Schnellstart", "Quick start"),
        loc::t (
            "1. Druecke SAMPLES und such dir einen Sound aus den ST-Disks aus\n"
            "   (Anklicken spielt ihn sofort vor). Dann IN SLOT LADEN.\n"
            "2. Klicke ins Raster und tippe Noten mit den Tasten Y X C V B N M.\n"
            "3. Druecke die Leertaste zum Abspielen. Nochmal = Stop.\n"
            "4. Stelle das Tempo (BPM) oben ein.\n"
            "5. Wenn es dir gefaellt: SONG SPEICHERN.\n\n"
            "Das war's im Kern. Alles Weitere findest du in den Themen links.",
            "1. Press SAMPLES and pick a sound from the ST disks (clicking plays\n"
            "   it instantly). Then LOAD INTO SLOT.\n"
            "2. Click into the grid and type notes with the keys Y X C V B N M.\n"
            "3. Press the space bar to play. Again = stop.\n"
            "4. Set the tempo (BPM) at the top.\n"
            "5. When you like it: SAVE SONG.\n\n"
            "That's the core. Everything else is in the topics on the left.") });

    topics.add ({ loc::t ("Noten tippen (Tastatur)", "Typing notes (keyboard)"),
        loc::t (
            "Die Tastatur ist dein Klavier (deutsches QWERTZ-Layout):\n\n"
            "  Y X C V B N M        Noten in der aktuellen Oktave\n"
            "  S D   G H J          die schwarzen Tasten (Halbtoene)\n"
            "  Q 2 W 3 E R 5 T 6 Z  eine Oktave hoeher\n\n"
            "  Pfeiltasten / Bild auf-ab / Pos1 / Ende   Cursor bewegen\n"
            "  Tab / Shift+Tab      naechste / vorherige Spur\n"
            "  Leertaste            Play / Stop\n"
            "  Entf / Ruecktaste    Zelle loeschen\n"
            "  + / -                Oktave wechseln\n"
            "  1 (Noten-Spalte)     Note-Aus (\"OFF\") - laesst SID-Stimmen ausklingen\n"
            "  Ziffern              Wert in Instrument-/Lautstaerke-Spalte\n\n"
            "  Strg+Z / Strg+Y      Rueckgaengig / Wiederholen\n"
            "  Strg+C / V / X       Spur kopieren / einfuegen / ausschneiden\n\n"
            "Vertippt? Kein Problem - Strg+Z nimmt jeden Schritt zurueck (bis zu\n"
            "64). Mit Strg+C/V kopierst du eine ganze Spur z.B. von Spur 1 nach 5:\n"
            "Cursor in die Quellspur, Strg+C, dann in die Zielspur und Strg+V.\n\n"
            "TESTEN vs. AUFNEHMEN: Mit Play (oder Leertaste) laeuft der Song und du\n"
            "kannst frei mitspielen, OHNE etwas zu veraendern - perfekt zum Ausprobieren\n"
            "einer Melodie. Erst wenn du REC drueckst (leuchtet rot), werden die live\n"
            "gespielten Noten an der laufenden Stelle in deiner Cursor-Spur\n"
            "aufgenommen. Nur bei REC reitet der Cursor auf dem Play-Balken mit.\n"
            "Im Stopp tippst du die Noten Schritt fuer Schritt an die Cursor-Zeile\n"
            "(der Cursor springt dann von selbst weiter).",
            "Your keyboard is the piano (German QWERTZ layout):\n\n"
            "  Y X C V B N M        notes in the current octave\n"
            "  S D   G H J          the black keys (semitones)\n"
            "  Q 2 W 3 E R 5 T 6 Z  one octave higher\n\n"
            "  Arrow keys / Page up-down / Home / End   move the cursor\n"
            "  Tab / Shift+Tab      next / previous track\n"
            "  Space bar            play / stop\n"
            "  Delete / Backspace   clear cell\n"
            "  + / -                change octave\n"
            "  1 (note column)      note off (\"OFF\") - lets SID voices fade out\n"
            "  Number keys          value in instrument / volume column\n\n"
            "  Ctrl+Z / Ctrl+Y      undo / redo\n"
            "  Ctrl+C / V / X       copy / paste / cut track\n\n"
            "Typo? No problem - Ctrl+Z takes back every step (up to 64). With\n"
            "Ctrl+C/V you copy a whole track, e.g. from track 1 to 5: cursor into\n"
            "the source track, Ctrl+C, then into the target track and Ctrl+V.\n\n"
            "TEST vs. RECORD: with Play (or space) the song runs and you can jam\n"
            "along WITHOUT changing anything - perfect for trying out a melody. Only\n"
            "when you press REC (lit red) are your live notes recorded onto the\n"
            "current row in your cursor track. Only during REC does the cursor ride\n"
            "the play bar. When\n"
            "stopped you instead type notes step by step at the cursor row (the\n"
            "cursor then advances on its own).") });

    topics.add ({ loc::t ("Pattern & Spuren", "Pattern & tracks"),
        loc::t (
            "Das Raster hat 16 Spuren und 64 Zeilen. Die Cursor-Zeile bleibt in\n"
            "der Mitte, das Pattern scrollt daran vorbei - wie beim alten\n"
            "ProTracker.\n\n"
            "Es passen nicht alle 16 Spuren nebeneinander ins Fenster - das Grid\n"
            "zeigt einen Ausschnitt und scrollt seitlich mit, sobald der Cursor an\n"
            "den Rand laeuft (Pfeil < bzw. > im Kopf = da liegen noch mehr Spuren).\n"
            "Mit TAB springst du eine Spur weiter, Pfeil links/rechts Spalte fuer\n"
            "Spalte. Ein breiteres Fenster zeigt mehr Spuren auf einmal.\n\n"
            "Jede Spur spielt zur gleichen Zeit eine Note ab. So baust du\n"
            "Schlagzeug, Bass und Melodie uebereinander.\n\n"
            "STUMM & SOLO (wie in jeder DAW): Unter jedem Spurnamen sitzen zwei\n"
            "kleine Knoepfe. M (rot) schaltet die Spur stumm. S (gelb) schaltet sie\n"
            "solo - dann hoerst du NUR die ge-soloten Spuren. So findest du schnell,\n"
            "welche Spur im Mix nicht passt. Beides per Mausklick, jederzeit auch\n"
            "waehrend der Wiedergabe; es veraendert deine Noten nicht und wird nicht\n"
            "im Song gespeichert.\n\n"
            "(Mehrere Patterns ergeben einen ganzen Song - siehe \"Song-Modus\".)",
            "The grid has 16 tracks and 64 rows. The cursor row stays in the\n"
            "centre and the pattern scrolls past it - just like the old\n"
            "ProTracker.\n\n"
            "Not all 16 tracks fit side by side - the grid shows a slice and\n"
            "scrolls sideways as the cursor reaches the edge (a < or > arrow in the\n"
            "header means there are more tracks that way). TAB jumps a whole track,\n"
            "left/right arrows move column by column. A wider window shows more\n"
            "tracks at once.\n\n"
            "Each track plays one note at the same moment. That's how you stack\n"
            "drums, bass and melody.\n\n"
            "MUTE & SOLO (like every DAW): below each track name sit two little\n"
            "buttons. M (red) mutes the track. S (yellow) solos it - then you hear\n"
            "ONLY the soloed tracks. That's how you quickly find which track doesn't\n"
            "sit right in the mix. Both by mouse click, any time even during\n"
            "playback; it doesn't change your notes and isn't saved with the song.\n\n"
            "(Several patterns become a whole song - see \"Song mode\".)") });

    topics.add ({ loc::t ("Block-Bearbeitung", "Block editing"),
        loc::t (
            "Damit bearbeitest du ganze Bereiche auf einmal - z.B. einen Takt\n"
            "ueber mehrere Spuren.\n\n"
            "- UMSCHALT + Pfeile: einen Bereich markieren (er wird hell\n"
            "  hinterlegt). Ein einfacher Pfeil ohne Umschalt hebt die Auswahl\n"
            "  wieder auf.\n"
            "- STRG+C kopiert den Block, STRG+X schneidet ihn aus,\n"
            "  STRG+V fuegt ihn ab der Cursor-Zelle wieder ein.\n"
            "  (Ohne Auswahl wirken C/X/V wie gehabt auf die ganze Spur.)\n\n"
            "Das Beste fuers Arbeiten nach Gehoer:\n"
            "- ALT + Pfeil verschiebt den markierten Block (oder einfach die\n"
            "  Zelle unterm Cursor) DIREKT um eine Zeile oder Spur - ohne\n"
            "  loeschen und neu eintippen. Im Stopp hoerst du die Note dabei\n"
            "  gleich. So kannst du nudgen und hoeren, bis es sitzt.\n\n"
            "Alles laesst sich mit STRG+Z rueckgaengig machen.",
            "This lets you edit whole areas at once - e.g. one bar across\n"
            "several tracks.\n\n"
            "- SHIFT + arrows: mark an area (it gets a light highlight). A plain\n"
            "  arrow without shift clears the selection again.\n"
            "- CTRL+C copies the block, CTRL+X cuts it, CTRL+V pastes it from\n"
            "  the cursor cell. (With no selection, C/X/V act on the whole\n"
            "  track as before.)\n\n"
            "The best part for working by ear:\n"
            "- ALT + arrow moves the selected block (or just the cell under the\n"
            "  cursor) DIRECTLY by one row or track - no deleting and retyping.\n"
            "  While stopped you hear the note as it moves, so you can nudge\n"
            "  and listen until it sits right.\n\n"
            "Everything can be undone with CTRL+Z.") });

    topics.add ({ loc::t ("Quantisieren", "Quantise"),
        loc::t (
            "Beim Live-Einspielen landen Noten manchmal eine Zeile zu frueh oder\n"
            "zu spaet. QUANT (in der Song-Leiste) schiebt alle Noten des aktuellen\n"
            "Patterns sauber aufs Raster.\n\n"
            "- Das Auswahlfeld daneben waehlt das Raster: 1/8 = jede 2. Zeile,\n"
            "  1/4 = jede 4., 1/2 = jede 8., 1/1 = jede 16.\n"
            "- Ein Klick auf QUANT schnappt alle Spuren auf das naechste Rasterfeld.\n"
            "- Mit STRG+Z wieder rueckgaengig, falls es doch nicht passt.\n\n"
            "So was hatte kein alter Tracker - eine kleine moderne Hilfe fuers\n"
            "Spielen nach Gefuehl.",
            "When playing live, notes sometimes land a row too early or late.\n"
            "QUANT (in the song bar) snaps all notes of the current pattern neatly\n"
            "onto the grid.\n\n"
            "- The box next to it picks the grid: 1/8 = every 2nd row, 1/4 = every\n"
            "  4th, 1/2 = every 8th, 1/1 = every 16th.\n"
            "- One click on QUANT snaps all tracks to the nearest grid row.\n"
            "- Undo with CTRL+Z if it doesn't fit after all.\n\n"
            "No old tracker had this - a small modern help for playing by feel.") });

    topics.add ({ loc::t ("Effekt-Spalte", "Effect column"),
        loc::t (
            "Rechts in jeder Spur steht die Effekt-Spalte (gruen). Damit machst\n"
            "du Klaenge lebendig - Tonhoehe biegen, zittern lassen, Lautstaerke\n"
            "und Tempo steuern.\n\n"
            "Eingabe: Cursor in die gruene Spalte (Pfeil rechts oder Klick),\n"
            "dann drei Hex-Stellen tippen - ein Befehl + zwei Parameter.\n"
            "Beispiel: C-4-0 ergibt \"C40\" = Lautstaerke auf 40 (hex) setzen.\n"
            "ENTF leert die Effekt-Zelle wieder.\n\n"
            "Die wichtigsten Befehle (Hex):\n"
            "  0xy  Arpeggio - Akkord: Grundton, +x, +y Halbtoene im Wechsel\n"
            "  1xx  Tonhoehe gleitet hoch (Slide up)\n"
            "  2xx  Tonhoehe gleitet runter (Slide down)\n"
            "  3xx  Tone-Portamento - zur neuen Note hingleiten (nicht neu\n"
            "       anschlagen); xx = Tempo des Gleitens\n"
            "  4xy  Vibrato - x = Tempo, y = Tiefe\n"
            "  9xx  Sample-Offset - Sample bei xx*256 Samples starten\n"
            "       (Breakbeat-Chops!); 900 = letzten Offset wiederverwenden\n"
            "  Axy  Lautstaerke-Slide - x hoch, y runter (pro Tick)\n"
            "  Bxx  Position-Jump - zu Eintrag xx der Reihenfolge springen\n"
            "  Cxx  Lautstaerke setzen (00..40 hex = 0..64)\n"
            "  Dxx  Pattern-Break - sofort weiter ins naechste Pattern, Zeile xx\n"
            "  E9x  Retrigger - Note alle x Ticks neu anschlagen (Snare-Wirbel)\n"
            "  ECx  Note-Cut - Note ab Tick x stumm (knackig abgehackt)\n"
            "  EDx  Note-Delay - Anschlag erst bei Tick x (Shuffle, versetzte\n"
            "       Akkorde)\n"
            "  Fxx  Tempo: xx unter 20 = Ticks pro Zeile (Speed),\n"
            "       ab 20 = BPM\n\n"
            "Tipp: 0 und 4 ohne Parameter tun nichts - die Werte machen die\n"
            "Musik. Probier z.B. 047 (Arpeggio Dur), 432 (sanftes Vibrato)\n"
            "oder E92 auf einer Snare (Wirbel).\n"
            "(Weitere Effekte wachsen hier mit, so wie das Programm waechst.)",
            "On the right of each track sits the effect column (green). It's how\n"
            "you bring sounds to life - bend pitch, add wobble, control volume\n"
            "and tempo.\n\n"
            "To type: move the cursor into the green column (right arrow or\n"
            "click), then type three hex digits - one command + two parameters.\n"
            "Example: C-4-0 gives \"C40\" = set volume to 40 (hex). DELETE clears\n"
            "the effect cell again.\n\n"
            "The most important commands (hex):\n"
            "  0xy  arpeggio - chord: root, +x, +y semitones, alternating\n"
            "  1xx  pitch slides up\n"
            "  2xx  pitch slides down\n"
            "  3xx  tone portamento - glide to the new note (no retrigger);\n"
            "       xx = glide speed\n"
            "  4xy  vibrato - x = speed, y = depth\n"
            "  9xx  sample offset - start the sample at xx*256 samples\n"
            "       (breakbeat chops!); 900 = reuse the last offset\n"
            "  Axy  volume slide - x up, y down (per tick)\n"
            "  Bxx  position jump - jump to entry xx of the order list\n"
            "  Cxx  set volume (00..40 hex = 0..64)\n"
            "  Dxx  pattern break - continue in the next pattern at row xx\n"
            "  E9x  retrigger - restrike the note every x ticks (snare rolls)\n"
            "  ECx  note cut - mute the note from tick x (tight and choppy)\n"
            "  EDx  note delay - strike the note only at tick x (shuffle,\n"
            "       staggered chords)\n"
            "  Fxx  tempo: xx below 20 = ticks per row (speed), 20+ = BPM\n\n"
            "Tip: 0 and 4 with no parameter do nothing - the values make the\n"
            "music. Try 047 (major arpeggio), 432 (gentle vibrato) or E92 on\n"
            "a snare (roll).\n"
            "(More effects will grow here as the program grows.)") });

    topics.add ({ loc::t ("Song-Modus", "Song mode"),
        loc::t (
            "Ein Pattern hat 64 Zeilen - meist ein kurzer Teil (Strophe,\n"
            "Refrain, Beat). Aus mehreren Patterns baust du einen ganzen Song.\n\n"
            "Die Leiste ueber dem Raster:\n"
            "- < PAT / PAT > : zwischen den Patterns blaettern. PATTERN 01,\n"
            "  02 ... sind voneinander getrennte, leere Blaetter zum Fuellen.\n"
            "- LOOP / SONG : umschalten. LOOP wiederholt beim Abspielen nur das\n"
            "  Pattern, das du gerade bearbeitest (gut zum Basteln). SONG spielt\n"
            "  die ganze Reihenfolge ab - dein fertiges Lied.\n"
            "- + PAT : haengt das aktuelle Pattern hinten an die Reihenfolge.\n"
            "- - PAT : nimmt den letzten Eintrag wieder weg.\n"
            "- Rechts steht die Reihenfolge, z.B. \"Reihe: 01 02 02 03\".\n\n"
            "So baust du einen Song: Patterns einzeln fuellen (mit < PAT/PAT >\n"
            "wechseln), dann mit + PAT die Reihenfolge zusammenstecken (ein\n"
            "Pattern darf mehrfach vorkommen!), auf SONG schalten und PLAY.\n"
            "Im Song-Lauf zeigt das Raster automatisch das Pattern, das gerade\n"
            "klingt. Alles wird mit dem Song gespeichert.",
            "A pattern has 64 rows - usually a short part (verse, chorus, beat).\n"
            "Several patterns make up a whole song.\n\n"
            "The bar above the grid:\n"
            "- < PAT / PAT > : page through the patterns. PATTERN 01, 02 ... are\n"
            "  separate, empty sheets to fill.\n"
            "- LOOP / SONG : switch. LOOP repeats only the pattern you're editing\n"
            "  (great for tinkering). SONG plays the whole order - your finished\n"
            "  song.\n"
            "- + PAT : appends the current pattern to the order.\n"
            "- - PAT : removes the last entry.\n"
            "- On the right is the order, e.g. \"Order: 01 02 02 03\".\n\n"
            "To build a song: fill patterns one by one (switch with < PAT/PAT >),\n"
            "then assemble the order with + PAT (a pattern may appear several\n"
            "times!), switch to SONG and press PLAY. During playback the grid\n"
            "automatically shows the pattern that's currently sounding.\n"
            "Everything is saved with the song.") });

    topics.add ({ loc::t ("Instrumente & Farben", "Instruments & colours"),
        loc::t (
            "Es gibt 16 Instrument-Slots. In jeden laedst du einen Sound (WAV,\n"
            "AIFF, FLAC, OGG, MP3 und das native Amiga-Format 8SVX/IFF) - entweder\n"
            "ueber SAMPLE LADEN von der Platte oder bequem ueber den SAMPLES-Browser.\n\n"
            "Jedes Instrument hat eine feste Farbe. Die Noten im Raster leuchten\n"
            "in der Farbe ihres Instruments, und der Farbpunkt neben der\n"
            "Instrument-Auswahl zeigt deine aktuelle \"Malfarbe\".",
            "There are 16 instrument slots. Load a sound into each (WAV, AIFF,\n"
            "FLAC, OGG, MP3 and the native Amiga format 8SVX/IFF) - either via\n"
            "LOAD SAMPLE from disk, or comfortably through the SAMPLES browser.\n\n"
            "Each instrument has a fixed colour. Notes in the grid glow in their\n"
            "instrument's colour, and the colour dot next to the instrument\n"
            "selector shows your current \"paint colour\".") });

    topics.add ({ loc::t ("SID-Synthesizer", "SID synthesizer"),
        loc::t (
            "Neben Samples kann jeder Slot auch ein selbst erzeugter Klang sein -\n"
            "im Stil des legendaeren C64-SID-Chips. Sample und SID mischst du in\n"
            "einem Song frei, Slot fuer Slot.\n\n"
            "So geht's: Slot oben waehlen, dann den Knopf SID druecken. Der Slot\n"
            "wird zu einem SID-Instrument, und das SID-Fenster geht auf. Tippe\n"
            "Noten ins Raster - der Klang ist sofort da.\n\n"
            "Im SID-Fenster stellst du ein:\n"
            "- WELLENFORM: Dreieck (weich), Saege (scharf), Puls (klassischer\n"
            "  C64-Sound) oder Rauschen (fuer Hi-Hats, Snares, Effekte).\n"
            "- PULSWEITE: nur bei Puls - aendert die Klangfarbe (duenn bis voll).\n"
            "  PWM-TEMPO + PWM-TIEFE lassen die Pulsweite von allein wabern - der\n"
            "  typische fette, lebendige C64-Lead/Bass.\n"
            "- Huellkurve ADSR - wie ein Ton ueber die Zeit lauter/leiser wird:\n"
            "    ANSTIEG (A)  wie schnell er einsetzt (0 = sofort, klick-hart)\n"
            "    ABFALL (D)   wie schnell er auf den Halte-Pegel faellt\n"
            "    HALTEN (S)   wie laut er gehalten wird, solange die Note klingt\n"
            "    AUSKLANG (R) wie lange er nach dem Note-Aus nachklingt\n"
            "- FILTER: das Herz des SID-Sounds. AUS = ungefiltert. TIEFPASS\n"
            "  schluckt die Hoehen (warm, dumpf), HOCHPASS die Tiefen (duenn,\n"
            "  zischelig), BANDPASS laesst nur die Mitte durch. GRENZE (Cutoff)\n"
            "  schiebt die Trennlinie hoch/runter, RESONANZ betont sie - hoch\n"
            "  gedreht pfeift und zwitschert es wie ein echter C64.\n"
            "- MODULATION (2. Oszillator): RING-MOD multipliziert zwei Oszillatoren\n"
            "  -> metallische, glockige Klaenge. HARD-SYNC laesst einen Master die\n"
            "  hoerbare Welle staendig zuruecksetzen -> zerreissende, schreiende\n"
            "  Leads. MOD-TONHOEHE stimmt den zweiten Oszillator (in Halbtoenen) -\n"
            "  damit formst du den Charakter; einmal durchdrehen lohnt sich.\n"
            "- STIMMEN (UNISONO): stapelt 1, 2 oder 3 leicht verstimmte Stimmen pro\n"
            "  Note -> fetter, breiter Klang (Super-Saw / Multi-SID). VERSTIMMUNG\n"
            "  regelt, wie weit sie auseinanderlaufen. Beim echten Chip nutzt das die\n"
            "  3 echten SID-Stimmen; mit Ring/Sync sind es hoechstens 2.\n"
            "- AKKORD (AUS 1 NOTE): aus einem einzigen Tastendruck klingt ein ganzer\n"
            "  Akkord. DUR (froehlich), MOLL (traurig), SUS4 (schwebend), QUINTE\n"
            "  (Powerchord, hart) oder OKTAVE (fett). Nutzt dieselben Stapel-Stimmen\n"
            "  wie Unisono - darum ruhen bei aktivem Akkord die STIMMEN-Knoepfe; die\n"
            "  VERSTIMMUNG verbreitert den Akkord weiterhin. Steht der Akkord auf AUS,\n"
            "  gilt wieder der Unisono-Stack. Mit Ring/Sync klingt nur der Grundton +\n"
            "  ein Akkord-Ton (die zweite Stimme ist dann der Modulator).\n\n"
            "Jede Aenderung spielt sofort ein Probe-C an - du hoerst, was du\n"
            "drehst (oder druecke TEST). Ein SID-Ton klingt weiter (Sustain), bis du ihn mit der\n"
            "Taste 1 (Note-Aus, zeigt \"OFF\") ausklingen laesst oder neu anschlaegst.\n\n"
            "WERKS-PRESETS (Reihe oben im SID-Fenster): fertige Startklaenge zum\n"
            "Anklicken - BASS, LEAD, GLOCKE, DRUMS, PAD, SYNC-LEAD, BLIP. Ein Klick\n"
            "setzt alle Regler auf den Klang und spielt ihn an; danach regelst du\n"
            "frei weiter. Schnellster Weg zu einem brauchbaren Ton.\n"
            "MEINE SID-SOUNDS: hast du einen Klang nach deinem Geschmack gedreht,\n"
            "druecke MERKEN und gib ihm einen Namen - er landet in deiner eigenen\n"
            "Liste und steht in jedem Song wieder bereit (mit Klangmotor). Aus der\n"
            "Liste waehlen laedt ihn sofort, VERGESSEN wirft ihn in den Papierkorb.\n"
            "Tipps: Bass = Puls, kurzes A/D, mittleres S. Lead = Saege oder\n"
            "Dreieck mit etwas Vibrato (Effekt 4xy). Drums = Rauschen mit sehr\n"
            "kurzem A/D und S=0.\n"
            "KLANG-MOTOR (oben im SID-Fenster): waehle pro Instrument zwischen\n"
            "KLASSISCH (der gewohnte selbstgebaute Synth) und ECHTER CHIP (die\n"
            "originalgetreue reSIDfp-Emulation des MOS-6581-Chips). Beide spielen\n"
            "aus denselben Reglern - probier aus, was dir besser gefaellt. Alte\n"
            "Songs bleiben auf KLASSISCH und klingen unveraendert.",
            "Besides samples, every slot can also be a sound you create yourself -\n"
            "in the style of the legendary C64 SID chip. Mix samples and SID\n"
            "freely in one song, slot by slot.\n\n"
            "How: pick a slot at the top, then press the SID button. The slot\n"
            "becomes a SID instrument and the SID window opens. Type notes into\n"
            "the grid - the sound is there instantly.\n\n"
            "In the SID window you set:\n"
            "- WAVEFORM: triangle (soft), saw (sharp), pulse (the classic C64\n"
            "  sound) or noise (for hi-hats, snares, effects).\n"
            "- PULSE WIDTH: pulse only - changes the tone colour (thin to full).\n"
            "  PWM RATE + PWM DEPTH make the pulse width sway on its own - the\n"
            "  classic fat, lively C64 lead/bass.\n"
            "- ADSR envelope - how a note's level changes over time:\n"
            "    ATTACK (A)   how fast it starts (0 = instant, click-hard)\n"
            "    DECAY (D)    how fast it falls to the sustain level\n"
            "    SUSTAIN (S)  how loud it's held while the note sounds\n"
            "    RELEASE (R)  how long it rings after a note off\n"
            "- FILTER: the heart of the SID sound. OFF = unfiltered. LOW-PASS\n"
            "  removes the highs (warm, dull), HIGH-PASS the lows (thin, hissy),\n"
            "  BAND-PASS lets only the middle through. CUTOFF moves the dividing\n"
            "  line up/down, RESONANCE emphasises it - turned up it whistles and\n"
            "  chirps like a real C64.\n"
            "- MODULATION (2nd osc): RING MOD multiplies two oscillators -> metallic,\n"
            "  bell-like tones. HARD SYNC has a master constantly reset the audible\n"
            "  wave -> tearing, screaming leads. MOD PITCH tunes the second\n"
            "  oscillator (in semitones) - that shapes the character; worth a sweep.\n"
            "- VOICES (UNISON): stacks 1, 2 or 3 slightly detuned voices per note ->\n"
            "  fat, wide sound (super-saw / multi-SID). DETUNE sets how far they\n"
            "  spread. On the real chip this uses the 3 actual SID voices; with\n"
            "  ring/sync it's at most 2.\n"
            "- CHORD (FROM 1 NOTE): a single key press sounds a whole chord. MAJOR\n"
            "  (happy), MINOR (sad), SUS4 (floating), FIFTH (power chord, hard) or\n"
            "  OCTAVE (fat). It uses the same stack voices as unison - so while a\n"
            "  chord is on the VOICES buttons rest; DETUNE still widens the chord.\n"
            "  Set the chord to OFF and the unison stack applies again. With ring/sync\n"
            "  only the root + one chord note sound (the second voice is the modulator).\n\n"
            "Every change plays a test C right away - you hear what you turn (or\n"
            "press TEST). A SID note keeps sounding (sustain) until you let it fade with key 1\n"
            "(note off, shows \"OFF\") or retrigger it.\n\n"
            "FACTORY PRESETS (row at the top of the SID window): ready-made starting\n"
            "sounds to click - BASS, LEAD, BELL, DRUMS, PAD, SYNC LEAD, BLIP. One\n"
            "click sets every control to that sound and plays it; then tweak freely.\n"
            "The fastest way to a usable tone.\n"
            "MY SID SOUNDS: once you've dialled in a sound you like, press REMEMBER\n"
            "and name it - it goes into your own list and is ready in every song\n"
            "(with its engine). Picking it from the list loads it instantly, FORGET\n"
            "moves it to the trash.\n"
            "Tips: bass = pulse, short A/D, medium S. Lead = saw or triangle with\n"
            "a little vibrato (effect 4xy). Drums = noise with very short A/D and\n"
            "S=0.\n"
            "SOUND ENGINE (top of the SID window): choose per instrument between\n"
            "CLASSIC (the familiar self-built synth) and REAL CHIP (the authentic\n"
            "reSIDfp emulation of the MOS 6581 chip). Both play from the same\n"
            "controls - try which one you like better. Old songs stay on CLASSIC\n"
            "and sound unchanged.") });

    topics.add ({ loc::t ("Klang & Stereo", "Sound & stereo"),
        loc::t (
            "RetroTrax verteilt die 16 Spuren automatisch im Stereobild - leicht\n"
            "nach links und rechts, abwechselnd (das LRRL-Muster der Amiga-Tracker).\n"
            "So klingt dein Beat von allein breit und lebendig, ohne dass du etwas\n"
            "einstellen musst. Das Vorhoeren beim Tippen bleibt mittig.\n\n"
            "Jede Note wird ausserdem winzig kurz ein- und am Ende wieder\n"
            "ausgeblendet. Dadurch knackt nichts, auch wenn du schnell tippst oder\n"
            "viele Noten dicht hintereinander setzt.\n\n"
            "Die Lautstaerke-Spalte (00-64) regelt einzelne Noten leiser: 64 ist\n"
            "voll, 32 etwa halb so laut, 00 still.",
            "RetroTrax spreads the 16 tracks across the stereo field automatically -\n"
            "slightly left and right, alternating (the LRRL pattern of the Amiga\n"
            "trackers). Your beat sounds wide and lively on its own, with nothing\n"
            "to set up. Previewing while typing stays centred.\n\n"
            "Every note also gets a tiny fade in and out. That keeps things from\n"
            "clicking, even when you type fast or place many notes close together.\n\n"
            "The volume column (00-64) makes individual notes quieter: 64 is full,\n"
            "32 about half, 00 silent.") });

    topics.add ({ loc::t ("Sample-Browser", "Sample browser"),
        loc::t (
            "Der SAMPLES-Knopf oeffnet den Browser mit den legendaeren ST-XX\n"
            "Amiga-Sample-Disketten (96 Disks, ~5.900 Sounds, Public Domain).\n\n"
            "- Links waehlst du eine Diskette oder einen eigenen Ordner.\n"
            "- Ein Klick auf ein Sample spielt es sofort vor.\n"
            "- IN SLOT LADEN (oder Doppelklick) legt es in den aktuellen Slot.\n"
            "- Das Suchfeld oben durchsucht ALLE Disks und Ordner auf einmal.\n"
            "- * hinter einem Namen heisst: schon heruntergeladen (laedt sofort).\n\n"
            "Mit + ORDNER bindest du eigene Sample-Ordner von der Festplatte ein\n"
            "(sie bleiben nach Neustart erhalten). ENTF nimmt einen wieder raus -\n"
            "deine Dateien bleiben dabei unangetastet.",
            "The SAMPLES button opens the browser with the legendary ST-XX Amiga\n"
            "sample disks (96 disks, ~5,900 sounds, public domain).\n\n"
            "- On the left you pick a disk or one of your own folders.\n"
            "- Clicking a sample plays it instantly.\n"
            "- LOAD INTO SLOT (or double-click) puts it in the current slot.\n"
            "- The search box at the top searches ALL disks and folders at once.\n"
            "- A * after a name means: already downloaded (loads instantly).\n\n"
            "With + FOLDER you add your own sample folders from disk (they stay\n"
            "after a restart). REMOVE takes one out again - your files are left\n"
            "untouched.") });

    topics.add ({ loc::t ("FX: Akai-Sampler-Effekte", "FX: Akai sampler effects"),
        loc::t (
            "Der FX-Knopf hat zwei Reiter. Der Reiter AKAI-SAMPLER-EFFEKTE bearbeitet\n"
            "das Sample im aktuellen Slot - der warme, druckvolle Klang der alten\n"
            "Sampler. (Der zweite Reiter MASTER-EFFEKTE wirkt auf den ganzen Mix.)\n\n"
            "- FILTER AN schaltet einen resonanten Tiefpass dazu (24 dB/Okt).\n"
            "- GRENZE bestimmt, ab wo die Hoehen weggenommen werden.\n"
            "- RESONANZ betont die Grenzfrequenz - hoeher = es faengt an zu pfeifen.\n"
            "- 12-BIT schaltet den koernigen lo-fi-Crunch der 12-Bit-Sampler dazu\n"
            "  (geht auch ohne Filter).\n"
            "- REVERSE spielt das Sample rueckwaerts.\n"
            "- KOERNUNG senkt die Abtastrate (Decimator) - rau und koernig, der\n"
            "  klassische heruntergetaktete Sampler-Sound (auch ohne Filter).\n"
            "- LOOP: AUS spielt einmal ab; VORWAERTS faengt vorne wieder an;\n"
            "  PING-PONG laeuft vor und zurueck (knackfrei) - gut fuer Flaechen.\n"
            "  Der Loop laeuft, solange die Note klingt (mit Taste 1 = Note-Aus enden).\n"
            "- GLAETTEN (Loop-Crossfade): blendet beim VORWAERTS-Loop das Ende sanft\n"
            "  in den Anfang - kurze Samples loopen smooth statt abgehackt (Fairlight-\n"
            "  Gefuehl). 0 = harter Sprung wie bisher.\n"
            "- DRIVE: weiche Saettigung wie die analogen Sampler-Filter - Waerme,\n"
            "  Druck und Punch; hohe Resonanz saettigt musikalisch statt zu clippen.\n"
            "- VINTAGE: pitcht roh ohne Glaettung (wie die langsame Wandler-Clock\n"
            "  alter Sampler) - crunchy, lebendig, besonders tief gespielt.\n"
            "- BAND-EIERN (Mellotron-Tape-Wow): langsames Band-Eiern + leichtes\n"
            "  Flattern - die Tonhoehe schwankt sanft, jede Note eiert eigen. 0 = aus.\n"
            "- HUELLKURVE (ADSR) + LAUTSTAERKE: das Sample wie an einem echten Sampler\n"
            "  formen - Attack/Decay/Sustain/Release und die Lautstaerke pro Klang.\n"
            "- VINTAGE-CHARAKTER: beruehmte Maschinen auf einen Klick - S950 (12-Bit\n"
            "  klar), S1000 (16-Bit sauber), SP-1200 (dreckige HipHop-Drums), EMU II\n"
            "  (12-Bit warm), MIRAGE (8-Bit rau), FAIRLIGHT (8-Bit kristallin),\n"
            "  MELLOTRON (Bandeiern + warme Saettigung). Jeder kombiniert nur die\n"
            "  obigen Regler - danach frei weiterdrehen.\n\n"
            "Standard ist AUS, dein Sample bleibt also unveraendert, bis du den\n"
            "Filter einschaltest. Die Einstellung wird im Song (.retrotrax)\n"
            "mitgespeichert. TEST spielt ein C-5 mit dem aktuellen Klang.",
            "The FX button has two tabs. The AKAI SAMPLER FX tab edits the sample in\n"
            "the current slot - the warm, punchy sound of the old samplers. (The\n"
            "second tab, MASTER FX, affects the whole mix.)\n\n"
            "- FILTER ON adds a resonant low-pass (24 dB/oct).\n"
            "- CUTOFF sets where the highs start to be removed.\n"
            "- RESONANCE emphasises the cutoff - higher = it starts to whistle.\n"
            "- 12-BIT adds the gritty lo-fi crunch of the 12-bit samplers\n"
            "  (works without the filter too).\n"
            "- REVERSE plays the sample backwards.\n"
            "- GRAIN lowers the sample rate (decimator) - rough and gritty, the\n"
            "  classic downsampled sampler sound (works without the filter too).\n"
            "- LOOP: OFF plays once; FORWARD restarts from the front; PING-PONG\n"
            "  runs forward and back (click-free) - great for pads. The loop runs\n"
            "  while the note sounds (end it with key 1 = note off).\n"
            "- SMOOTH (loop crossfade): on a FORWARD loop, blends the end gently into\n"
            "  the start - short samples loop smoothly instead of choppy (Fairlight\n"
            "  feel). 0 = hard jump as before.\n"
            "- DRIVE: soft saturation like the analog sampler filters - warmth,\n"
            "  punch and push; high resonance saturates musically instead of clipping.\n"
            "- VINTAGE: pitches raw without smoothing (like the slow converter clock\n"
            "  of old samplers) - crunchy, alive, especially when played low.\n"
            "- TAPE WOW (Mellotron): slow tape pitch drift + gentle flutter - the\n"
            "  pitch wavers softly, each note wobbles on its own. 0 = off.\n"
            "- VINTAGE CHARACTER: famous machines in one click - S950 (12-bit clear),\n"
            "  S1000 (16-bit clean), SP-1200 (dirty hip-hop drums), EMU II (12-bit\n"
            "  warm), MIRAGE (8-bit rough), FAIRLIGHT (8-bit crystalline), MELLOTRON\n"
            "  (tape wow + warm saturation). Each just combines the knobs above -\n"
            "  tweak further afterwards.\n\n"
            "Off by default, so your sample stays unchanged until you switch the\n"
            "filter on. The setting is saved with the song (.retrotrax). TEST plays\n"
            "a C-5 with the current sound.") });

    topics.add ({ loc::t ("SoundFonts (SF2)", "SoundFonts (SF2)"),
        loc::t (
            "Eine SF2-Datei (SoundFont) ist eine ganze Klang-Bibliothek in einer\n"
            "Datei - oft hunderte fertige Sounds. Davon gibt es riesige, frei\n"
            "nutzbare Sammlungen im Netz.\n\n"
            "So bindest du eine ein: druecke + ORDNER und waehle statt eines\n"
            "Ordners einfach eine .sf2-Datei aus. Sie erscheint dann links in der\n"
            "Quellenliste (farbig hervorgehoben) - klick sie an, und rechts stehen\n"
            "alle Sounds der Bank.\n\n"
            "Anklicken hoert vor, IN SLOT LADEN laedt - genau wie bei den ST-Disks.\n"
            "Das gewaehlte Sample wird dabei als WAV herausgezogen, damit es sich\n"
            "auch MERKEN und mit dem Song speichern laesst. ENTF nimmt eine\n"
            "SF2-Bank wieder aus der Liste (die Datei bleibt).",
            "An SF2 file (SoundFont) is a whole sound library in one file - often\n"
            "hundreds of ready-made sounds. There are huge, freely usable\n"
            "collections out there.\n\n"
            "To add one: press + FOLDER and simply pick a .sf2 file instead of a\n"
            "folder. It shows up on the left in the source list (highlighted) -\n"
            "click it and all the bank's sounds appear on the right.\n\n"
            "Click to preview, LOAD INTO SLOT to load - just like the ST disks.\n"
            "The chosen sample is extracted to a WAV so you can REMEMBER it and\n"
            "save it with your song. REMOVE takes an SF2 bank out of the list\n"
            "again (the file stays).") });

    topics.add ({ loc::t ("Meine Sounds (Sammlung)", "My Sounds (collection)"),
        loc::t (
            "Ganz oben in der Ordnerliste steht immer \"Meine Sounds\" - deine\n"
            "persoenliche Sammlung.\n\n"
            "Gefaellt dir ein Sound (egal ob von einer ST-Disk oder aus einem\n"
            "eigenen Ordner)? Waehl ihn an und druecke MERKEN. Schon liegt er\n"
            "dauerhaft in deiner Sammlung - so baust du dir nach und nach deine\n"
            "eigene Lieblings-Bibliothek auf.\n\n"
            "Brauchst du einen Sound nicht mehr? Oeffne links \"Meine Sounds\",\n"
            "waehl den Sound an - der MERKEN-Knopf heisst dort jetzt VERGESSEN.\n"
            "Druecken, und der Sound wandert in den Papierkorb (ein Fehlklick ist\n"
            "also nicht endgueltig). ENTF ist nur fuer eigene Ordner da.\n\n"
            "Aufraeumen im Schnelldurchlauf: Mit Strg- oder Shift-Klick markierst\n"
            "du mehrere Sounds und VERGESSEN raeumt sie alle auf einmal weg. Nach\n"
            "dem Loeschen springt die Auswahl von selbst zum naechsten Sound -\n"
            "so klickst du dich zuegig durch.",
            "At the very top of the folder list there's always \"My Sounds\" -\n"
            "your personal collection.\n\n"
            "Like a sound (whether from an ST disk or one of your own folders)?\n"
            "Select it and press REMEMBER. It's stored in your collection for\n"
            "good - that's how you gradually build your own favourites library.\n\n"
            "Don't need a sound anymore? Open \"My Sounds\" on the left, select the\n"
            "sound - the REMEMBER button is labelled FORGET there. Press it and the\n"
            "sound goes to the trash (so a misclick isn't final). REMOVE is only\n"
            "for your own folders.\n\n"
            "Tidying up fast: Ctrl- or Shift-click marks several sounds and FORGET\n"
            "clears them all at once. After deleting, the selection jumps to the\n"
            "next sound by itself - so you can click through quickly.") });

    topics.add ({ loc::t ("Song speichern & oeffnen", "Save & open song"),
        loc::t (
            "Oben rechts: SONG SPEICHERN und SONG OEFFNEN.\n\n"
            "Deine Songs werden als .retrotrax-Datei gesichert (im Ordner\n"
            "Musik/RetroTrax). Gespeichert werden Tempo, alle Noten und welche\n"
            "Samples in den Slots liegen.\n\n"
            "WAV: Der Knopf WAV oben rechts rendert deinen ganzen Song in eine\n"
            "Stereo-WAV-Datei - fertig zum Teilen, Hochladen (Ko-fi/YouTube) oder\n"
            "Weiterverarbeiten. Im Song-Modus wird die ganze Reihenfolge gerendert,\n"
            "sonst das gerade gezeigte Pattern (je einmal).\n\n"
            "Hinweis: Es werden die WEGE zu den Samples gemerkt, nicht die Klaenge\n"
            "selbst. Solange die Sample-Dateien an ihrem Platz bleiben, klingt\n"
            "dein Song beim Oeffnen wieder genau gleich. Fehlt mal ein Sample,\n"
            "sagt dir RetroTrax beim Oeffnen Bescheid, welches.\n\n"
            "MOD LADEN: importiert ein klassisches Amiga-MOD (.mod). Die Samples\n"
            "landen in den Instrument-Slots (bis zu 31), die Patterns und die\n"
            "Reihenfolge werden uebernommen - du kannst den Song gleich abspielen,\n"
            "weiterbauen und als .retrotrax sichern. Die Samples stecken dann fest\n"
            "im Song. (Tausende alte MODs gibt es frei auf modarchive.org.) Hinweis:\n"
            "MODs mit Loop-Samples (z.B. lange Flaechen) klingen evtl. etwas anders,\n"
            "weil noch ohne Sample-Schleife importiert wird.\n\n"
            "XM LADEN: importiert ein FastTracker-2-Modul (.xm) - den groesseren\n"
            "Bruder des MOD. Mehr Kanaele, 16-Bit-Samples und Feinstimmung werden\n"
            "verstanden; pro Instrument wird das erste Sample geladen. Patterns mit\n"
            "mehr als 64 Zeilen werden auf 64 gekuerzt, Huellkurven/Loops bleiben\n"
            "(noch) aussen vor. (Tausende XMs gibt es frei auf modarchive.org.)\n\n"
            "S3M / IT: Scream Tracker 3 (.s3m) und Impulse Tracker (.it) - zwei\n"
            "weitere Klassiker (im LADEN-Menue unter Importieren). Gleiches Prinzip:\n"
            "Samples in die Slots, Patterns und Reihenfolge werden uebernommen.\n"
            "Gepackte IT-Samples und Huellkurven bleiben aussen vor. (Auch davon\n"
            "Tausende frei auf modarchive.org.)",
            "Top right: SAVE SONG and OPEN SONG.\n\n"
            "Your songs are stored as a .retrotrax file (in the Music/RetroTrax\n"
            "folder). Saved are the tempo, all notes and which samples sit in the\n"
            "slots.\n\n"
            "WAV: The WAV button (top right) renders your whole song into a stereo\n"
            "WAV file - ready to share, upload (Ko-fi/YouTube) or process further.\n"
            "In song mode the whole order is rendered, otherwise the current pattern\n"
            "(once each).\n\n"
            "Note: the PATHS to the samples are remembered, not the sounds\n"
            "themselves. As long as the sample files stay in place, your song\n"
            "sounds exactly the same when reopened. If a sample is missing,\n"
            "RetroTrax tells you which one on opening.\n\n"
            "LOAD MOD: imports a classic Amiga MOD (.mod). Its samples go into the\n"
            "instrument slots (up to 31), the patterns and the order are taken over\n"
            "- you can play it right away, keep building and save it as .retrotrax\n"
            "(the samples are then baked into the song). Thousands of old MODs are\n"
            "free on modarchive.org. Note: MODs with looped samples (e.g. long pads)\n"
            "may sound a bit different, as import is still without sample looping.\n\n"
            "LOAD XM: imports a FastTracker 2 module (.xm) - the bigger brother of\n"
            "the MOD. More channels, 16-bit samples and finetuning are understood;\n"
            "the first sample of each instrument is loaded. Patterns longer than 64\n"
            "rows are clipped to 64, envelopes/loops are not handled yet. (Thousands\n"
            "of XMs are free on modarchive.org.)\n\n"
            "S3M / IT: Scream Tracker 3 (.s3m) and Impulse Tracker (.it) - two more\n"
            "classics (in the LOAD menu under Import). Same idea: samples go into the\n"
            "slots, patterns and order are taken over. Packed IT samples and envelopes\n"
            "are not handled. (Thousands of these are free on modarchive.org too.)") });

    topics.add ({ loc::t ("TFMX (Chris Huelsbeck)", "TFMX (Chris Huelsbeck)"),
        loc::t (
            "TFMX ist das legendaere Amiga-Musikformat von Chris Huelsbeck (Turrican,\n"
            "Apidya, R-Type ...). Anders als MOD/XM ist es KEIN Noten-Raster, sondern\n"
            "eine laufende Makro-Maschine - darum spielt RetroTrax TFMX mit einem\n"
            "eigenen Nachbau ab, statt es ins Grid zu importieren.\n\n"
            "TFMX-Songs bestehen aus ZWEI Dateien: mdat.NAME (die Musik) und\n"
            "smpl.NAME (die Klaenge). Beide muessen im selben Ordner liegen. Mit dem\n"
            "Knopf TFMX waehlst du die mdat-Datei; die passende smpl wird automatisch\n"
            "daneben gesucht. (Frei sammeln kann man sie z.B. bei ExoticA/Modland.)\n\n"
            "Nach dem Laden zeigt RetroTrax kurz, was drinsteckt (Subsongs, Patterns,\n"
            "Makros, Samples). Mit PLAY startet die Wiedergabe, STOP haelt an, PLAY\n"
            "spielt wieder von vorn. Geladen wird Subsong 0. Die Wiedergabe nutzt\n"
            "einen eingebundenen, offenen TFMX-Player (wie der echte SID-Chip beim\n"
            "SID-Synth) - laesst sich aber nicht im Raster bearbeiten.",
            "TFMX is the legendary Amiga music format by Chris Huelsbeck (Turrican,\n"
            "Apidya, R-Type ...). Unlike MOD/XM it is NOT a note grid but a running\n"
            "macro machine - so RetroTrax plays TFMX with its own rebuilt player\n"
            "instead of importing it into the grid.\n\n"
            "TFMX songs consist of TWO files: mdat.NAME (the music) and smpl.NAME\n"
            "(the sounds). Both must sit in the same folder. The TFMX button lets you\n"
            "pick the mdat file; the matching smpl is found next to it automatically.\n"
            "(They can be collected freely e.g. at ExoticA/Modland.)\n\n"
            "After loading, RetroTrax briefly shows what's inside (subsongs, patterns,\n"
            "macros, samples). PLAY starts playback, STOP halts, PLAY plays from the\n"
            "top again. Subsong 0 is loaded. Playback uses a vendored open TFMX player\n"
            "(like the real SID chip for the SID synth) - but it can't be edited in\n"
            "the grid.") });

    topics.add ({ loc::t ("TFMX-Samples entnehmen (Grabber)", "Grab TFMX samples"),
        loc::t (
            "Die einzelnen Klaenge aus einem TFMX-Modul kannst du herausziehen und\n"
            "wie normale Samples weiterverwenden - die Idee vom Plugin-Grabber in\n"
            "Renoise, nur fuer TFMX. Im LADEN-Menue: \"TFMX-Samples entnehmen\".\n\n"
            "Du waehlst eine mdat-Datei; RetroTrax liest aus den Makros heraus, welche\n"
            "Bereiche im Sample-Speicher wirklich benutzt werden, und speichert jeden\n"
            "als eigene WAV unter Musik/RetroTrax/TFMX-Samples/NAME. Dieser Ordner\n"
            "wird gleich im Sample-Browser geoeffnet: Anklicken hoert vor, IN SLOT\n"
            "LADEN legt den Klang in ein Instrument, MERKEN nimmt ihn in \"Meine\n"
            "Sounds\". Die laufende TFMX-Wiedergabe wird dabei nicht gestoert.\n\n"
            "Hinweis: Es kommen alle benutzten Bereiche - auch winzige Wellenform-\n"
            "Schnipsel. Was du nicht brauchst, einfach liegen lassen oder loeschen.",
            "You can pull the individual sounds out of a TFMX module and reuse them\n"
            "like normal samples - the plugin-grabber idea from Renoise, but for TFMX.\n"
            "In the LOAD menu: \"Grab samples from TFMX\".\n\n"
            "Pick an mdat file; RetroTrax reads from the macros which regions of the\n"
            "sample memory are actually used and saves each as its own WAV under\n"
            "Music/RetroTrax/TFMX-Samples/NAME. That folder opens right away in the\n"
            "sample browser: click to preview, LOAD INTO SLOT puts the sound into an\n"
            "instrument, REMEMBER adds it to \"My Sounds\". Ongoing TFMX playback is\n"
            "not disturbed.\n\n"
            "Note: every used region comes out - including tiny waveform snippets.\n"
            "Just leave or delete what you don't need.") });

    topics.add ({ loc::t ("Drum-Kit (16 Pads)", "Drum kit (16 pads)"),
        loc::t (
            "Der KIT-Knopf oeffnet das Drum-Kit im Stil der klassischen Drum-Sampler\n"
            "(MPC60, E-mu SP-1200): ein 4x4-Pad-Feld mit 16 EIGENEN Samples, getrennt\n"
            "von den 16 Spur-Slots - zum Finger-Trommeln und Klang-Basteln.\n\n"
            "- Pad anklicken spielt es (und leuchtet auf). Wo du klickst zaehlt:\n"
            "  oben am Pad = harter, lauter Schlag, unten = leise (Anschlagdynamik).\n"
            "- Per Tastatur trommeln:\n"
            "  1 2 3 4 / Q W E R / A S D F / Y X C V (oben links = Pad 13, unten\n"
            "  links = Pad 1, wie auf einer echten MPC).\n"
            "- LADEN legt ein Sample ins gewaehlte Pad (Doppelklick auf ein leeres\n"
            "  Pad geht auch). LEEREN raeumt es wieder weg.\n"
            "- -> SLOT kopiert das Pad in den aktuellen Spur-Slot, damit du es im\n"
            "  Tracker benutzen kannst; SLOT -> holt umgekehrt das Sample aus dem\n"
            "  aktuellen Slot ins Pad. So wandern Klaenge frei hin und her\n"
            "  (auch TFMX-Grabber-Beute oder Browser-Sounds).\n"
            "- Charakter pro Pad (SP-1200/Emu): STIMMUNG stimmt das Pad in Halbtoenen\n"
            "  (runter = dicker/crunchy), GRIT gibt Koernung dazu, 12-BIT den 12-Bit-\n"
            "  Crunch, und SP-1200 legt den klassischen Mix mit einem Klick auf.\n\n"
            "Das Kit wird im Song (.retrotrax) mitgespeichert. Jedes Pad ist ein\n"
            "ganz normales Sample - der Akai-Filter, 12-Bit, Loop usw. lassen sich\n"
            "spaeter genauso darauf anwenden.",
            "The KIT button opens the drum kit in the style of the classic drum\n"
            "samplers (MPC60, E-mu SP-1200): a 4x4 pad grid with 16 OWN samples,\n"
            "separate from the 16 track slots - for finger drumming and sound design.\n\n"
            "- Click a pad to play it (it lights up). Where you click matters:\n"
            "  top of the pad = hard, loud hit, bottom = soft (velocity).\n"
            "- Finger-drum on the keyboard:\n"
            "  1 2 3 4 / Q W E R / A S D F / Z X C V (top left = pad 13, bottom left\n"
            "  = pad 1, like a real MPC).\n"
            "- LOAD puts a sample into the selected pad (double-clicking an empty pad\n"
            "  works too). CLEAR removes it again.\n"
            "- -> SLOT copies the pad into the current track slot so you can use it in\n"
            "  the tracker; SLOT -> grabs the sample from the current slot into the\n"
            "  pad. Sounds move freely both ways (TFMX-grabber loot or browser sounds\n"
            "  included).\n"
            "- ALLE IN SLOTS legt alle 16 Pads in die Instrument-Slots 1-16. Dann\n"
            "  DRUM-EINGABE einschalten (im Drumsampler) und das Panel schliessen:\n"
            "  die Pad-Tasten (1234/QWER/ASDF/YXCV) schreiben die Pads direkt in EINE\n"
            "  Spur - ein ganzer Beat auf einer Drum-Spur, ohne Oktaven zu wechseln\n"
            "  (mit REC auch live aufnehmbar). PAD IN SLOT / SLOT IN PAD schieben\n"
            "  einzelne Sounds zwischen Pad und Spur-Slot.\n"
            "- KITS (oben rechts): das ganze Kit als .retrokit-Datei speichern und\n"
            "  spaeter wieder laden (mit allen 16 Samples drin).\n"
            "- Per-pad character (SP-1200/Emu): TUNE tunes the pad in semitones (down\n"
            "  = fatter/crunchier), GRIT adds sample-rate reduction, 12-BIT adds the\n"
            "  12-bit crunch, and SP-1200 applies the classic combo in one click.\n\n"
            "The kit is saved with the song (.retrotrax). Each pad is an ordinary\n"
            "sample - the Akai filter, loop etc. can be applied to it later just the\n"
            "same.") });

    topics.add ({ loc::t ("Fairlight-Sample-Werkzeug", "Fairlight sample tool"),
        loc::t (
            "Der FAIRLIGHT-Knopf oeffnet das Sample-Werkzeug fuer den aktuellen Slot -\n"
            "ein kleiner Wellenform-Editor im Geiste des Fairlight CMI.\n\n"
            "- Mit der Maus ueber die Welle ziehen markiert einen Bereich. Beim\n"
            "  Abspielen wandert eine helle Laufmarke durch die Welle - du siehst, wo\n"
            "  du gerade bist.\n"
            "- WELLE erzeugt eine fertige Single-Cycle-Welle (Sinus/Saege/Rechteck/\n"
            "  Dreieck/Puls) zum Weiterbauen.\n"
            "- TRIMMEN schneidet auf die Markierung zu; AUSSCHNEIDEN entfernt die\n"
            "  Markierung und fuegt den Rest zusammen.\n"
            "- NORMALISIEREN hebt das Sample auf vollen Pegel.\n"
            "- UMKEHREN dreht es um.\n"
            "- FREIHAND: die Wellenform mit der Maus zeichnen - das Lichtgriffel-\n"
            "  Gefuehl des Fairlight. Ist der Slot leer, malst du von Null an.\n"
            "- IN KIT (16) schneidet das Sample in 16 gleiche Scheiben und legt sie\n"
            "  auf die Drum-Pads (z.B. einen Break zerlegen) - die klassische\n"
            "  MPC/SP-1200-Idee.\n"
            "- -> PATTERN schneidet in Instrument-Slots UND schreibt die Scheiben als\n"
            "  Noten ins aktuelle Pattern (Spur 1) - der Break wird spielbar und\n"
            "  umbaubar (Recycle/Page-R-Idee).\n"
            "- TIME-STRETCH: das Sample mit dem Regler laenger/kuerzer machen und mit\n"
            "  DEHNEN anwenden - die Tonhoehe bleibt gleich (granulare Dehnung).\n"
            "- LOOP/One-Shot: legt fest, ob das Sample einmal spielt oder in der\n"
            "  Schleife laeuft (wirkt beim Vorhoeren und Uebernehmen).\n"
            "- LOOP-PUNKT: Bereich markieren, dann klicken - die Schleife startet ab\n"
            "  dem Markierungs-Anfang (cyan markiert), nicht mehr von vorn.\n"
            "- SPEICHERN sichert das (bearbeitete) Sample als WAV-Datei auf die Platte\n"
            "  - auch wenn du es gar nicht im Song nutzt.\n"
            "- VORHOEREN spielt die Arbeitskopie; UEBERNEHMEN legt sie zurueck in den\n"
            "  Slot (als WAV gesichert, bleibt im Song erhalten).",
            "The FAIRLIGHT button opens the sample tool for the current slot - a small\n"
            "waveform editor in the spirit of the Fairlight CMI.\n\n"
            "- Drag across the wave to select a range.\n"
            "- TRIM cuts to the selection.\n"
            "- NORMALISE boosts the sample to full level.\n"
            "- REVERSE flips it.\n"
            "- FREEHAND: draw the waveform with the mouse - the Fairlight light-pen\n"
            "  feel. If the slot is empty, you paint from scratch.\n"
            "- TO KIT (16) slices the sample into 16 equal pieces and puts them on the\n"
            "  drum pads (e.g. to chop a break) - the classic MPC/SP-1200 idea.\n"
            "- -> PATTERN slices into instrument slots AND writes the slices as notes\n"
            "  into the current pattern (track 1) - the break becomes playable and\n"
            "  rearrangeable (Recycle/Page-R idea).\n"
            "- TIME-STRETCH: make the sample longer/shorter with the slider and apply\n"
            "  with STRETCH - the pitch stays the same (granular stretch).\n"
            "- PREVIEW plays the working copy; APPLY puts it back into the slot (saved\n"
            "  as a WAV, kept with the song).") });

    topics.add ({ loc::t ("FX: Master-Effekte (Echo & Hall)", "FX: master effects (echo & reverb)"),
        loc::t (
            "Der FX-Knopf, Reiter MASTER-EFFEKTE, wirkt auf den GANZEN Mix\n"
            "(alle Spuren zusammen), wie der Master-Track in Renoise. (Der erste\n"
            "Reiter AKAI-SAMPLER-EFFEKTE bearbeitet nur das aktuelle Sample.)\n\n"
            "- ECHO (Delay): ZEIT = Verzoegerung, RUECKKOPPLUNG = wie oft es\n"
            "  wiederholt, MIX = wie laut das Echo. MIX 0 = aus.\n"
            "- HALL (Reverb): RAUM = Groesse des Raums, MIX = wie viel Hall. MIX 0 = aus.\n"
            "- EQ (3-Band): BASS / MITTEN / HOEHEN anheben oder absenken (+/- dB,\n"
            "  0 = flach) - gibt dem Mix Druck oder nimmt Schaerfe raus.\n\n"
            "Beide sind Standard AUS, dein Klang bleibt also unveraendert, bis du MIX\n"
            "aufdrehst. Die Einstellungen werden im Song (.retrotrax) mitgespeichert.",
            "The FX button, MASTER FX tab, affects the WHOLE mix (all tracks\n"
            "together), like the master track in Renoise. (The first tab, AKAI\n"
            "SAMPLER FX, edits only the current sample.)\n\n"
            "- ECHO (delay): TIME = delay length, FEEDBACK = how often it repeats,\n"
            "  MIX = how loud the echo. MIX 0 = off.\n"
            "- REVERB: ROOM = room size, MIX = how much reverb. MIX 0 = off.\n"
            "- EQ (3-band): boost or cut LOW / MID / HIGH (+/- dB, 0 = flat) - gives\n"
            "  the mix punch or takes the harshness out.\n\n"
            "Both are off by default, so your sound stays unchanged until you turn MIX\n"
            "up. The settings are saved with the song (.retrotrax).") });

    topics.add ({ loc::t ("Swing / Groove", "Swing / groove"),
        loc::t (
            "Der SWING-Regler (oben neben BPM) gibt deinem Beat den Groove - das\n"
            "Geheimnis der alten MPC/SP-1200. Bei 0 % laeuft alles streng aufs Raster\n"
            "(gerade). Drehst du auf, werden die geraden und ungeraden Zeilen leicht\n"
            "gegeneinander versetzt (laenger/kuerzer im Wechsel) - der typische\n"
            "Shuffle/Swing. Das Tempo bleibt dabei gleich.\n\n"
            "Wenig (10-20 %) macht es schon lebendiger, mehr (30-50 %) ergibt den\n"
            "deutlichen HipHop/Funk-Schwung. Wird im Song mitgespeichert.",
            "The SWING slider (top, next to BPM) gives your beat its groove - the\n"
            "secret of the old MPC/SP-1200. At 0 % everything is strictly on the grid\n"
            "(straight). Turn it up and even/odd rows shift slightly against each\n"
            "other (alternating longer/shorter) - the classic shuffle/swing. The\n"
            "tempo stays the same.\n\n"
            "A little (10-20 %) already feels more alive, more (30-50 %) gives the\n"
            "clear hip-hop/funk swing. Saved with the song.") });

    topics.add ({ loc::t ("Wellenform erzeugen", "Generate waveform"),
        loc::t (
            "Im LADEN-Menue unter \"Wellenform erzeugen\" legst du eine einfache\n"
            "Single-Cycle-Welle (Sinus / Saege / Rechteck / Dreieck / Puls) in den\n"
            "aktuellen Slot. Sie ist auf Loop gestellt und klingt dauerhaft wie ein\n"
            "Oszillator - so baust du dir Synth-Sounds ganz ohne Sample. Mit dem\n"
            "Akai-Filter, Drive usw. (AKAI-Knopf) formst du sie weiter, oder du\n"
            "zeichnest sie im FAIRLIGHT-Werkzeug mit FREIHAND selbst.",
            "In the LOAD menu under \"Generate waveform\" you put a simple single-cycle\n"
            "wave (sine / saw / square / triangle / pulse) into the current slot. It is\n"
            "set to loop and sounds continuously like an oscillator - so you build\n"
            "synth sounds with no sample at all. Shape it further with the Akai filter,\n"
            "drive etc. (AKAI button), or draw your own with FREEHAND in the FAIRLIGHT\n"
            "tool.") });

    topics.add ({ loc::t ("Zufallsmelodie (WUERFEL)", "Random melody (DICE)"),
        loc::t (
            "Der WUERFEL-Knopf wuerfelt eine kleine Melodie in die aktuelle Spur -\n"
            "perfekt gegen das leere Blatt. Die Toene kommen aus einer Moll-\n"
            "Pentatonik (klingt fast immer gut zusammen), gebaut mit dem aktuellen\n"
            "Instrument rund um die gewaehlte Oktave. Gefaellt's nicht, einfach nochmal\n"
            "wuerfeln; mit Strg+Z geht's zurueck. Danach in Ruhe von Hand verfeinern.",
            "The DICE button rolls a little melody into the current track - perfect\n"
            "against the blank page. The notes come from a minor pentatonic (almost\n"
            "always sounds good together), built with the current instrument around the\n"
            "selected octave. Don't like it? Just roll again; Ctrl+Z undoes it. Then\n"
            "refine it by hand at your leisure.") });

    topics.add ({ loc::t ("Spektrum-Anzeige", "Spectrum display"),
        loc::t (
            "Der Knopf SPEKTRUM oben oeffnet die Frequenz-Anzeige - die tanzenden\n"
            "Balken aus den alten Trackern und Demos. Links stehen die tiefen, rechts\n"
            "die hohen Frequenzen; je lauter ein Bereich klingt, desto hoeher der\n"
            "Balken (eine weisse Kappe haelt kurz den hoechsten Wert).\n\n"
            "Das ist reine Optik - es aendert nichts am Klang. Praktisch, um zu sehen,\n"
            "ob dein Bass druckvoll ist oder die Hoehen fehlen. SCHLIESSEN (oder der\n"
            "Knopf erneut) blendet es wieder aus.",
            "The SPECTRUM button at the top opens the frequency display - the dancing\n"
            "bars from the old trackers and demos. Lows are on the left, highs on the\n"
            "right; the louder a range sounds, the higher its bar (a white cap briefly\n"
            "holds the peak).\n\n"
            "It's purely visual - it doesn't change the sound. Handy to see whether\n"
            "your bass has punch or the highs are missing. CLOSE (or the button again)\n"
            "hides it.") });

    topics.add ({ loc::t ("Sprache", "Language"),
        loc::t (
            "Der Knopf DE/EN oben schaltet zwischen Deutsch und Englisch um. Die\n"
            "Wahl wird gespeichert. Beim allerersten Start richtet RetroTrax sich\n"
            "nach der Sprache deines Systems.",
            "The DE/EN button at the top switches between German and English. Your\n"
            "choice is saved. On the very first start RetroTrax follows your\n"
            "system language.") });

    topics.add ({ loc::t ("Tipps & Dank", "Tips & thanks"),
        loc::t (
            "- Speichere oft. SONG SPEICHERN kostet nichts und schuetzt deine Arbeit.\n"
            "- Vorhoeren per Klick spart Zeit: erst stoebern, dann laden.\n"
            "- ESC schliesst Browser und Hilfe.\n\n"
            "RetroTrax ist und bleibt kostenlos. Wenn es dir Freude macht, freut\n"
            "sich das Mukkemann-Universum ueber einen Kaffee auf Ko-fi. Danke,\n"
            "dass du dabei bist! <3",
            "- Save often. SAVE SONG costs nothing and protects your work.\n"
            "- Previewing on click saves time: browse first, load second.\n"
            "- ESC closes the browser and this help.\n\n"
            "RetroTrax is and stays free. If it brings you joy, the Mukkemann\n"
            "universe would love a coffee on Ko-fi. Thank you for being here! <3") });

    currentTopic = juce::jlimit (0, topics.size() - 1, keep);
    topicList.updateContent();
    topicList.selectRow (currentTopic);
    showTopic (currentTopic);
}

void HelpPanel::showTopic (int index)
{
    if (index < 0 || index >= topics.size())
        return;
    currentTopic = index;
    bodyView.setText (topics.getReference (index).body, juce::dontSendNotification);
    bodyView.moveCaretToTop (false);
}

// ---- ListBox --------------------------------------------------------------

int HelpPanel::Model::getNumRows()
{
    return owner.topics.size();
}

void HelpPanel::Model::paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool selected)
{
    if (row < 0 || row >= owner.topics.size())
        return;
    if (selected)
        g.fillAll (rt::playBar);
    g.setFont (rt::mono (13.0f));
    g.setColour (selected ? juce::Colours::white : rt::text);
    g.drawText (owner.topics.getReference (row).title, 8, 0, w - 12, h,
                juce::Justification::centredLeft);
}

void HelpPanel::Model::selectedRowsChanged (int row)
{
    if (row >= 0)
        owner.showTopic (row);
}

// ---- Optik & Layout -------------------------------------------------------

bool HelpPanel::keyPressed (const juce::KeyPress& key)
{
    if (key.getKeyCode() == juce::KeyPress::escapeKey && onClose != nullptr)
    {
        onClose();
        return true;
    }
    return false;
}

void HelpPanel::paint (juce::Graphics& g)
{
    g.fillAll (rt::bg);
    g.setColour (rt::steel.withAlpha (0.7f));
    g.drawRect (getLocalBounds(), 1);

    g.setFont (rt::mono (15.0f, true));
    g.setColour (rt::cursor);
    g.drawText (loc::t ("HILFE", "HELP"), 12, 6, getWidth() - 24, 20,
                juce::Justification::centredLeft);

    g.setFont (rt::mono (12.0f));
    g.setColour (rt::textDim);
    g.drawText (loc::t ("Thema links waehlen - ESC schliesst.",
                        "Pick a topic on the left - ESC closes."),
                12, 26, getWidth() - 24, 16, juce::Justification::centredLeft);
}

void HelpPanel::resized()
{
    auto area = getLocalBounds().reduced (10);
    area.removeFromTop (38); // Titel (nur paint)

    auto bottom = area.removeFromBottom (30);
    closeButton.setBounds (bottom.removeFromRight (110).reduced (0, 2));
    area.removeFromBottom (6);

    topicList.setBounds (area.removeFromLeft (210));
    area.removeFromLeft (8);
    bodyView.setBounds (area);
}
