#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"
#include "RetroLookAndFeel.h"
#include "Localisation.h"

// Kleiner Editor fuer ein SID-Synth-Instrument: Wellenform waehlen, Pulsweite
// und die ADSR-Huellkurve einstellen. Liegt als Overlay ueber dem Grid, genau
// wie der Hilfe- und der Sample-Browser. Aenderungen wirken sofort (live) auf
// das Instrument im aktuell gewaehlten Slot.
class SidPanel : public juce::Component
{
public:
    explicit SidPanel (RetroTraxProcessor& processor);

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;

    void applyLanguage();
    void refresh(); // Regler aus dem aktuellen Slot fuellen (Slot wird hier gemerkt)

    std::function<void()> onClose;
    std::function<void()> onChanged; // Editor zieht den Instrument-Namen nach

private:
    using Wave   = TrackerEngine::Instrument::Wave;
    using Filter = TrackerEngine::Instrument::Filter;
    using Engine = TrackerEngine::Instrument::Engine;

    void selectWave (Wave w);
    void selectFilter (Filter f);
    void selectEngine (Engine e); // Klangmotor umschalten (Klassisch / Echter Chip)
    void toggleRing();
    void toggleSync();
    void writeParams();  // Reglerwerte ins Instrument schreiben (live, ohne Ton)
    void previewNote();  // ein C-5 mit automatischem Note-Aus -> ganze Huellkurve hoerbar
    void updateWaveButtons();
    void updateFilterButtons();
    void updateModButtons();
    void updateEngineButtons();

    RetroTraxProcessor& proc;
    int slot = 0; // Slot, den dieses Panel gerade bearbeitet

    juce::Label  titleLabel;
    juce::Label  slotLabel;

    // Klangmotor-Umschalter (oben): selbstgebaut vs. echter reSIDfp-Chip.
    juce::Label      engineLabel;
    juce::TextButton engineClassic { "KLASSISCH" };
    juce::TextButton engineChip    { "ECHTER CHIP" };

    juce::Label  waveLabel;
    juce::TextButton waveTri   { "DREIECK" };
    juce::TextButton waveSaw   { "SAEGE" };
    juce::TextButton wavePulse { "PULS" };
    juce::TextButton waveNoise { "RAUSCHEN" };

    juce::Label  pwLabel,  attackLabel, decayLabel, sustainLabel, releaseLabel;
    juce::Slider pwSlider, attackSlider, decaySlider, sustainSlider, releaseSlider;

    juce::Label  pwmRateLabel, pwmDepthLabel;
    juce::Slider pwmRateSlider, pwmDepthSlider;

    juce::Label  filterLabel;
    juce::TextButton filtOff  { "AUS" };
    juce::TextButton filtLow  { "TIEFPASS" };
    juce::TextButton filtHigh { "HOCHPASS" };
    juce::TextButton filtBand { "BANDPASS" };
    juce::Label  cutoffLabel, resoLabel;
    juce::Slider cutoffSlider, resoSlider;

    juce::Label  modLabel, modTuneLabel;
    juce::TextButton ringButton { "RING-MOD" };
    juce::TextButton syncButton { "HARD-SYNC" };
    juce::Slider modTuneSlider;

    juce::Label      hintLabel;
    juce::TextButton testButton  { "TEST" }; // aktuellen Klang anspielen (mit Ausklang)
    juce::TextButton closeButton { "SCHLIESSEN" };

    bool loading = false; // true, waehrend refresh() die Regler setzt (keine Callbacks)

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SidPanel)
};
