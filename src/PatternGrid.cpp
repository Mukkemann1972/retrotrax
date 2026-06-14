#include "PatternGrid.h"
#include "PluginProcessor.h"
#include "RetroLookAndFeel.h"

PatternGrid::PatternGrid (RetroTraxProcessor& processor)
    : proc (processor), engine (processor.engine)
{
    setWantsKeyboardFocus (true);
    startTimerHz (25);
}

void PatternGrid::timerCallback()
{
    if (engine.playing.load())
        repaint();
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
        int flat = cursorTrack * 3 + cursorCol + colDelta;
        const int total = TrackerEngine::kTracks * 3;
        flat = (flat % total + total) % total;
        cursorTrack = flat / 3;
        cursorCol   = flat % 3;
    }
    repaint();
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
    if (note < 0)
        return "---";
    return juce::String (names[note % 12]) + juce::String (note / 12);
}

bool PatternGrid::handleNoteKey (juce::juce_wchar c)
{
    const int offset = noteOffsetForChar (c);
    if (offset < 0)
        return false;

    pushUndo();
    const int note = juce::jlimit (0, TrackerEngine::kMaxNote,
                                   proc.currentOctave.load() * 12 + offset);
    auto& cell = engine.cells[cursorRow][cursorTrack];
    cell.note = note;
    cell.instrument = proc.currentInstrument.load();

    engine.audition (note, cell.instrument);
    moveCursor (1, 0); // wie damals: nach der Eingabe eine Zeile weiter
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
    return true;
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
    repaint();
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

    // "Nudgen und hoeren": im Stopp die Note unterm Cursor gleich anspielen.
    if (! engine.playing.load())
    {
        const auto& c = engine.cells[cursorRow][cursorTrack];
        if (c.note >= 0)
            engine.audition (c.note, c.instrument);
    }
    repaint();
}

