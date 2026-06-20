#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"
#include "RetroLookAndFeel.h"
#include "Localisation.h"

// Drum-Kit im MPC60/SP-1200-Stil: ein 4x4-Pad-Feld mit 16 EIGENEN Samples,
// getrennt von den Spur-Slots. Pads anklicken oder per Tastatur trommeln (sie
// leuchten beim Anschlag auf). Ein Pad ist ein normales Instrument (Sample),
// laesst sich also frei zwischen Pad und Spur-Slot schieben (-> SLOT / SLOT ->).
// Liegt als Overlay ueber dem Grid, genau wie das AKAI-/SID-Panel.
class DrumKitPanel : public juce::Component,
                     private juce::Timer
{
public:
    explicit DrumKitPanel (RetroTraxProcessor& processor);
    ~DrumKitPanel() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    bool keyPressed (const juce::KeyPress&) override;

    void applyLanguage();
    void refresh(); // Pad-Namen aus dem Kit neu einlesen

    std::function<void()> onClose;

private:
    void timerCallback() override;       // Pad-Leuchten ausblenden
    void triggerPad (int pad, int velocity = 64); // Pad anschlagen (Velocity 0..64) + aufleuchten
    void setSelected (int pad);          // Pad waehlen + Charakter-Regler nachziehen
    void refreshPadControls();           // Regler aus dem gewaehlten Pad fuellen
    void writePadParams();               // Regler -> gewaehltes Pad (live)
    void applySP1200();                  // SP-1200-Charakter aufs Pad legen
    int  padAtIndex (int visCol, int visRow) const; // Sichtposition -> Pad-Index (MPC-Layout)
    int  padFromKey (const juce::KeyPress&) const;   // Tastatur -> Pad (-1 = keins)
    juce::Rectangle<int> padBounds (int pad) const;  // Bildschirm-Rechteck eines Pads
    void loadIntoSelected();             // Datei in das gewaehlte Pad laden
    void setHint (const juce::String& de, const juce::String& en);

    RetroTraxProcessor& proc;

    juce::Rectangle<int> gridRect;       // Bereich, in dem die 16 Pads liegen
    int  selected = 0;                   // gerade gewaehltes Pad (0..15)
    float padGlow[TrackerEngine::kPads] = {}; // Anschlag-Leuchten 1..0
    juce::String padNames[TrackerEngine::kPads];
    bool padFilled[TrackerEngine::kPads] = {};

    juce::Label titleLabel;

    // Charakter-Regler fuer das GEWAEHLTE Pad (SP-1200/Emu-Klang).
    juce::Label      selLabel;                      // "PAD n"
    juce::Label      tuneLabel, gritLabel;
    juce::Slider     tuneSlider, gritSlider;        // Stimmung (Halbtoene), Grit (SR-Reduktion)
    juce::TextButton bitButton { "12-BIT" };        // 12-Bit-Crunch
    juce::TextButton spButton  { "SP-1200" };       // ein Klick = klassischer Crunch
    bool loadingCtl = false;                        // true, waehrend Regler gesetzt werden

    juce::TextButton loadButton   { "LADEN" };       // Datei in gewaehltes Pad
    juce::TextButton clearButton  { "LEEREN" };      // gewaehltes Pad leeren
    juce::TextButton allSlotsButton { "KIT -> SLOTS" }; // alle 16 Pads in die Slots 1-16
    juce::TextButton toSlotButton { "-> SLOT" };     // Pad in aktuellen Spur-Slot
    juce::TextButton fromSlotBtn  { "SLOT ->" };     // aktuellen Spur-Slot in Pad
    juce::TextButton closeButton  { "SCHLIESSEN" };
    juce::Label hintLabel;

    std::unique_ptr<juce::FileChooser> chooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DrumKitPanel)
};
