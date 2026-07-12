#include "PatternGrid.h"
#include "PluginProcessor.h"
#include "RetroLookAndFeel.h"
#include "Localisation.h"

PatternGrid::PatternGrid (RetroTraxProcessor& processor)
    : proc (processor), engine (processor.engine)
{
    setWantsKeyboardFocus (true);
    startTimerHz (25);
}

void PatternGrid::timerCallback()
{
    const bool playing = engine.playing.load();

    // Start des Abspielens: aktuelle Bearbeitungsstelle merken, damit der Cursor
    // nach dem Stoppen genau dorthin zurueckkehrt (und nicht beim Play-Balken
    // stehenbleibt). Gilt fuer Abspielen UND Aufnahme gleichermassen.
    if (playing && ! wasPlaying)
        savedCursorRow = cursorRow;

    if (playing)
    {
        // Der orange Eingabe-Cursor reitet jetzt auch beim reinen Abspielen auf
        // dem Play-Balken mit (vorher nur bei REC) - so sieht man immer, wo man
        // gerade ist, und die Ansicht folgt der blauen Linie.
        cursorRow = engine.currentRow.load();
        repaint();
    }
    else if (wasPlaying)
    {
        // Gerade gestoppt: Cursor an die vorherige Bearbeitungsstelle zuruecksetzen.
        if (savedCursorRow >= 0 && savedCursorRow < TrackerEngine::kRows)
            cursorRow = savedCursorRow;
        repaint();
    }

    wasPlaying = playing;
}

void PatternGrid::togglePlay()
{
    if (engine.playing.load())
        engine.stop();
    else
        engine.play();
    if (onTransportChange)
        onTransportChange();
    repaint();
}

void PatternGrid::moveCursor (int rowDelta, int colDelta)
{
    hasSelection = false; // einfache Navigation hebt eine Auswahl auf
    cursorRow = (cursorRow + rowDelta % TrackerEngine::kRows + TrackerEngine::kRows) % TrackerEngine::kRows;

    if (colDelta != 0)
    {
        // Im Anfaenger-Modus nur 2 Spalten (Note, Instrument); sonst alle 4.
        const int kCols = beginnerMode ? 2 : 4; // Note, Instrument[, Lautstaerke, Effekt]
        int flat = cursorTrack * kCols + cursorCol + colDelta;
        const int total = TrackerEngine::kTracks * kCols;
        flat = (flat % total + total) % total;
        cursorTrack = flat / kCols;
        cursorCol   = flat % kCols;
    }
    ensureTrackVisible();
    repaint();
    emitCursorInfo();
}

// Wie viele Spuren passen bei der aktuellen Fensterbreite nebeneinander? Wir
// peilen eine angenehme, gut lesbare Spaltenbreite an; auf einem breiten Fenster
// werden mehr gezeigt, auf einem schmalen weniger (mindestens eine).
int PatternGrid::visibleTracks() const
{
    constexpr int kTargetTrackW = 150; // Wunsch-Spaltenbreite (gut lesbar)
    const int fit = (getWidth() - kLeftW) / kTargetTrackW;
    return juce::jlimit (1, TrackerEngine::kTracks, fit);
}

// Den sichtbaren Ausschnitt so verschieben, dass die Cursor-Spur drin liegt.
void PatternGrid::ensureTrackVisible()
{
    const int nVis = visibleTracks();
    if (cursorTrack < firstVisTrack)
        firstVisTrack = cursorTrack;
    else if (cursorTrack >= firstVisTrack + nVis)
        firstVisTrack = cursorTrack - nVis + 1;
    firstVisTrack = juce::jlimit (0, juce::jmax (0, TrackerEngine::kTracks - nVis), firstVisTrack);
}

// Tastatur als Klavier, fuer deutsches QWERTZ-Layout:
// untere Reihe  y s x d c v g b h n j m  = aktuelle Oktave
// obere Reihe   q 2 w 3 e r 5 t 6 z 7 u i 9 o 0 p = eine Oktave hoeher
int PatternGrid::noteOffsetForChar (juce::juce_wchar c)
{
    switch (c)
    {
        case 'y': return 0;  case 's': return 1;  case 'x': return 2;
        case 'd': return 3;  case 'c': return 4;  case 'v': return 5;
        case 'g': return 6;  case 'b': return 7;  case 'h': return 8;
        case 'n': return 9;  case 'j': return 10; case 'm': return 11;
        case 'q': return 12; case '2': return 13; case 'w': return 14;
        case '3': return 15; case 'e': return 16; case 'r': return 17;
        case '5': return 18; case 't': return 19; case '6': return 20;
        case 'z': return 21; case '7': return 22; case 'u': return 23;
        case 'i': return 24; case '9': return 25; case 'o': return 26;
        case '0': return 27; case 'p': return 28;
        default:  return -1;
    }
}

juce::String PatternGrid::noteName (int note)
{
    static const char* names[] = { "C-", "C#", "D-", "D#", "E-", "F-",
                                   "F#", "G-", "G#", "A-", "A#", "B-" };
    if (note == TrackerEngine::kNoteOff)
        return "OFF";
    if (note < 0)
        return "---";
    return juce::String (names[note % 12]) + juce::String (note / 12);
}

