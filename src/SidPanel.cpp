#include "SidPanel.h"

SidPanel::SidPanel (RetroTraxProcessor& processor) : proc (processor)
{
    setWantsKeyboardFocus (true);

    titleLabel.setFont (rt::mono (16.0f, true));
    titleLabel.setColour (juce::Label::textColourId, rt::cursor);
    addAndMakeVisible (titleLabel);

    slotLabel.setFont (rt::mono (13.0f, true));
    slotLabel.setColour (juce::Label::textColourId, rt::text);
    addAndMakeVisible (slotLabel);

    waveLabel.setFont (rt::mono (12.0f, true));
    waveLabel.setColour (juce::Label::textColourId, rt::textDim);
    addAndMakeVisible (waveLabel);

    for (auto* b : { &waveTri, &waveSaw, &wavePulse, &waveNoise })
        addAndMakeVisible (*b);
    waveTri.onClick   = [this] { selectWave (Wave::Triangle); };
    waveSaw.onClick   = [this] { selectWave (Wave::Saw); };
    wavePulse.onClick = [this] { selectWave (Wave::Pulse); };
    waveNoise.onClick = [this] { selectWave (Wave::Noise); };

    // Pulsweite + ADSR als gut ablesbare Balken-Regler.
    auto setupSlider = [this] (juce::Slider& s, juce::Label& lab, double lo, double hi,
                               double interval, const juce::String& suffix, int decimals)
    {
        s.setSliderStyle (juce::Slider::LinearBar);
        s.setRange (lo, hi, interval);
        s.setTextValueSuffix (suffix);
        s.setNumDecimalPlacesToDisplay (decimals);
        s.onValueChange = [this] { applyToProc(); };
        addAndMakeVisible (s);

        lab.setFont (rt::mono (12.0f, true));
        lab.setColour (juce::Label::textColourId, rt::textDim);
        addAndMakeVisible (lab);
    };
    setupSlider (pwSlider,      pwLabel,      5.0,  95.0, 1.0,  " %", 0);
    setupSlider (attackSlider,  attackLabel,  0.0,  1.50, 0.005, " s", 3);
    setupSlider (decaySlider,   decayLabel,   0.0,  2.00, 0.005, " s", 3);
    setupSlider (sustainSlider, sustainLabel, 0.0, 100.0, 1.0,  " %", 0);
    setupSlider (releaseSlider, releaseLabel, 0.0,  2.00, 0.005, " s", 3);

    hintLabel.setFont (rt::mono (12.0f));
    hintLabel.setColour (juce::Label::textColourId, rt::textDim);
    hintLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (hintLabel);

    closeButton.onClick = [this] { if (onClose) onClose(); };
    addAndMakeVisible (closeButton);

    applyLanguage();
}

void SidPanel::applyLanguage()
{
    titleLabel.setText (loc::t ("SID-SYNTHESIZER", "SID SYNTHESIZER"), juce::dontSendNotification);
    waveLabel.setText  (loc::t ("WELLENFORM", "WAVEFORM"), juce::dontSendNotification);
    waveTri.setButtonText   (loc::t ("DREIECK", "TRIANGLE"));
    waveSaw.setButtonText   (loc::t ("SAEGE", "SAW"));
    wavePulse.setButtonText (loc::t ("PULS", "PULSE"));
    waveNoise.setButtonText (loc::t ("RAUSCHEN", "NOISE"));

    pwLabel.setText      (loc::t ("PULSWEITE", "PULSE WIDTH"), juce::dontSendNotification);
    attackLabel.setText  (loc::t ("ANSTIEG (A)", "ATTACK (A)"), juce::dontSendNotification);
    decayLabel.setText   (loc::t ("ABFALL (D)", "DECAY (D)"), juce::dontSendNotification);
    sustainLabel.setText (loc::t ("HALTEN (S)", "SUSTAIN (S)"), juce::dontSendNotification);
    releaseLabel.setText (loc::t ("AUSKLANG (R)", "RELEASE (R)"), juce::dontSendNotification);

    hintLabel.setText (loc::t (
        "Tippe Noten ins Grid - dieser SID klingt sofort.  Taste 1 = Note aus (laesst ausklingen).",
        "Type notes into the grid - this SID sounds right away.  Key 1 = note off (lets it fade)."),
        juce::dontSendNotification);
    closeButton.setButtonText (loc::t ("SCHLIESSEN", "CLOSE"));
}

