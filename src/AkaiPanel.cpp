#include "AkaiPanel.h"

// --- Modell-Startklaenge ----------------------------------------------------
// Drei Klassiker plus ein sanfter Allrounder. Ein Preset setzt nur Filter-AN,
// Grenze, Resonanz und den 12-Bit-Crunch - das Sample selbst bleibt unangetastet.
namespace
{
    struct AkaiPreset
    {
        const char* nameDe; const char* nameEn;
        bool  on;  float cutoff, reso;  bool bit12;
    };

    const AkaiPreset kAkaiPresets[] =
    {
        // Name              an     cut    res    12-Bit
        { "S900",  "S900",  true,  0.45f, 0.30f, true  }, // grobkoerniger 12-Bit-Klassiker
        { "S950",  "S950",  true,  0.60f, 0.22f, true  }, // etwas klarer, noch 12-Bit
        { "S1000", "S1000", true,  0.80f, 0.12f, false }, // sanfter 16-Bit-Charakter
        { "WARM",  "WARM",  true,  0.40f, 0.10f, false }, // einfach weich gefiltert
    };
    constexpr int kNumAkaiPresets = (int) (sizeof (kAkaiPresets) / sizeof (kAkaiPresets[0]));
}

AkaiPanel::AkaiPanel (RetroTraxProcessor& processor) : proc (processor)
{
    setWantsKeyboardFocus (true);

    titleLabel.setFont (rt::mono (16.0f, true));
    titleLabel.setColour (juce::Label::textColourId, rt::cursor);
    addAndMakeVisible (titleLabel);

    slotLabel.setFont (rt::mono (13.0f, true));
    slotLabel.setColour (juce::Label::textColourId, rt::text);
    addAndMakeVisible (slotLabel);

    presetLabel.setFont (rt::mono (12.0f, true));
    presetLabel.setColour (juce::Label::textColourId, rt::textDim);
    addAndMakeVisible (presetLabel);
    for (int i = 0; i < kNumAkaiPresets; ++i)
    {
        auto* b = presetButtons.add (new juce::TextButton());
        b->onClick = [this, i] { applyPreset (i); };
        addAndMakeVisible (*b);
    }

    onButton.setClickingTogglesState (true);
    onButton.onClick = [this]
    {
        const bool on = onButton.getToggleState();
        proc.editSample (slot, [on] (TrackerEngine::Instrument& i) { i.akaiOn = on; });
        updateButtons();
        previewNote();
    };
    addAndMakeVisible (onButton);

    bitButton.setClickingTogglesState (true);
    bitButton.onClick = [this]
    {
        const bool on = bitButton.getToggleState();
        proc.editSample (slot, [on] (TrackerEngine::Instrument& i) { i.akai12bit = on; });
        previewNote();
    };
    addAndMakeVisible (bitButton);

    revButton.setClickingTogglesState (true);
    revButton.onClick = [this]
    {
        const bool on = revButton.getToggleState();
        proc.editSample (slot, [on] (TrackerEngine::Instrument& i) { i.reverse = on; });
        previewNote();
    };
    addAndMakeVisible (revButton);

    // Loop-Modus: drei sich ausschliessende Knoepfe.
    loopLabel.setFont (rt::mono (12.0f, true));
    loopLabel.setColour (juce::Label::textColourId, rt::textDim);
    addAndMakeVisible (loopLabel);
    using Loop = TrackerEngine::Instrument::Loop;
    loopOff.onClick  = [this] { selectLoop (Loop::Off); };
    loopFwd.onClick  = [this] { selectLoop (Loop::Forward); };
    loopPing.onClick = [this] { selectLoop (Loop::PingPong); };
    addAndMakeVisible (loopOff);
    addAndMakeVisible (loopFwd);
    addAndMakeVisible (loopPing);

    // Grenze (Cutoff) + Resonanz als gut ablesbare Balken-Regler.
    auto setupSlider = [this] (juce::Slider& s, juce::Label& lab)
    {
        s.setSliderStyle (juce::Slider::LinearBar);
        s.setRange (0.0, 100.0, 1.0);
        s.setTextValueSuffix (" %");
        s.setNumDecimalPlacesToDisplay (0);
        s.onValueChange = [this] { writeParams(); };
        s.onDragEnd     = [this] { previewNote(); };
        addAndMakeVisible (s);

        lab.setFont (rt::mono (12.0f, true));
        lab.setColour (juce::Label::textColourId, rt::textDim);
        addAndMakeVisible (lab);
    };
    setupSlider (cutoffSlider, cutoffLabel);
    setupSlider (resoSlider,   resoLabel);
    setupSlider (grainSlider,  grainLabel);

    hintLabel.setFont (rt::mono (12.0f, false));
    hintLabel.setColour (juce::Label::textColourId, rt::textDim);
    hintLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (hintLabel);

    testButton.onClick  = [this] { previewNote(); };
    addAndMakeVisible (testButton);

    closeButton.onClick = [this] { if (onClose) onClose(); };
    addAndMakeVisible (closeButton);

    applyLanguage();
}