bool PatternGrid::handleNoteKey (juce::juce_wchar c)
{
    const int offset = noteOffsetForChar (c);
    if (offset < 0)
        return false;

    const int note = juce::jlimit (0, TrackerEngine::kMaxNote,
                                   proc.currentOctave.load() * 12 + offset);

    // Waehrend der Song laeuft: LIVE mitspielen UND die Note an der gerade
    // laufenden Stelle aufnehmen (wie ein mitgeschnittenes Klavierspiel). Sie
    // landet im klingenden Pattern auf der gerade abspielenden Zeile, in der
    // Spur des Cursors. Der Cursor bleibt stehen - der Abspielkopf wandert selbst.
    if (engine.playing.load())
    {
        engine.audition (note, proc.currentInstrument.load()); // immer hoerbar (zum Testen)
        // Nur mit scharfer Aufnahme (REC) landet die Note auch im Pattern.
        if (engine.recording.load())
        {
            const int rrow = engine.currentRow.load();
            const int ppat = engine.displayPattern();
            if (rrow >= 0 && rrow < TrackerEngine::kRows
                && ppat >= 0 && ppat < TrackerEngine::kMaxPatterns)
            {
                auto& cell = engine.patterns[ppat][rrow][cursorTrack];
                cell.note       = note;
                cell.instrument = proc.currentInstrument.load();
            }
        }
        repaint();
        return true;
    }

    // Gestoppt: ohne REC nur testen (Note hoeren); erst mit REC landet sie als
    // Step-Eingabe im Pattern. So kann man Melodien/Samples frei ausprobieren.
    if (! engine.recording.load())
    {
        engine.audition (note, proc.currentInstrument.load());
        if (onCursorInfo)
            onCursorInfo (loc::t ("Vorhoeren - zum Aufnehmen REC druecken",
                                  "Preview - press REC to record"));
        return true;
    }

    pushUndo();
    auto& cell = engine.cells[cursorRow][cursorTrack];
    cell.note = note;
    cell.instrument = proc.currentInstrument.load();

    engine.audition (note, cell.instrument);
    moveCursor (1, 0); // im Stopp: nach der Eingabe eine Zeile weiter
    return true;
}

bool PatternGrid::handleDigitKey (juce::juce_wchar c)
{
    if (c < '0' || c > '9')
        return false;

    pushUndo();
    const int digit = (int) (c - '0');
    auto& cell = engine.cells[cursorRow][cursorTrack];

    if (cursorCol == 1)
    {
        int v = cell.instrument < 0 ? 0 : cell.instrument;
        v = (v * 10 + digit) % 100;
        if (v >= TrackerEngine::kInstruments)
            v = digit;
        cell.instrument = juce::jmin (v, TrackerEngine::kInstruments - 1);
    }
    else
    {
        int v = cell.volume < 0 ? 0 : cell.volume;
        v = (v * 10 + digit) % 100;
        if (v > 64)
            v = digit;
        cell.volume = juce::jmin (v, 64);
    }
    repaint();
    emitCursorInfo();
    return true;
}

// Effekt-Spalte: Hex-Ziffern (0-9, A-F) werden von links eingeschoben.
// Die drei Stellen sind Befehl + zwei Parameter, z.B. C-4-0 -> Effekt C, Parameter 40.
bool PatternGrid::handleEffectKey (juce::juce_wchar c)
{
    int digit = -1;
    if (c >= '0' && c <= '9')      digit = (int) (c - '0');
    else if (c >= 'a' && c <= 'f') digit = 10 + (int) (c - 'a');
    if (digit < 0)
        return false;

    pushUndo();
    auto& cell = engine.cells[cursorRow][cursorTrack];
    int combined = cell.effect < 0 ? 0 : (((cell.effect & 0xF) << 8) | (cell.effectParam & 0xFF));
    combined = ((combined << 4) | digit) & 0xFFF; // 12 Bit: 1 Stelle Befehl + 2 Stellen Parameter
    cell.effect      = (combined >> 8) & 0xF;
    cell.effectParam =  combined       & 0xFF;
    repaint();
    emitCursorInfo(); // Effekt-Bedeutung live mitschreiben
    return true;
}

juce::String PatternGrid::effectText (int effect, int param)
{
    if (effect < 0)
        return "...";
    return juce::String::toHexString (effect).toUpperCase()
         + juce::String::formatted ("%02X", param & 0xFF);
}

void PatternGrid::trackHeaderButtons (int tx, int trackW,
                                      juce::Rectangle<int>& muteR,
                                      juce::Rectangle<int>& soloR)
{
    const int bw    = juce::jlimit (16, 30, trackW / 5); // Knopfbreite, mit der Spur wachsend
    const int bh    = 15;
    const int gap   = 6;
    const int total = bw * 2 + gap;
    const int x0    = tx + (trackW - total) / 2;         // mittig unter dem Spurnamen
    const int y0    = kHeaderH - bh - 3;                 // unten im Kopf
    muteR = { x0,            y0, bw, bh };
    soloR = { x0 + bw + gap, y0, bw, bh };
}

// --- Live-Hilfe an der Cursor-Stelle --------------------------------------

void PatternGrid::setLiveHelp (bool on)
{
    liveHelp = on;
    if (on)
        emitCursorInfo(); // sofort die aktuelle Stelle erklaeren
}

void PatternGrid::setBeginnerMode (bool on)
{
    beginnerMode = on;
    // Cursor koennte in einer jetzt ausgeblendeten Spalte stehen -> auf Instrument
    // zurueckholen, damit er sichtbar bleibt.
    if (beginnerMode && cursorCol > 1)
        cursorCol = 1;
    repaint();
}

void PatternGrid::emitCursorInfo()
{
    if (liveHelp && onCursorInfo)
        onCursorInfo (cursorHelpText());
}

