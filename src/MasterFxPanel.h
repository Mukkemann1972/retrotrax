#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"
#include "RetroLookAndFeel.h"
#include "Localisation.h"

// Master-FX-Sektion: Effekte, die auf den GANZEN Mix wirken (wie Renoises
// Master-Track). Hier wohnen Echo (Delay) und Hall (Reverb). Standard AUS
// (Mix 0) - der Klang bleibt unveraendert, bis man aufdreht. Overlay wie die
// anderen Panels; Werte werden im Song (.retrotrax) mitgespeichert.
class MasterFxPanel : public juce::Component
{
public:
    explicit MasterFxPanel (RetroTraxProcessor& processor);

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;

    void applyLanguage();
    void refresh(); // Regler aus den Prozessor-Werten fuellen

    std::function<void()> onClose;

private:
    void writeParams();

    RetroTraxProcessor& proc;

    juce::Label titleLabel;

    // Echo (Delay).
    juce::Label      echoHead, echoTimeLabel, echoFbLabel, echoMixLabel;
    juce::Slider     echoTimeSlider, echoFbSlider, echoMixSlider;

    // Hall (Reverb).
    juce::Label      revHead, revSizeLabel, revMixLabel;
    juce::Slider     revSizeSlider, revMixSlider;

    juce::TextButton closeButton { "SCHLIESSEN" };
    bool loading = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MasterFxPanel)
};
