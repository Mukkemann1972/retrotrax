#include "MasterFxPanel.h"

MasterFxPanel::MasterFxPanel (RetroTraxProcessor& processor) : proc (processor)
{
    setWantsKeyboardFocus (true);

    titleLabel.setFont (rt::mono (16.0f, true));
    titleLabel.setColour (juce::Label::textColourId, rt::text);
    addAndMakeVisible (titleLabel);

    auto setupHead = [this] (juce::Label& l)
    {
        l.setFont (rt::mono (13.0f, true));
        l.setColour (juce::Label::textColourId, rt::steelHi);
        addAndMakeVisible (l);
    };
    auto setupSlider = [this] (juce::Slider& s, juce::Label& lab, double lo, double hi, double step)
    {
        s.setSliderStyle (juce::Slider::LinearBar);
        s.setRange (lo, hi, step);
        s.onValueChange = [this] { writeParams(); };
        addAndMakeVisible (s);
        lab.setFont (rt::mono (12.0f, true));
        lab.setColour (juce::Label::textColourId, rt::textDim);
        addAndMakeVisible (lab);
    };

    setupHead (echoHead);
    setupSlider (echoTimeSlider, echoTimeLabel, 20.0, 1000.0, 1.0);
    setupSlider (echoFbSlider,   echoFbLabel,    0.0,   95.0, 1.0);
    setupSlider (echoMixSlider,  echoMixLabel,   0.0,  100.0, 1.0);
    echoTimeSlider.setTextValueSuffix (" ms");
    echoFbSlider.setTextValueSuffix (" %");
    echoMixSlider.setTextValueSuffix (" %");

    setupHead (revHead);
    setupSlider (revSizeSlider, revSizeLabel, 0.0, 100.0, 1.0);
    setupSlider (revMixSlider,  revMixLabel,  0.0, 100.0, 1.0);
    revSizeSlider.setTextValueSuffix (" %");
    revMixSlider.setTextValueSuffix (" %");

    closeButton.onClick = [this] { if (onClose) onClose(); };
    addAndMakeVisible (closeButton);

    refresh();
    applyLanguage();
}

void MasterFxPanel::applyLanguage()
{
    titleLabel.setText (loc::t ("MASTER-FX (wirkt auf alles)", "MASTER FX (affects everything)"),
                        juce::dontSendNotification);
    echoHead.setText (loc::t ("ECHO (Delay)", "ECHO (delay)"), juce::dontSendNotification);
    echoTimeLabel.setText (loc::t ("ZEIT", "TIME"), juce::dontSendNotification);
    echoFbLabel.setText (loc::t ("RUECKKOPPLUNG", "FEEDBACK"), juce::dontSendNotification);
    echoMixLabel.setText (loc::t ("MIX", "MIX"), juce::dontSendNotification);
    echoMixSlider.setTooltip (loc::t ("Echo-Anteil - 0 = aus", "Echo amount - 0 = off"));
    revHead.setText (loc::t ("HALL (Reverb)", "REVERB"), juce::dontSendNotification);
    revSizeLabel.setText (loc::t ("RAUM", "ROOM"), juce::dontSendNotification);
    revMixLabel.setText (loc::t ("MIX", "MIX"), juce::dontSendNotification);
    revMixSlider.setTooltip (loc::t ("Hall-Anteil - 0 = aus", "Reverb amount - 0 = off"));
    closeButton.setButtonText (loc::t ("SCHLIESSEN", "CLOSE"));
}

void MasterFxPanel::refresh()
{
    loading = true;
    echoTimeSlider.setValue ((double) proc.echoTimeMs.load(),        juce::dontSendNotification);
    echoFbSlider.setValue   ((double) proc.echoFeedback.load() * 100.0, juce::dontSendNotification);
    echoMixSlider.setValue  ((double) proc.echoMix.load() * 100.0,   juce::dontSendNotification);
    revSizeSlider.setValue  ((double) proc.reverbSize.load() * 100.0, juce::dontSendNotification);
    revMixSlider.setValue   ((double) proc.reverbMix.load() * 100.0, juce::dontSendNotification);
    loading = false;
}

void MasterFxPanel::writeParams()
{
    if (loading)
        return;
    proc.echoTimeMs   = (float) echoTimeSlider.getValue();
    proc.echoFeedback = (float) (echoFbSlider.getValue()  / 100.0);
    proc.echoMix      = (float) (echoMixSlider.getValue() / 100.0);
    proc.reverbSize   = (float) (revSizeSlider.getValue() / 100.0);
    proc.reverbMix    = (float) (revMixSlider.getValue()  / 100.0);
}

bool MasterFxPanel::keyPressed (const juce::KeyPress& key)
{
    if (key.getKeyCode() == juce::KeyPress::escapeKey && onClose != nullptr)
    {
        onClose();
        return true;
    }
    return false;
}

void MasterFxPanel::paint (juce::Graphics& g)
{
    g.fillAll (rt::bg);
    g.setColour (rt::steel.withAlpha (0.7f));
    g.drawRect (getLocalBounds(), 1);
}

void MasterFxPanel::resized()
{
    auto area = getLocalBounds().reduced (16);

    titleLabel.setBounds (area.removeFromTop (28));
    area.removeFromTop (10);

    auto sliderRow = [&area] (juce::Label& lab, juce::Slider& s)
    {
        auto row = area.removeFromTop (28);
        lab.setBounds (row.removeFromLeft (150));
        s.setBounds   (row);
        area.removeFromTop (8);
    };

    echoHead.setBounds (area.removeFromTop (22));
    area.removeFromTop (4);
    sliderRow (echoTimeLabel, echoTimeSlider);
    sliderRow (echoFbLabel,   echoFbSlider);
    sliderRow (echoMixLabel,  echoMixSlider);

    area.removeFromTop (12);
    revHead.setBounds (area.removeFromTop (22));
    area.removeFromTop (4);
    sliderRow (revSizeLabel, revSizeSlider);
    sliderRow (revMixLabel,  revMixSlider);

    auto bottom = getLocalBounds().reduced (16).removeFromBottom (32);
    closeButton.setBounds (bottom.removeFromRight (140));
}
