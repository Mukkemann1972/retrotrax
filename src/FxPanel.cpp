#include "FxPanel.h"

FxPanel::FxPanel (RetroTraxProcessor& processor)
    : proc (processor), samplerFx (processor), masterFx (processor)
{
    setWantsKeyboardFocus (true);

    // Reiter oben: schlichte Knoepfe, deren Zustand (AN) den aktiven Bereich zeigt.
    tabSampler.onClick = [this] { showSection (0); };
    tabMaster.onClick  = [this] { showSection (1); };
    addAndMakeVisible (tabSampler);
    addAndMakeVisible (tabMaster);

    closeButton.onClick = [this] { if (onClose) onClose(); };
    addAndMakeVisible (closeButton);

    // Die beiden bestehenden Panels als Unterbereiche einbetten. Ihre eigenen
    // SCHLIESSEN-Knoepfe schliessen das ganze FX-Overlay.
    addChildComponent (samplerFx);
    addChildComponent (masterFx);
    samplerFx.onClose = [this] { if (onClose) onClose(); };
    masterFx.onClose  = [this] { if (onClose) onClose(); };

    applyLanguage();
    showSection (0);
}

void FxPanel::applyLanguage()
{
    tabSampler.setButtonText (loc::t ("AKAI-SAMPLER-EFFEKTE", "AKAI SAMPLER FX"));
    tabMaster.setButtonText  (loc::t ("MASTER-EFFEKTE", "MASTER FX"));
    tabSampler.setTooltip (loc::t ("Effekte fuer das aktuelle Sample (Filter, 12-Bit, Vintage-Charaktere ...)",
                                   "Effects for the current sample (filter, 12-bit, vintage characters ...)"));
    tabMaster.setTooltip (loc::t ("Effekte fuer den ganzen Mix (Echo, Hall, EQ)",
                                  "Effects for the whole mix (echo, reverb, EQ)"));
    closeButton.setButtonText (loc::t ("SCHLIESSEN", "CLOSE"));
    samplerFx.applyLanguage();
    masterFx.applyLanguage();
}

void FxPanel::refresh()
{
    samplerFx.refresh();
    masterFx.refresh();
}

void FxPanel::showSection (int s)
{
    section = s;
    tabSampler.setToggleState (s == 0, juce::dontSendNotification);
    tabMaster.setToggleState  (s == 1, juce::dontSendNotification);
    samplerFx.setVisible (s == 0);
    masterFx.setVisible  (s == 1);
    if (s == 0) samplerFx.grabKeyboardFocus();
    else        masterFx.grabKeyboardFocus();
    resized();
}

bool FxPanel::keyPressed (const juce::KeyPress& key)
{
    if (key.getKeyCode() == juce::KeyPress::escapeKey && onClose != nullptr)
    {
        onClose();
        return true;
    }
    return false;
}

void FxPanel::paint (juce::Graphics& g)
{
    g.fillAll (rt::bg);
    g.setColour (rt::steel.withAlpha (0.7f));
    g.drawRect (getLocalBounds(), 1);
}

void FxPanel::resized()
{
    auto area = getLocalBounds();

    auto tabs = area.removeFromTop (40).reduced (12, 8);
    closeButton.setBounds (tabs.removeFromRight (130));
    tabs.removeFromRight (12);
    const int tw = (tabs.getWidth() - 10) / 2;
    tabSampler.setBounds (tabs.removeFromLeft (tw));
    tabs.removeFromLeft (10);
    tabMaster.setBounds (tabs.removeFromLeft (tw));

    // Beide bekommen denselben Bereich darunter; sichtbar ist immer nur einer.
    samplerFx.setBounds (area);
    masterFx.setBounds (area);
}
