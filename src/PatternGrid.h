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

    // Live-Hilfe: zeigt im Editor unten im Klartext, was an der Cursor-Stelle gilt.
    void setLiveHelp (bool on);
    bool liveHelpOn() const { return liveHelp; }
    std::function<void(const juce::String&)> onCursorInfo; // Editor zeigt den Text an

private:
    void timerCallback() override;
    void togglePlay();
    void moveCursor (int rowDelta, int colDelta);
    bool handleNoteKey (juce::juce_wchar c);
    bool handleDigitKey (juce::juce_wchar c);
    bool handleEffectKey (juce::juce_wchar c); // Hex-Eingabe in der Effekt-Spalte (cursorCol 3)

    static juce::String effectText (int effect, int param); // "C40" bzw. "..."

    // Live-Hilfe: Klartext zur aktuellen Cursor-Stelle + Effekt-Bedeutung.
    void emitCursorInfo();
    juce::String cursorHelpText() const;
    static juce::String effectHelp (int effect, int param);
    bool liveHelp = false;

    static int noteOffsetForChar (juce::juce_wchar c);
    static juce::String noteName (int note);

    // --- Rueckgaengig/Wiederholen + Zwischenablage (Spalten) ---------------
    struct Snapshot { TrackerEngine::Cell cells[TrackerEngine::kRows][TrackerEngine::kTracks]; };
    Snapshot takeSnapshot() const;
    void restore (const Snapshot& s);
    void pushUndo();        // aktuellen Stand sichern, bevor etwas veraendert wird
    void undo();
    void redo();
    void copyTrack();       // aktuelle Spur in die Zwischenablage
    void cutTrack();        // kopieren + Spur leeren
    void pasteTrack();      // Zwischenablage in die aktuelle Spur

    // --- Block-Bearbeitung: Bereich markieren, kopieren, verschieben --------
    void extendSelection (int rowDelta, int trackDelta); // Umschalt+Pfeil
    void clearSelection();
    void blockRect (int& r0, int& r1, int& t0, int& t1) const; // Auswahl, sonst Cursorzelle
    void copyBlock();
    void cutBlock();
    void pasteBlock();      // oben-links = Cursor
    void nudgeBlock (int rowDelta, int trackDelta);            // Alt+Pfeil: direkt verschieben

    std::vector<Snapshot> undoStack, redoStack;
    static constexpr int kMaxUndo = 64;

    TrackerEngine::Cell clipColumn[TrackerEngine::kRows];
    bool hasClip = false;

    // Rechteckige Auswahl im Grid (Anker = Start, Cursor = bewegtes Ende).
    bool hasSelection = false;
    int  selAnchorRow = 0;
    int  selAnchorTrack = 0;

    // Block-Zwischenablage [Zeile][Spur].
    std::vector<std::vector<TrackerEngine::Cell>> blockClip;
    bool hasBlockClip = false;

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
