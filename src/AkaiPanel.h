#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"
#include "RetroLookAndFeel.h"
#include "Localisation.h"

// Kleiner Editor fuer den Akai-Sampler-Filter eines Sample-Slots: ein resonanter
// Tiefpass im Stil der Akai S900/S950/S1000 (24 dB/Okt) plus optionaler 12-Bit-
// Crunch (lo-fi-Charakter). Liegt als Overlay ueber dem Grid, genau wie das SID-
// Panel. Aenderungen wirken sofort (live) auf das Sample im gewaehlten Slot.
class AkaiPanel : public juce::Component
{
public:
    explicit AkaiPanel (RetroTraxProcessor& processor);

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;

    void applyLanguage();
    void refresh(); // Regler aus dem aktuellen Slot fuellen (Slot wird hier gemerkt)

    std::function<void()> onClose;

private:
    void writeParams();   // Reglerwerte ins Instrument schreiben (live)
    void selectLoop (TrackerEngine::Instrument::Loop m); // Loop-Modus setzen
    void applyPreset (int index); // einen der Modell-Startklaenge legen
    void previewNote();   // C-5 mit Note-Aus -> Klang inkl. Filter hoerbar
    void updateButtons(); // Knopf-Zustaende (AN/AUS, 12-Bit) nachziehen

    RetroTraxProcessor& proc;
    int slot = 0; // Slot, den dieses Panel gerade bearbeitet

    juce::Label  titleLabel;
    juce::Label  slotLabel;

    // Werks-Startklaenge (Modelle): eine Reihe Knoepfe.
    juce::Label                        presetLabel;
    juce::OwnedArray<juce::TextButton> presetButtons;

    juce::TextButton onButton  { "FILTER AN" }; // Akai-Filter an/aus
    juce::TextButton bitButton { "12-BIT" };    // 12-Bit-Crunch an/aus
    juce::TextButton bit8Button { "8-BIT" };    // 8-Bit (Mirage/Fairlight)
    juce::TextButton compButton { "KOMPANDER" };// EMU-II-Kompander (mu-law)
    juce::TextButton revButton { "REVERSE" };   // Sample rueckwaerts
    juce::TextButton vintButton { "VINTAGE" };  // Vintage-Pitch (rohe Wandlung)

    // Loop-Modus: AUS / VORWAERTS / PING-PONG (gegenseitig ausschliessend).
    juce::Label      loopLabel;
    juce::TextButton loopOff  { "AUS" };
    juce::TextButton loopFwd  { "VORWAERTS" };
    juce::TextButton loopPing { "PING-PONG" };

    juce::Label  cutoffLabel, resoLabel, grainLabel, driveLabel, crossLabel;
    juce::Slider cutoffSlider, resoSlider, grainSlider, driveSlider, crossSlider; // grain = SR-Reduktion, cross = Loop-Crossfade

    // Sampler-Huellkurve (ADSR) + Lautstaerke - eigenen Sound formen.
    juce::TextButton envButton { "HUELLKURVE" };
    juce::Label  attLabel, decLabel, susLabel, relLabel, volLabel;
    juce::Slider attSlider, decSlider, susSlider, relSlider, volSlider;

    juce::Label  hintLabel;
    juce::TextButton testButton  { "TEST" };
    juce::TextButton closeButton { "SCHLIESSEN" };

    bool loading = false; // true, waehrend refresh() die Regler setzt (keine Callbacks)

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AkaiPanel)
};
