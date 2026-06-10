#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "TrackerEngine.h"

class RetroTraxProcessor;

// Das Herzstueck: das Zahlen-Grid wie damals. Cursor-Zeile bleibt in der Mitte,
// das Pattern scrollt darunter durch (klassisches ProTracker-Verhalten).
class PatternGrid : public juce::Component,
                    private juce::Timer
{
public:
    explicit PatternGrid (RetroTraxProcessor& processor);

    void paint (juce::Graphics&) override;
    bool keyPressed (const juce::KeyPress&) override;
    void mouseDown (const juce::MouseEvent&) override;

    std::function<void()> onTransportChange; // Editor aktualisiert dann seine Anzeige

private:
    void timerCallback() override;
    void togglePlay();
    void moveCursor (int rowDelta, int colDelta);
    bool handleNoteKey (juce::juce_wchar c);
    bool handleDigitKey (juce::juce_wchar c);

    static int noteOffsetForChar (juce::juce_wchar c);
    static juce::String noteName (int note);

    RetroTraxProcessor& proc;
    TrackerEngine& engine;

    int cursorRow = 0;
    int cursorTrack = 0;
    int cursorCol = 0; // 0 = Note, 1 = Instrument, 2 = Lautstaerke

    static constexpr int kHeaderH = 26;
    static constexpr int kRowH    = 20;
    static constexpr int kLeftW   = 44;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PatternGrid)
};