// Klartext-Bedeutung eines Effekts (zweisprachig), z.B. "C40 -> Lautstaerke auf 64".
juce::String PatternGrid::effectHelp (int effect, int param)
{
    if (effect < 0)
        return loc::t ("Effekt: leer - tippe 3 Hex-Stellen, z.B. C40 (A-F + 0-9)",
                       "Effect: empty - type 3 hex digits, e.g. C40 (A-F + 0-9)");

    const int x = (param >> 4) & 0xF, y = param & 0xF;
    const auto pre = effectText (effect, param) + "  ->  ";

    switch (effect)
    {
        case 0x0:
            if (param == 0)
                return pre + loc::t ("Arpeggio aus (Parameter 0)", "arpeggio off (param 0)");
            return pre + loc::t ("Arpeggio: Grundton, +" , "arpeggio: root, +")
                 + juce::String (x) + loc::t (" und +", " and +") + juce::String (y)
                 + loc::t (" Halbtoene im Wechsel", " semitones, alternating");
        case 0x1: return pre + loc::t ("Tonhoehe gleitet hoch", "pitch slides up");
        case 0x2: return pre + loc::t ("Tonhoehe gleitet runter", "pitch slides down");
        case 0x3: return pre + loc::t ("Tone-Portamento: zur neuen Note hingleiten",
                                       "tone portamento: glide to the new note");
        case 0x4: return pre + loc::t ("Vibrato: Tempo ", "vibrato: speed ") + juce::String (x)
                             + loc::t (", Tiefe ", ", depth ") + juce::String (y);
        case 0x8: return pre + loc::t ("Panorama: 00 = links, 80 = Mitte, FF = rechts",
                                       "panning: 00 = left, 80 = center, FF = right");
        case 0xA: return pre + loc::t ("Lautstaerke-Slide: +", "volume slide: +") + juce::String (x)
                             + loc::t (" / -", " / -") + juce::String (y) + loc::t (" pro Tick", " per tick");
        case 0xC: return pre + loc::t ("Lautstaerke setzen auf ", "set volume to ")
                             + juce::String (juce::jmin (param, 64)) + " (0-64)";
        case 0xF:
            if (param > 0 && param < 0x20)
                return pre + loc::t ("Tempo: ", "tempo: ") + juce::String (param)
                           + loc::t (" Ticks pro Zeile (Speed)", " ticks per row (speed)");
            return pre + loc::t ("Tempo: ", "tempo: ") + juce::String (param) + " BPM";
        default:
            return pre + loc::t ("Effekt noch ohne Funktion", "effect not in use yet");
    }
}

juce::String PatternGrid::cursorHelpText() const
{
    const auto& cell = engine.cells[cursorRow][cursorTrack];
    const auto where = loc::t ("Spur ", "Track ") + juce::String (cursorTrack + 1)
                     + loc::t (", Zeile ", ", row ") + juce::String::formatted ("%02d", cursorRow) + "  |  ";

    switch (cursorCol)
    {
        case 0:
            if (cell.note == TrackerEngine::kNoteOff)
                return where + loc::t (
                    "NOTE-AUS (OFF): laesst eine SID-Stimme ausklingen. Taste 1 setzt sie, ENTF loescht.",
                    "NOTE OFF: lets a SID voice fade out. Key 1 sets it, DEL clears.");
            return where + loc::t (
                "NOTE: Tastatur spielt Toene (y s x d c v g b h n j m), Reihe darueber = hoeher; +/- Oktave; 1 = Note-Aus",
                "NOTE: keyboard plays notes (y s x d c v g b h n j m), row above = higher; +/- octave; 1 = note off");
        case 1:
            return where + loc::t ("INSTRUMENT: Ziffern 01-16 waehlen das Sample",
                                   "INSTRUMENT: digits 01-16 pick the sample");
        case 2:
            return where + loc::t ("LAUTSTAERKE: 00 (leer = voll) bis 64",
                                   "VOLUME: 00 (empty = full) up to 64");
        default:
            return where + loc::t ("EFFEKT  ", "EFFECT  ") + effectHelp (cell.effect, cell.effectParam);
    }
}

// --- Rueckgaengig/Wiederholen + Spalten-Zwischenablage --------------------

PatternGrid::Snapshot PatternGrid::takeSnapshot() const
{
    Snapshot s;
    const juce::ScopedLock sl (engine.lock);
    for (int r = 0; r < TrackerEngine::kRows; ++r)
        for (int t = 0; t < TrackerEngine::kTracks; ++t)
            s.cells[r][t] = engine.cells[r][t];
    return s;
}

void PatternGrid::restore (const Snapshot& s)
{
    {
        const juce::ScopedLock sl (engine.lock);
        for (int r = 0; r < TrackerEngine::kRows; ++r)
            for (int t = 0; t < TrackerEngine::kTracks; ++t)
                engine.cells[r][t] = s.cells[r][t];
    }
    repaint();
}

void PatternGrid::quantize (int step)
{
    if (step < 2)
        return; // 1 = schon auf dem Raster, nichts zu tun

    pushUndo(); // mit Strg+Z umkehrbar
    {
        const juce::ScopedLock sl (engine.lock);
        for (int t = 0; t < TrackerEngine::kTracks; ++t)
        {
            // Spalte einsammeln + leeren, dann jede Note auf die naechste Rasterzeile setzen.
            TrackerEngine::Cell col[TrackerEngine::kRows];
            for (int r = 0; r < TrackerEngine::kRows; ++r)
            {
                col[r] = engine.cells[r][t];
                engine.cells[r][t] = TrackerEngine::Cell();
            }
            for (int r = 0; r < TrackerEngine::kRows; ++r)
            {
                const auto& c = col[r];
                const bool empty = (c.note < 0 && c.instrument < 0 && c.volume < 0 && c.effect < 0);
                if (empty)
                    continue;
                int tr = ((r + step / 2) / step) * step;          // naechstes Vielfaches
                tr = juce::jlimit (0, TrackerEngine::kRows - 1, tr);
                engine.cells[tr][t] = c;                            // bei Kollision gewinnt die letzte
            }
        }
    }
    repaint();
    emitCursorInfo();
}

