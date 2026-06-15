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
    using Wave = TrackerEngine::Instrument::Wave;

    void selectWave (Wave w);
    void applyToProc(); // Reglerwerte ins Instrument schreiben + kurz anspielen
    void updateWaveButtons();

    RetroTraxProcessor& proc;
    int slot = 0; // Slot, den dieses Panel gerade bearbeitet

    juce::Label  titleLabel;
    juce::Label  slotLabel;
    juce::Label  waveLabel;
    juce::TextButton waveTri   { "DREIECK" };
    juce::TextButton waveSaw   { "SAEGE" };
    juce::TextButton wavePulse { "PULS" };
    juce::TextButton waveNoise { "RAUSCHEN" };

    juce::Label  pwLabel,  attackLabel, decayLabel, sustainLabel, releaseLabel;
    juce::Slider pwSlider, attackSlider, decaySlider, sustainSlider, releaseSlider;

    juce::Label      hintLabel;
    juce::TextButton closeButton { "SCHLIESSEN" };

    bool loading = false; // true, waehrend refresh() die Regler setzt (keine Callbacks)

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SidPanel)
};