void SidPanel::refresh()
{
    slot = proc.currentInstrument.load();
    slotLabel.setText (loc::t ("Slot ", "Slot ") + juce::String::formatted ("%02d", slot + 1),
                       juce::dontSendNotification);

    TrackerEngine::Instrument s;
    if (! proc.getSid (slot, s))
        return; // Slot ist (noch) kein SID-Instrument

    loading = true; // Regler setzen, ohne dass die Callbacks zurueckschreiben
    pwSlider.setValue      (s.pulseWidth * 100.0, juce::dontSendNotification);
    attackSlider.setValue  (s.attack,  juce::dontSendNotification);
    decaySlider.setValue   (s.decay,   juce::dontSendNotification);
    sustainSlider.setValue (s.sustain * 100.0, juce::dontSendNotification);
    releaseSlider.setValue (s.release, juce::dontSendNotification);
    loading = false;

    updateWaveButtons();
}

void SidPanel::selectWave (Wave w)
{
    proc.editSid (slot, [w] (TrackerEngine::Instrument& i) { i.wave = w; });
    updateWaveButtons();
    if (onChanged) onChanged();
    proc.engine.audition (60, slot); // C-5 zum Vorhoeren
}

void SidPanel::updateWaveButtons()
{
    TrackerEngine::Instrument s;
    const Wave w = proc.getSid (slot, s) ? s.wave : Wave::Pulse;
    waveTri.setToggleState   (w == Wave::Triangle, juce::dontSendNotification);
    waveSaw.setToggleState   (w == Wave::Saw,      juce::dontSendNotification);
    wavePulse.setToggleState (w == Wave::Pulse,    juce::dontSendNotification);
    waveNoise.setToggleState (w == Wave::Noise,    juce::dontSendNotification);

    // Pulsweite ist nur bei der Puls-Welle wirksam.
    pwSlider.setEnabled (w == Wave::Pulse);
}

void SidPanel::applyToProc()
{
    if (loading)
        return;

    const float pw  = (float) (pwSlider.getValue()      / 100.0);
    const float a   = (float)  attackSlider.getValue();
    const float d   = (float)  decaySlider.getValue();
    const float sus = (float) (sustainSlider.getValue() / 100.0);
    const float rel = (float)  releaseSlider.getValue();

    proc.editSid (slot, [=] (TrackerEngine::Instrument& i)
    {
        i.pulseWidth = pw;
        i.attack     = a;
        i.decay      = d;
        i.sustain    = sus;
        i.release    = rel;
    });
    if (onChanged) onChanged();
    proc.engine.audition (60, slot);
}

bool SidPanel::keyPressed (const juce::KeyPress& key)
{
    if (key.getKeyCode() == juce::KeyPress::escapeKey && onClose != nullptr)
    {
        onClose();
        return true;
    }
    return false;
}

void SidPanel::paint (juce::Graphics& g)
{
    g.fillAll (rt::bg);
    g.setColour (rt::steel.withAlpha (0.7f));
    g.drawRect (getLocalBounds(), 1);
}

void SidPanel::resized()
{
    auto area = getLocalBounds().reduced (14);

    auto top = area.removeFromTop (26);
    titleLabel.setBounds (top.removeFromLeft (240));
    slotLabel.setBounds  (top.removeFromLeft (120));
    area.removeFromTop (10);

    // Wellenform-Reihe
    waveLabel.setBounds (area.removeFromTop (18));
    auto waveRow = area.removeFromTop (34);
    const int bw = juce::jmin (130, (waveRow.getWidth() - 24) / 4);
    for (auto* b : { &waveTri, &waveSaw, &wavePulse, &waveNoise })
    {
        b->setBounds (waveRow.removeFromLeft (bw));
        waveRow.removeFromLeft (8);
    }
    area.removeFromTop (16);

    // Ein Regler je Zeile: links Beschriftung, rechts der Balken.
    auto sliderRow = [&area] (juce::Label& lab, juce::Slider& s)
    {
        auto row = area.removeFromTop (30);
        lab.setBounds (row.removeFromLeft (150));
        row.removeFromLeft (6);
        s.setBounds (row.removeFromLeft (juce::jmin (360, row.getWidth())));
        area.removeFromTop (8);
    };
    sliderRow (pwLabel,      pwSlider);
    sliderRow (attackLabel,  attackSlider);
    sliderRow (decayLabel,   decaySlider);
    sliderRow (sustainLabel, sustainSlider);
    sliderRow (releaseLabel, releaseSlider);

    auto bottom = getLocalBounds().reduced (14).removeFromBottom (30);
    closeButton.setBounds (bottom.removeFromRight (120).reduced (0, 2));
    hintLabel.setBounds (bottom);
}
