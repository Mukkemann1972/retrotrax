#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"
#include "RetroLookAndFeel.h"
#include "Localisation.h"
#include "AkaiPanel.h"
#include "MasterFxPanel.h"

// EIN "FX"-Knopf, zwei klar getrennte Bereiche unter Reitern oben:
//   - AKAI-SAMPLER-EFFEKTE: wirken auf das aktuelle Sample (Filter, 12-Bit,
//     Koernung, Drive, Vintage-Charaktere ...). Das ist das alte AKAI-Panel.
//   - MASTER-EFFEKTE: wirken auf den ganzen Mix (Echo, Hall, EQ).
// So heisst nichts mehr irrefuehrend "AKAI", obwohl es nur Effekte sind. Die
// beiden bestehenden Panels werden 1:1 eingebettet - nichts geht verloren.
class FxPanel : public juce::Component
{
public:
    explicit FxPanel (RetroTraxProcessor& processor);

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;

    void applyLanguage();
    void refresh();           // beide Unterbereiche auffrischen
    void showSection (int s); // 0 = Sampler (Akai), 1 = Master

    std::function<void()> onClose;

private:
    RetroTraxProcessor& proc;

    juce::TextButton tabSampler { "SAMPLER" };
    juce::TextButton tabMaster  { "MASTER" };
    juce::TextButton closeButton { "SCHLIESSEN" }; // immer erreichbar in der Reiterleiste

    AkaiPanel     samplerFx;
    MasterFxPanel masterFx;

    int section = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FxPanel)
};