// 4x4-Pad-Layout wie im KIT-Panel: 1234 / QWER / ASDF / YXCV (unten links = Pad 1).
int PatternGrid::drumPadFromChar (juce::juce_wchar c) const
{
    const int lc = (int) juce::CharacterFunctions::toLowerCase (c);
    static const char* const rows[4] = { "1234", "qwer", "asdf", "yxcv" };
    for (int r = 0; r < 4; ++r)
        for (int col = 0; col < 4; ++col)
            if (lc == (int) rows[r][col])
                return (3 - r) * 4 + col;
    if (lc == (int) 'z') return (3 - 3) * 4 + 0; // QWERTY-Variante = Pad 1
    return -1;
}

void PatternGrid::enterDrum (int pad)
{
    if (pad < 0 || pad >= TrackerEngine::kInstruments)
        return;
    engine.audition (60, pad); // Slot = Pad-Index (Kit -> Slots 1-16 vorausgesetzt)
    if (engine.playing.load())
    {
        if (engine.recording.load())
        {
            const int rrow = engine.currentRow.load();
            const int ppat = engine.displayPattern();
            if (rrow >= 0 && rrow < TrackerEngine::kRows
                && ppat >= 0 && ppat < TrackerEngine::kMaxPatterns)
            {
                auto& cell = engine.patterns[ppat][rrow][cursorTrack];
                cell.note       = 60;
                cell.instrument = pad;
            }
        }
        repaint();
        return;
    }
    if (! engine.recording.load())
        return; // ohne REC nur vorhoeren (oben schon angespielt), nicht schreiben
    pushUndo();
    auto& cell = engine.cells[cursorRow][cursorTrack];
    cell.note       = 60;
    cell.instrument = pad;
    moveCursor (1, 0);
}

void PatternGrid::randomMelody()
{
    pushUndo(); // mit Strg+Z umkehrbar
    static const int pent[10] = { 0, 3, 5, 7, 10, 12, 15, 17, 19, 22 }; // Moll-Pentatonik, 2 Oktaven
    auto& rng = juce::Random::getSystemRandom();
    const int inst = proc.currentInstrument.load();
    const int root = juce::jlimit (0, 96, proc.currentOctave.load() * 12); // C der Oktave
    {
        const juce::ScopedLock sl (engine.lock);
        const int t = juce::jlimit (0, TrackerEngine::kTracks - 1, cursorTrack);
        for (int r = 0; r < TrackerEngine::kRows; ++r)
            engine.cells[r][t] = TrackerEngine::Cell();
        for (int r = 0; r < TrackerEngine::kRows; r += 2) // Achtel-Raster
        {
            if (rng.nextFloat() < 0.7f)
            {
                auto& c = engine.cells[r][t];
                c.note       = juce::jlimit (0, TrackerEngine::kMaxNote, root + pent[rng.nextInt (10)]);
                c.instrument = inst;
            }
        }
    }
    repaint();
    emitCursorInfo();
}

void PatternGrid::pushUndo()
{
    undoStack.push_back (takeSnapshot());
    if ((int) undoStack.size() > kMaxUndo)
        undoStack.erase (undoStack.begin());
    redoStack.clear(); // ein neuer Schritt verwirft den Wiederholen-Pfad
}

void PatternGrid::undo()
{
    if (undoStack.empty())
        return;
    redoStack.push_back (takeSnapshot());
    restore (undoStack.back());
    undoStack.pop_back();
}

void PatternGrid::redo()
{
    if (redoStack.empty())
        return;
    undoStack.push_back (takeSnapshot());
    restore (redoStack.back());
    redoStack.pop_back();
}

void PatternGrid::copyTrack()
{
    const juce::ScopedLock sl (engine.lock);
    for (int r = 0; r < TrackerEngine::kRows; ++r)
        clipColumn[r] = engine.cells[r][cursorTrack];
    hasClip = true;
    hasBlockClip = false; // Spur kopiert -> Block-Ablage tritt zurueck
}

void PatternGrid::cutTrack()
{
    copyTrack();
    pushUndo();
    {
        const juce::ScopedLock sl (engine.lock);
        for (int r = 0; r < TrackerEngine::kRows; ++r)
            engine.cells[r][cursorTrack] = TrackerEngine::Cell();
    }
    repaint();
}

void PatternGrid::pasteTrack()
{
    if (! hasClip)
        return;
    pushUndo();
    {
        const juce::ScopedLock sl (engine.lock);
        for (int r = 0; r < TrackerEngine::kRows; ++r)
            engine.cells[r][cursorTrack] = clipColumn[r];
    }
    repaint();
}

// --- Block-Bearbeitung -----------------------------------------------------

void PatternGrid::extendSelection (int rowDelta, int trackDelta)
{
    if (! hasSelection)
    {
        hasSelection   = true;
        selAnchorRow   = cursorRow;
        selAnchorTrack = cursorTrack;
    }
    // Bei Auswahl NICHT umwickeln - sonst springt das Rechteck quer durchs Pattern.
    cursorRow   = juce::jlimit (0, TrackerEngine::kRows   - 1, cursorRow   + rowDelta);
    cursorTrack = juce::jlimit (0, TrackerEngine::kTracks - 1, cursorTrack + trackDelta);
    ensureTrackVisible();
    repaint();
    emitCursorInfo();
}

void PatternGrid::clearSelection()
{
    if (hasSelection)
    {
        hasSelection = false;
        repaint();
    }
}

void PatternGrid::blockRect (int& r0, int& r1, int& t0, int& t1) const
{
    if (hasSelection)
    {
        r0 = juce::jmin (selAnchorRow,   cursorRow);
        r1 = juce::jmax (selAnchorRow,   cursorRow);
        t0 = juce::jmin (selAnchorTrack, cursorTrack);
        t1 = juce::jmax (selAnchorTrack, cursorTrack);
    }
    else // ohne Auswahl: nur die Zelle unterm Cursor
    {
        r0 = r1 = cursorRow;
        t0 = t1 = cursorTrack;
    }
}