void AkaiPanel::applyLanguage()
{
    titleLabel.setText (loc::t ("AKAI-SAMPLER", "AKAI SAMPLER"),
                        juce::dontSendNotification);
    presetLabel.setText (loc::t ("MODELL", "MODEL"), juce::dontSendNotification);
    for (int i = 0; i < presetButtons.size() && i < kNumAkaiPresets; ++i)
        presetButtons[i]->setButtonText (loc::t (kAkaiPresets[i].nameDe, kAkaiPresets[i].nameEn));

    onButton.setButtonText  (loc::t ("FILTER AN", "FILTER ON"));
    bitButton.setButtonText (loc::t ("12-BIT", "12-BIT"));
    bitButton.setTooltip (loc::t ("12-Bit-Crunch: der koernige lo-fi-Klang der alten Sampler",
                                  "12-bit crunch: the gritty lo-fi sound of the old samplers"));
    revButton.setButtonText (loc::t ("REVERSE", "REVERSE"));
    revButton.setTooltip (loc::t ("Sample rueckwaerts abspielen",
                                  "Play the sample backwards"));
    loopLabel.setText (loc::t ("LOOP", "LOOP"), juce::dontSendNotification);
    loopOff.setButtonText  (loc::t ("AUS", "OFF"));
    loopFwd.setButtonText  (loc::t ("VORWAERTS", "FORWARD"));
    loopPing.setButtonText (loc::t ("PING-PONG", "PING-PONG"));
    loopPing.setTooltip (loc::t ("Sample laeuft vor und zurueck in der Schleife (knackfrei)",
                                 "Sample runs forward and back in a loop (click-free)"));
    cutoffLabel.setText (loc::t ("GRENZE", "CUTOFF"), juce::dontSendNotification);
    resoLabel.setText   (loc::t ("RESONANZ", "RESONANCE"), juce::dontSendNotification);
    grainLabel.setText  (loc::t ("KOERNUNG", "GRAIN"), juce::dontSendNotification);
    grainSlider.setTooltip (loc::t ("Sample-Rate-Reduktion: rauer, koerniger lo-fi-Klang (Decimator)",
                                    "Sample-rate reduction: rough, gritty lo-fi sound (decimator)"));
    hintLabel.setText (loc::t ("Resonanter Tiefpass + 12-Bit + Reverse + Koernung (Decimator). Standard AUS - dein Sample bleibt unveraendert.",
                              "Resonant low-pass + 12-bit + reverse + grain (decimator). Off by default - your sample stays unchanged."),
                       juce::dontSendNotification);
    testButton.setButtonText  (loc::t ("TEST", "TEST"));
    closeButton.setButtonText (loc::t ("SCHLIESSEN", "CLOSE"));
}

void AkaiPanel::refresh()
{
    slot = proc.currentInstrument.load();

    TrackerEngine::Instrument s;
    const bool have = proc.getSample (slot, s);

    slotLabel.setText ("Slot " + juce::String (slot + 1)
                       + (have ? (" - " + s.name) : juce::String()),
                       juce::dontSendNotification);

    loading = true;
    cutoffSlider.setValue (have ? s.akaiCutoff    * 100.0 : 100.0, juce::dontSendNotification);
    resoSlider.setValue   (have ? s.akaiResonance * 100.0 :  12.0, juce::dontSendNotification);
    grainSlider.setValue  (have ? s.srReduction   * 100.0 :   0.0, juce::dontSendNotification);
    onButton.setToggleState  (have && s.akaiOn,    juce::dontSendNotification);
    bitButton.setToggleState (have && s.akai12bit, juce::dontSendNotification);
    revButton.setToggleState (have && s.reverse,   juce::dontSendNotification);
    loading = false;

    updateButtons();
}

void AkaiPanel::updateButtons()
{
    const bool on = onButton.getToggleState();
    // Regler nur wirksam, wenn der Filter an ist (12-Bit geht auch ohne Filter).
    cutoffSlider.setEnabled (on);
    resoSlider.setEnabled (on);

    // Loop-Knoepfe gegenseitig ausschliessend - aktiven hervorheben.
    using Loop = TrackerEngine::Instrument::Loop;
    TrackerEngine::Instrument s;
    const Loop m = proc.getSample (slot, s) ? s.loopMode : Loop::Off;
    loopOff.setToggleState  (m == Loop::Off,      juce::dontSendNotification);
    loopFwd.setToggleState  (m == Loop::Forward,  juce::dontSendNotification);
    loopPing.setToggleState (m == Loop::PingPong, juce::dontSendNotification);
}

