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

bool PatternGrid::keyPressed (const juce::KeyPress& key)
{
    const auto code = key.getKeyCode();
    const auto c    = juce::CharacterFunctions::toLowerCase (key.getTextCharacter());

    if (code == juce::KeyPress::spaceKey)           { togglePlay(); return true; }
    if (code == juce::KeyPress::upKey)              { moveCursor (-1, 0); return true; }
    if (code == juce::KeyPress::downKey)            { moveCursor (1, 0);  return true; }
    if (code == juce::KeyPress::leftKey)            { moveCursor (0, -1); return true; }
    if (code == juce::KeyPress::rightKey)           { moveCursor (0, 1);  return true; }
    if (code == juce::KeyPress::pageUpKey)          { moveCursor (-16, 0); return true; }
    if (code == juce::KeyPress::pageDownKey)        { moveCursor (16, 0);  return true; }
    if (code == juce::KeyPress::homeKey)            { cursorRow = 0; repaint(); return true; }
    if (code == juce::KeyPress::endKey)             { cursorRow = TrackerEngine::kRows - 1; repaint(); return true; }

    if (code == juce::KeyPress::tabKey)
    {
        const int delta = key.getModifiers().isShiftDown() ? -1 : 1;
        cursorTrack = (cursorTrack + delta + TrackerEngine::kTracks) % TrackerEngine::kTracks;
        cursorCol = 0;
        repaint();
        return true;
    }

    if (code == juce::KeyPress::deleteKey || code == juce::KeyPress::backspaceKey)
    {
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