void PatternGrid::copyBlock()
{
    int r0, r1, t0, t1; blockRect (r0, r1, t0, t1);
    blockClip.assign (r1 - r0 + 1, std::vector<TrackerEngine::Cell> (t1 - t0 + 1));
    const juce::ScopedLock sl (engine.lock);
    for (int r = r0; r <= r1; ++r)
        for (int t = t0; t <= t1; ++t)
            blockClip[r - r0][t - t0] = engine.cells[r][t];
    hasBlockClip = true;
    hasClip = false; // Block kopiert -> Spur-Ablage tritt zurueck
}

void PatternGrid::cutBlock()
{
    copyBlock();
    pushUndo();
    int r0, r1, t0, t1; blockRect (r0, r1, t0, t1);
    {
        const juce::ScopedLock sl (engine.lock);
        for (int r = r0; r <= r1; ++r)
            for (int t = t0; t <= t1; ++t)
                engine.cells[r][t] = TrackerEngine::Cell();
    }
    repaint();
}

void PatternGrid::pasteBlock()
{
    if (! hasBlockClip || blockClip.empty())
        return;
    pushUndo();
    const int rows = (int) blockClip.size();
    const int trks = (int) blockClip[0].size();
    {
        const juce::ScopedLock sl (engine.lock);
        for (int r = 0; r < rows; ++r)
            for (int t = 0; t < trks; ++t)
            {
                const int dr = cursorRow + r, dt = cursorTrack + t;
                if (dr >= 0 && dr < TrackerEngine::kRows && dt >= 0 && dt < TrackerEngine::kTracks)
                    engine.cells[dr][dt] = blockClip[r][t];
            }
    }
    repaint();
}

void PatternGrid::nudgeBlock (int rowDelta, int trackDelta)
{
    int r0, r1, t0, t1; blockRect (r0, r1, t0, t1);

    // Der Zielbereich darf nicht aus dem Pattern hinauslaufen.
    if (r0 + rowDelta   < 0 || r1 + rowDelta   >= TrackerEngine::kRows
     || t0 + trackDelta < 0 || t1 + trackDelta >= TrackerEngine::kTracks)
        return;

    pushUndo();
    const int rows = r1 - r0 + 1, trks = t1 - t0 + 1;
    std::vector<std::vector<TrackerEngine::Cell>> tmp (rows, std::vector<TrackerEngine::Cell> (trks));
    {
        const juce::ScopedLock sl (engine.lock);
        for (int r = 0; r < rows; ++r)            // Block sichern
            for (int t = 0; t < trks; ++t)
                tmp[r][t] = engine.cells[r0 + r][t0 + t];
        for (int r = r0; r <= r1; ++r)            // Quelle leeren
            for (int t = t0; t <= t1; ++t)
                engine.cells[r][t] = TrackerEngine::Cell();
        for (int r = 0; r < rows; ++r)            // am neuen Ort einsetzen
            for (int t = 0; t < trks; ++t)
                engine.cells[r0 + rowDelta + r][t0 + trackDelta + t] = tmp[r][t];
    }

    // Cursor (und ggf. die Auswahl) mit dem Block mitziehen.
    cursorRow   += rowDelta;
    cursorTrack += trackDelta;
    if (hasSelection)
    {
        selAnchorRow   += rowDelta;
        selAnchorTrack += trackDelta;
    }
    ensureTrackVisible();

    // "Nudgen und hoeren": im Stopp die Note unterm Cursor gleich anspielen.
    if (! engine.playing.load())
    {
        const auto& c = engine.cells[cursorRow][cursorTrack];
        if (c.note >= 0)
            engine.audition (c.note, c.instrument);
    }
    repaint();
    emitCursorInfo();
}