bool PatternGrid::keyPressed (const juce::KeyPress& key)
{
    const auto code = key.getKeyCode();
    const auto c    = juce::CharacterFunctions::toLowerCase (key.getTextCharacter());
    const auto mods = key.getModifiers();

    // Strg-Kombinationen zuerst, damit C/V/X/Z nicht als Noten landen.
    // Mit aktiver Auswahl arbeiten C/X/V auf dem ganzen Block, sonst spaltenweise.
    if (mods.isCommandDown())
    {
        if (c == 'z') { mods.isShiftDown() ? redo() : undo(); return true; }
        if (c == 'y') { redo(); return true; }
        if (c == 'c') { hasSelection ? copyBlock() : copyTrack(); return true; }
        if (c == 'x') { hasSelection ? cutBlock()  : cutTrack();  return true; }
        if (c == 'v') { hasBlockClip ? pasteBlock() : pasteTrack(); return true; }
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
        repaint();
        return true;
    }

    if (code == juce::KeyPress::deleteKey || code == juce::KeyPress::backspaceKey)
    {
        pushUndo();
        auto& cell = engine.cells[cursorRow][cursorTrack];
        if (cursorCol == 0)      cell = TrackerEngine::Cell();
        else if (cursorCol == 1) cell.instrument = -1;
        else                     cell.volume = -1;
        moveCursor (1, 0);
        return true;
    }

    if (c == '+') { proc.currentOctave = juce::jmin (8, proc.currentOctave.load() + 1); repaint(); return true; }
    if (c == '-') { proc.currentOctave = juce::jmax (1, proc.currentOctave.load() - 1); repaint(); return true; }

    if (cursorCol == 0)
    {
        if (handleNoteKey (c))
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

    const int trackW = (getWidth() - kLeftW) / TrackerEngine::kTracks;
    if (trackW <= 0)
        return;

    const bool playing = engine.playing.load();
    const int focusRow = playing ? engine.currentRow.load() : cursorRow;
    const int visRows  = juce::jmax (1, (getHeight() - kHeaderH) / kRowH);

    if (e.x > kLeftW)
    {
        const int t = juce::jmin (TrackerEngine::kTracks - 1, (e.x - kLeftW) / trackW);
        const int xin = (e.x - kLeftW) % trackW;
        cursorTrack = t;
        cursorCol = xin < trackW * 45 / 100 ? 0 : (xin < trackW * 72 / 100 ? 1 : 2);
    }
    if (e.y > kHeaderH)
    {
        const int line = (e.y - kHeaderH) / kRowH;
        const int row  = focusRow + (line - visRows / 2);
        if (row >= 0 && row < TrackerEngine::kRows)
            cursorRow = row;
    }
    repaint();
}

void PatternGrid::paint (juce::Graphics& g)
{
    const int w = getWidth();
    const int h = getHeight();
    g.fillAll (rt::bg);

    const int trackW = (w - kLeftW) / TrackerEngine::kTracks;
    const bool playing = engine.playing.load();
    const int focusRow = playing ? engine.currentRow.load() : cursorRow;
    const int visRows  = juce::jmax (1, (h - kHeaderH) / kRowH);
    const int centerLine = visRows / 2;
    const int centerY = kHeaderH + centerLine * kRowH;

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

        for (int t = 0; t < TrackerEngine::kTracks; ++t)
        {
            const auto& cell = engine.cells[row][t];
            const int tx = kLeftW + t * trackW;

            // Markierter Block: zarte Fuellung hinter den Zellen.
            if (hasSelection && ! playing
                && row >= selR0 && row <= selR1 && t >= selT0 && t <= selT1)
            {
                g.setColour (rt::cursor.withAlpha (0.16f));
                g.fillRect (tx, y, trackW, kRowH);
            }

            const int noteX = tx + 8;
            const int noteW = trackW * 38 / 100;
            const int instX = tx + trackW * 45 / 100;
            const int instW = trackW * 24 / 100;
            const int volX  = tx + trackW * 72 / 100;
            const int volW  = trackW * 24 / 100;

            // Cursor-Markierung (nur sichtbar, wenn die Cursor-Zeile gerade angezeigt wird)
            if (row == cursorRow && t == cursorTrack && ! playing)
            {
                g.setColour (rt::cursor);
                const int cx = cursorCol == 0 ? noteX - 3 : (cursorCol == 1 ? instX - 3 : volX - 3);
                const int cw = cursorCol == 0 ? noteW + 6 : (cursorCol == 1 ? instW + 6 : volW + 6);
                g.drawRoundedRectangle ((float) cx, (float) y + 1.0f, (float) cw, (float) kRowH - 2.0f, 3.0f, 1.6f);
            }

            const bool empty = cell.note < 0;
            // Jede Note leuchtet in der Farbe ihres Instruments
            g.setColour (empty ? rt::textDim.withAlpha (0.55f)
                               : rt::instColour (cell.instrument));
            g.drawText (noteName (cell.note), noteX, y, noteW, kRowH, juce::Justification::centredLeft);

            g.setColour (cell.instrument < 0 ? rt::textDim.withAlpha (0.55f)
                                             : rt::instColour (cell.instrument));
            g.drawText (cell.instrument < 0 ? juce::String ("00")
                                            : juce::String::formatted ("%02d", cell.instrument + 1),
                        instX, y, instW, kRowH, juce::Justification::centredLeft);

            g.setColour (cell.volume < 0 ? rt::textDim.withAlpha (0.55f) : rt::volCol);
            g.drawText (cell.volume < 0 ? juce::String ("00")
                                        : juce::String::formatted ("%02d", cell.volume),
                        volX, y, volW, kRowH, juce::Justification::centredLeft);
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
    for (int t = 0; t < TrackerEngine::kTracks; ++t)
    {
        const bool isCur = t == cursorTrack;
        g.setColour (isCur ? rt::cursor : rt::text);
        g.drawText ("SPUR " + juce::String (t + 1),
                    kLeftW + t * trackW, 0, trackW, kHeaderH, juce::Justification::centred);
    }

    // Spur-Trennlinien
    g.setColour (juce::Colour (0xff262c3a));
    for (int t = 0; t <= TrackerEngine::kTracks; ++t)
        g.drawVerticalLine (kLeftW + t * trackW, 0.0f, (float) h);

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