void AkaiPanel::applyPreset (int index)
{
    if (index < 0 || index >= kNumAkaiPresets || ! proc.isSampleSlot (slot))
        return;
    const auto& p = kAkaiPresets[index];
    proc.editSample (slot, [&p] (TrackerEngine::Instrument& i)
    {
        i.akaiOn        = p.on;
        i.akaiCutoff    = p.cutoff;
        i.akaiResonance = p.reso;
        i.akai12bit     = p.bit12;
    });
    refresh();
    previewNote();
}

void AkaiPanel::writeParams()
{
    if (loading)
        return;
    const float cut   = (float) (cutoffSlider.getValue() / 100.0);
    const float res   = (float) (resoSlider.getValue()   / 100.0);
    const float grain = (float) (grainSlider.getValue()  / 100.0);
    proc.editSample (slot, [=] (TrackerEngine::Instrument& i)
    {
        i.akaiCutoff    = cut;
        i.akaiResonance = res;
        i.srReduction   = grain;
    });
}

void AkaiPanel::selectLoop (TrackerEngine::Instrument::Loop m)
{
    proc.editSample (slot, [m] (TrackerEngine::Instrument& i) { i.loopMode = m; });
    updateButtons();
    previewNote();
}

void AkaiPanel::previewNote()
{
    if (! proc.isSampleSlot (slot))
        return;
    double sr = proc.getSampleRate();
    if (sr <= 0.0)
        sr = 44100.0;
    proc.engine.audition (60, slot, (int) (1.2 * sr)); // C-5, kurz halten
}

bool AkaiPanel::keyPressed (const juce::KeyPress& key)
{
    if (key.getKeyCode() == juce::KeyPress::escapeKey && onClose != nullptr)
    {
        onClose();
        return true;
    }
    return false;
}

void AkaiPanel::paint (juce::Graphics& g)
{
    g.fillAll (rt::bg);
    g.setColour (rt::steel.withAlpha (0.7f));
    g.drawRect (getLocalBounds(), 1);
}

void AkaiPanel::resized()
{
    auto area = getLocalBounds().reduced (14);

    auto top = area.removeFromTop (26);
    titleLabel.setBounds (top.removeFromLeft (260));
    slotLabel.setBounds  (top);
    area.removeFromTop (12);

    // Modell-Startklaenge: eine Reihe gleich breiter Knoepfe.
    presetLabel.setBounds (area.removeFromTop (16));
    {
        auto row = area.removeFromTop (30);
        const int n = presetButtons.size();
        if (n > 0)
        {
            const int bw = (row.getWidth() - (n - 1) * 6) / n;
            for (auto* b : presetButtons)
            {
                b->setBounds (row.removeFromLeft (bw));
                row.removeFromLeft (6);
            }
        }
    }
    area.removeFromTop (16);

    // FILTER AN + 12-BIT + REVERSE nebeneinander.
    {
        auto row = area.removeFromTop (32);
        onButton.setBounds  (row.removeFromLeft (150));
        row.removeFromLeft (10);
        bitButton.setBounds (row.removeFromLeft (120));
        row.removeFromLeft (10);
        revButton.setBounds (row.removeFromLeft (130));
    }
    area.removeFromTop (14);

    // Grenze + Resonanz + Koernung mit Beschriftung links.
    auto sliderRow = [&area] (juce::Label& lab, juce::Slider& s)
    {
        auto row = area.removeFromTop (28);
        lab.setBounds (row.removeFromLeft (90));
        s.setBounds   (row);
    };
    sliderRow (cutoffLabel, cutoffSlider);
    area.removeFromTop (8);
    sliderRow (resoLabel, resoSlider);
    area.removeFromTop (8);
    sliderRow (grainLabel, grainSlider);
    area.removeFromTop (14);

    // Loop-Reihe: Label + AUS / VORWAERTS / PING-PONG.
    {
        auto row = area.removeFromTop (32);
        loopLabel.setBounds (row.removeFromLeft (60));
        loopOff.setBounds  (row.removeFromLeft (90));
        row.removeFromLeft (8);
        loopFwd.setBounds  (row.removeFromLeft (130));
        row.removeFromLeft (8);
        loopPing.setBounds (row.removeFromLeft (130));
    }
    area.removeFromTop (14);

    hintLabel.setBounds (area.removeFromTop (40));

    // Test + Schliessen unten.
    auto bottom = area.removeFromBottom (32);
    closeButton.setBounds (bottom.removeFromRight (140));
    bottom.removeFromRight (10);
    testButton.setBounds  (bottom.removeFromRight (110));
}