bool PatternGrid::keyPressed (const juce::KeyPress& key)
{
    const auto code = key.getKeyCode();
    const auto c    = juce::CharacterFunctions::toLowerCase (key.getTextCharacter());
    const auto mods = key.getModifiers();

    // Strg-Kombinationen zuerst, damit C/V/X/Z nicht als Noten landen.
    // Mit aktiver Auswahl arbeiten C/X/V auf dem ganzen Block, sonst spaltenweise.
    // WICHTIG: bei gedrueckter Strg-Taste liefert Windows als Text-Zeichen ein
    // Steuerzeichen (Strg+C -> Code 3), nicht 'c'. Darum ueber den Tasten-Code
    // pruefen (plattformrobust), nicht ueber getTextCharacter().
    if (mods.isCommandDown())
    {
        const int kc = key.getKeyCode();
        const auto is = [kc, c] (char letter)
        {
            const int upper = letter - 'a' + 'A';
            return kc == upper || kc == letter || c == (juce::juce_wchar) letter;
        };

        if (is ('z')) { mods.isShiftDown() ? redo() : undo(); return true; }
        if (is ('y')) { redo(); return true; }
        if (is ('c')) { hasSelection ? copyBlock() : copyTrack(); return true; }
        if (is ('x')) { hasSelection ? cutBlock()  : cutTrack();  return true; }
        if (is ('v')) { hasBlockClip ? pasteBlock() : pasteTrack(); return true; }
        return false; // andere Strg-Kombis ignorieren (keine versehentliche Note)
    }

    // Alt+Pfeil: markierten Block (oder die Zelle unterm Cursor) direkt verschieben.
    if (mods.isAltDown())
    {
        if (code == juce::KeyPress::upKey)    { nudgeBlock (-1, 0); return true; }
        if (code == juce::KeyPress::downKey)  { nudgeBlock ( 1, 0); return true; }
        if (code == juce::KeyPress::leftKey)  { nudgeBlock (0, -1); return true; }
        if (code == juce::KeyPress::rightKey) { nudgeBlock (0,  1); return true; }
    }

    // Umschalt+Pfeil: rechteckige Auswahl aufziehen / erweitern.
    if (mods.isShiftDown())
    {
        if (code == juce::KeyPress::upKey)    { extendSelection (-1, 0); return true; }
        if (code == juce::KeyPress::downKey)  { extendSelection ( 1, 0); return true; }
        if (code == juce::KeyPress::leftKey)  { extendSelection (0, -1); return true; }
        if (code == juce::KeyPress::rightKey) { extendSelection (0,  1); return true; }
    }

    if (code == juce::KeyPress::spaceKey)           { togglePlay(); return true; }
    if (code == juce::KeyPress::upKey)              { moveCursor (-1, 0); return true; }
    if (code == juce::KeyPress::downKey)            { moveCursor (1, 0);  return true; }
    if (code == juce::KeyPress::leftKey)            { moveCursor (0, -1); return true; }
    if (code == juce::KeyPress::rightKey)           { moveCursor (0, 1);  return true; }
    if (code == juce::KeyPress::pageUpKey)          { moveCursor (-16, 0); return true; }
    if (code == juce::KeyPress::pageDownKey)        { moveCursor (16, 0);  return true; }
    if (code == juce::KeyPress::homeKey)            { clearSelection(); cursorRow = 0; repaint(); return true; }
    if (code == juce::KeyPress::endKey)             { clearSelection(); cursorRow = TrackerEngine::kRows - 1; repaint(); return true; }

    if (code == juce::KeyPress::tabKey)
    {
        clearSelection();
        const int delta = key.getModifiers().isShiftDown() ? -1 : 1;
        cursorTrack = (cursorTrack + delta + TrackerEngine::kTracks) % TrackerEngine::kTracks;
        cursorCol = 0;
        ensureTrackVisible();
        repaint();
        return true;
    }

    if (code == juce::KeyPress::deleteKey || code == juce::KeyPress::backspaceKey)
    {
        pushUndo();
        auto& cell = engine.cells[cursorRow][cursorTrack];
        if (cursorCol == 0)      cell = TrackerEngine::Cell();
        else if (cursorCol == 1) cell.instrument = -1;
        else if (cursorCol == 2) cell.volume = -1;
        else                     { cell.effect = -1; cell.effectParam = 0; }
        moveCursor (1, 0);
        return true;
    }

    if (c == '+') { proc.currentOctave = juce::jmin (8, proc.currentOctave.load() + 1); repaint(); return true; }
    if (c == '-') { proc.currentOctave = juce::jmax (1, proc.currentOctave.load() - 1); repaint(); return true; }

    // Drum-Eingabe: die Pad-Tasten schreiben das Pad/Slot direkt in die Spur.
    if (proc.drumInput.load())
    {
        const int pad = drumPadFromChar (c);
        if (pad >= 0) { enterDrum (pad); return true; }
    }

    if (cursorCol == 0)
    {
        // Taste 1 = Note-Aus: laesst eine SID-Stimme ausklingen (oder stoppt ein Sample).
        if (c == '1')
        {
            if (engine.playing.load())
            {
                // Live-Aufnahme nur bei scharfer Aufnahme (REC).
                if (engine.recording.load())
                {
                    const int rrow = engine.currentRow.load();
                    const int ppat = engine.displayPattern();
                    if (rrow >= 0 && rrow < TrackerEngine::kRows
                        && ppat >= 0 && ppat < TrackerEngine::kMaxPatterns)
                    {
                        auto& cell = engine.patterns[ppat][rrow][cursorTrack];
                        cell.note = TrackerEngine::kNoteOff;
                        cell.instrument = -1;
                    }
                }
                repaint();
                return true;
            }
            pushUndo();
            auto& cell = engine.cells[cursorRow][cursorTrack];
            cell.note = TrackerEngine::kNoteOff;
            cell.instrument = -1;
            moveCursor (1, 0);
            repaint();
            emitCursorInfo();
            return true;
        }
        if (handleNoteKey (c))
            return true;
    }
    else if (cursorCol == 3)
    {
        if (handleEffectKey (c))
            return true;
    }
    else if (handleDigitKey (c))
    {
        return true;
    }

    return false;
}

void PatternGrid::mouseDown (const juce::MouseEvent& e)
{
    grabKeyboardFocus();

    const int nVis = visibleTracks();
    const int trackW = (getWidth() - kLeftW) / nVis;
    if (trackW <= 0)
        return;

    // Klick auf die M/S-Knoepfe im Spurkopf: Spur stumm bzw. solo schalten
    // (ohne Cursor/Zeile zu veraendern). Solo schlaegt Mute (siehe TrackerEngine).
    if (e.y < kHeaderH && e.x > kLeftW)
    {
        const int vi = juce::jmin (nVis - 1, (e.x - kLeftW) / trackW);
        const int t  = juce::jmin (TrackerEngine::kTracks - 1, firstVisTrack + vi);
        const int tx = kLeftW + vi * trackW;
        juce::Rectangle<int> muteR, soloR;
        trackHeaderButtons (tx, trackW, muteR, soloR);
        if (muteR.contains (e.x, e.y)) { engine.trackMute[t] = ! engine.trackMute[t].load(); repaint(); return; }
        if (soloR.contains (e.x, e.y)) { engine.trackSolo[t] = ! engine.trackSolo[t].load(); repaint(); return; }
    }

    const bool playing = engine.playing.load();
    const int focusRow = playing ? engine.currentRow.load() : cursorRow;
    const int visRows  = juce::jmax (1, (getHeight() - kHeaderH) / kRowH);

    if (e.x > kLeftW)
    {
        const int vi  = juce::jmin (nVis - 1, (e.x - kLeftW) / trackW);
        const int t   = juce::jmin (TrackerEngine::kTracks - 1, firstVisTrack + vi);
        const int xin = (e.x - kLeftW) % trackW;
        cursorTrack = t;
        // Anfaenger-Modus: nur Note/Instrument anklickbar; sonst alle vier Spalten.
        cursorCol = beginnerMode
                  ? (xin < trackW * 55 / 100 ? 0 : 1)
                  : (xin < trackW * 34 / 100 ? 0
                   : xin < trackW * 49 / 100 ? 1
                   : xin < trackW * 65 / 100 ? 2 : 3);
    }
    if (e.y > kHeaderH)
    {
        const int line = (e.y - kHeaderH) / kRowH;
        const int row  = focusRow + (line - visRows / 2);
        if (row >= 0 && row < TrackerEngine::kRows)
            cursorRow = row;
    }
    repaint();
    emitCursorInfo();
}

