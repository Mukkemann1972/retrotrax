#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"
#include "RetroLookAndFeel.h"
#include "Localisation.h"

// Kleiner Editor fuer ein Sprachsynthese-Instrument: Text tippen, Charakter
// (SAM/NARRATOR) waehlen, Speed/Pitch/Stimmlage/Mundoeffnung regeln. Liegt als
// Overlay ueber dem Grid, genau wie das SID-Panel. Das Ergebnis ist ein ganz
// normales Sample im Slot - der Akai-Sampler-Filter (FX-Knopf) wirkt danach
// ganz normal mit (12-Bit/Drive/Loop/Filter ...).
class SpeechPanel : public juce::Component
{
public:
    explicit SpeechPanel (RetroTraxProcessor& processor);

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;

    void applyLanguage();
    void refresh(); // Regler aus dem aktuellen Slot fuellen (Slot wird hier gemerkt)

    std::function<void()> onClose;
    std::function<void()> onChanged; // Editor zieht den Instrument-Namen nach

private:
    void selectCharacter (int character); // 0 = SAM, 1 = NARRATOR
    void writeParams();  // Reglerwerte + Text ins Instrument schreiben und neu rendern
    void previewNote();  // C-5 anspielen - spielt das gerenderte Sample ab
    void updateCharButtons();

    RetroTraxProcessor& proc;
    int slot = 0; // Slot, den dieses Panel gerade bearbeitet

    juce::Label  titleLabel;
    juce::Label  slotLabel;

    juce::Label      charLabel;
    juce::TextButton charSam       { "SAM (C64)" };
    juce::TextButton charNarrator  { "NARRATOR (AMIGA)" };

    juce::Label       textLabel;
    juce::TextEditor  textBox;

    juce::Label  speedLabel, pitchLabel, throatLabel, mouthLabel;
    juce::Slider speedSlider, pitchSlider, throatSlider, mouthSlider;

    juce::Label      hintLabel;
    juce::TextButton speakButton { "SPRECHEN" }; // Text (neu) rendern
    juce::TextButton testButton  { "TEST" };     // gerendertes Sample anspielen
    juce::TextButton closeButton { "SCHLIESSEN" };

    bool loading = false; // true, waehrend refresh() die Regler setzt (keine Callbacks)

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SpeechPanel)
};