void PatternGrid::paint (juce::Graphics& g)
{
    const int w = getWidth();
    const int h = getHeight();
    g.fillAll (rt::bg);

    // Nur den sichtbaren Spuren-Ausschnitt zeichnen (16 Spuren passen nicht alle
    // nebeneinander). Die sichtbaren Spuren fuellen die Breite voll aus.
    ensureTrackVisible();
    const int nVis = visibleTracks();
    const int firstT = firstVisTrack;
    const int trackW = (w - kLeftW) / nVis;
    // Zellen-Schrift waechst/schrumpft mit der Spurbreite, damit die zweistelligen
    // Instrument-/Lautstaerke-Zahlen in der schmalen Spalte (14 % der Spur) immer
    // ganz reinpassen - egal wie klein das Fenster gezogen ist.
    const float cellFontH = juce::jlimit (9.0f, 15.0f, (float) trackW * 0.11f);
    const bool playing = engine.playing.load();
    const int focusRow = playing ? engine.currentRow.load() : cursorRow;
    const int visRows  = juce::jmax (1, (h - kHeaderH) / kRowH);
    const int centerLine = visRows / 2;
    const int centerY = kHeaderH + centerLine * kRowH;
    const int dispPat = engine.displayPattern(); // im Song-Lauf das klingende Pattern

    // Auswahl-Rechteck (Anker .. Cursor), nur im Stopp sinnvoll sichtbar.
    const int selR0 = juce::jmin (selAnchorRow,   cursorRow);
    const int selR1 = juce::jmax (selAnchorRow,   cursorRow);
    const int selT0 = juce::jmin (selAnchorTrack, cursorTrack);
    const int selT1 = juce::jmax (selAnchorTrack, cursorTrack);

    g.setFont (rt::mono (15.0f));

    // Mittelbalken (aktuelle Zeile)
    g.setColour (playing ? rt::playBar : rt::centerBar);
    g.fillRect (0, centerY, w, kRowH);

    for (int line = 0; line < visRows; ++line)
    {
        const int row = focusRow + (line - centerLine);
        if (row < 0 || row >= TrackerEngine::kRows)
            continue;

        const int y = kHeaderH + line * kRowH;

        if (row % 4 == 0 && line != centerLine)
        {
            g.setColour (rt::rowBeat);
            g.fillRect (0, y, w, kRowH);
        }

        g.setColour (row % 4 == 0 ? rt::steelHi : rt::textDim);
        g.drawText (juce::String::formatted ("%02d", row),
                    4, y, kLeftW - 10, kRowH, juce::Justification::centredRight);

        for (int vi = 0; vi < nVis; ++vi)
        {
            const int t = firstT + vi;
            if (t >= TrackerEngine::kTracks)
                break;
            const auto& cell = engine.patterns[dispPat][row][t];
            const int tx = kLeftW + vi * trackW;

            // Markierter Block: zarte Fuellung hinter den Zellen.
            if (hasSelection && ! playing
                && row >= selR0 && row <= selR1 && t >= selT0 && t <= selT1)
            {
                g.setColour (rt::cursor.withAlpha (0.16f));
                g.fillRect (tx, y, trackW, kRowH);
            }

            // Im Anfaenger-Modus nur Note + Instrument, dafuer breiter und luftiger;
            // im Profi-Modus zusaetzlich Lautstaerke + Effekt (die schmalen Hex-Spalten).
            const int noteX = tx + 6;
            const int noteW = beginnerMode ? trackW * 52 / 100 : trackW * 30 / 100;
            const int instX = tx + (beginnerMode ? trackW * 58 / 100 : trackW * 34 / 100);
            const int instW = beginnerMode ? trackW * 36 / 100 : trackW * 14 / 100;
            const int volX  = tx + trackW * 49 / 100;
            const int volW  = trackW * 14 / 100;
            const int fxX   = tx + trackW * 65 / 100;
            const int fxW   = trackW * 33 / 100;

            // Cursor-Markierung - waehrend des Abspielens reitet sie auf dem
            // Play-Balken mit (cursorRow folgt der Abspielzeile, siehe timerCallback),
            // sodass man live sieht, wo in der Spur man gerade aufnimmt.
            if (row == cursorRow && t == cursorTrack)
            {
                const int cx = cursorCol == 0 ? noteX - 3 : (cursorCol == 1 ? instX - 3
                             : cursorCol == 2 ? volX  - 3 : fxX - 3);
                const int cw = cursorCol == 0 ? noteW + 6 : (cursorCol == 1 ? instW + 6
                             : cursorCol == 2 ? volW  + 6 : fxW + 6);
                const juce::Rectangle<float> cr ((float) cx, (float) y + 1.0f,
                                                 (float) cw, (float) kRowH - 2.0f);
                // Deutliche Fuellung + kraeftiger Rahmen: man sieht sofort, in
                // welcher der vier Spalten man gerade tippt.
                g.setColour (rt::cursor.withAlpha (0.30f));
                g.fillRoundedRectangle (cr, 3.0f);
                g.setColour (rt::cursor);
                g.drawRoundedRectangle (cr, 3.0f, 2.2f);
            }

            // Leere Felder zeigen ein zartes "..", gesetzte Werte stehen fett und
            // in kraeftiger Farbe da - so sieht man sofort, was eingetragen ist.
            const juce::Colour emptyCol = rt::textDim.withAlpha (0.45f);

            const bool noteEmpty = cell.note < 0;
            g.setFont (rt::mono (cellFontH, ! noteEmpty));
            g.setColour (noteEmpty ? emptyCol : rt::instColour (cell.instrument));
            g.drawText (noteName (cell.note), noteX, y, noteW, kRowH, juce::Justification::centredLeft);

            const bool instEmpty = cell.instrument < 0;
            g.setFont (rt::mono (cellFontH, ! instEmpty));
            g.setColour (instEmpty ? emptyCol : rt::instColour (cell.instrument));
            g.drawText (instEmpty ? juce::String ("..")
                                  : juce::String::formatted ("%02d", cell.instrument + 1),
                        instX, y, instW, kRowH, juce::Justification::centredLeft);

            // Lautstaerke- und Effekt-Spalte gibt es nur im Profi-Modus.
            if (! beginnerMode)
            {
                const bool volEmpty = cell.volume < 0;
                g.setFont (rt::mono (cellFontH, ! volEmpty));
                g.setColour (volEmpty ? emptyCol : rt::volCol.brighter (0.2f));
                g.drawText (volEmpty ? juce::String ("..")
                                     : juce::String::formatted ("%02d", cell.volume),
                            volX, y, volW, kRowH, juce::Justification::centredLeft);

                const bool fxEmpty = cell.effect < 0;
                g.setFont (rt::mono (cellFontH, ! fxEmpty));
                g.setColour (fxEmpty ? emptyCol : rt::fxCol);
                g.drawText (effectText (cell.effect, cell.effectParam),
                            fxX, y, fxW, kRowH, juce::Justification::centredLeft);
            }
        }
    }

    // Spaltenkopf
    g.setColour (rt::panel);
    g.fillRect (0, 0, w, kHeaderH);
    g.setColour (rt::steel.withAlpha (0.6f));
    g.drawHorizontalLine (kHeaderH, 0.0f, (float) w);

    g.setFont (rt::mono (14.0f, true));
    g.setColour (rt::text);
    g.drawText ("##", 0, 0, kLeftW - 6, kHeaderH, juce::Justification::centredRight);
    for (int vi = 0; vi < nVis; ++vi)
    {
        const int t = firstT + vi;
        if (t >= TrackerEngine::kTracks)
            break;
        const int tx = kLeftW + vi * trackW;
        const bool isCur = t == cursorTrack;

        // Spurname oben in den Kopf.
        g.setFont (rt::mono (14.0f, true));
        g.setColour (isCur ? rt::cursor : rt::text);
        g.drawText ("SPUR " + juce::String (t + 1), tx, 1, trackW, 18,
                    juce::Justification::centred);

        // M (Stumm) / S (Solo) Knoepfe darunter - per Mausklick umschaltbar.
        juce::Rectangle<int> muteR, soloR;
        trackHeaderButtons (tx, trackW, muteR, soloR);
        const bool muted  = engine.trackMute[t].load();
        const bool soloed = engine.trackSolo[t].load();

        auto drawBtn = [&g] (juce::Rectangle<int> r, const char* label, bool on, juce::Colour onCol)
        {
            const auto rf = r.toFloat();
            g.setColour (on ? onCol : rt::panel.brighter (0.18f));
            g.fillRoundedRectangle (rf, 3.0f);
            g.setColour (on ? rt::panel.darker (0.6f) : rt::textDim);
            g.drawRoundedRectangle (rf, 3.0f, 1.0f);
            g.setFont (rt::mono (12.0f, true));
            g.setColour (on ? juce::Colours::black : rt::textDim);
            g.drawText (label, r, juce::Justification::centred);
        };
        drawBtn (muteR, "M", muted,  juce::Colour (0xffe05a4a)); // Stumm: rot
        drawBtn (soloR, "S", soloed, juce::Colour (0xfff2c14e)); // Solo: gelb
    }

    // Spur-Trennlinien
    g.setColour (juce::Colour (0xff262c3a));
    for (int vi = 0; vi <= nVis; ++vi)
        g.drawVerticalLine (kLeftW + vi * trackW, 0.0f, (float) h);

    // Scroll-Pfeile im Kopf: zeigen an, dass links/rechts noch mehr Spuren liegen.
    g.setColour (rt::cursor);
    g.setFont (rt::mono (16.0f, true));
    if (firstT > 0)
        g.drawText ("<", kLeftW + 2, 0, 16, kHeaderH, juce::Justification::centredLeft);
    if (firstT + nVis < TrackerEngine::kTracks)
        g.drawText (">", w - 18, 0, 16, kHeaderH, juce::Justification::centredRight);

    // Fokus-Hinweis
    if (! hasKeyboardFocus (false))
    {
        g.setColour (rt::bg.withAlpha (0.55f));
        g.fillRect (0, kHeaderH, w, h - kHeaderH);
        g.setColour (rt::cursor);
        g.setFont (rt::mono (16.0f, true));
        g.drawText ("Klick ins Grid, dann lostippen!", 0, 0, w, h, juce::Justification::centred);
    }
}
