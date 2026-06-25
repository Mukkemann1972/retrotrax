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
        float grain;  // Sample-Rate-Reduktion (Koernung)
        float drive;  // weiche Saettigung (analoge Waerme)
        bool  vintage;// rohe Wandlung (crunchy beim Pitchen)
        bool  comp;   // EMU-II-Kompander (mu-law, warm/druckvoll)
        float wow;    // Tape-Wow/Flutter (Mellotron-Band-Eiern)
    };

    // Beruehmte Vintage-Sampler als Ein-Klick-Charaktere. Jeder kombiniert nur die
    // vorhandenen Bausteine (Filter, 12-Bit, Koernung/Decimator, Drive, Vintage-
    // Pitch) zum typischen Klang-Fingerabdruck der Maschine - das Sample selbst
    // bleibt unangetastet.
    const AkaiPreset kAkaiPresets[] =
    {
        // Name                       an     cut    res    12-Bit grain  drive  vintage comp   wow
        { "S950",      "S950",       true,  0.60f, 0.22f, true,  0.10f, 0.10f, false, false, 0.00f }, // 12-Bit-Klassiker, klar
        { "S1000",     "S1000",      true,  0.85f, 0.10f, false, 0.00f, 0.00f, false, false, 0.00f }, // 16-Bit sauber/transparent
        { "SP-1200",   "SP-1200",    true,  0.70f, 0.10f, true,  0.40f, 0.30f, true,  false, 0.00f }, // dreckige HipHop-Drums
        { "EMU II",    "EMU II",     true,  0.50f, 0.20f, false, 0.05f, 0.35f, false, true,  0.00f }, // 12-Bit kompandiert, warm + analoge Filter
        { "MIRAGE",    "MIRAGE",     true,  0.60f, 0.15f, true,  0.55f, 0.20f, true,  false, 0.00f }, // 8-Bit rau, koernig
        { "FAIRLIGHT", "FAIRLIGHT",  true,  0.85f, 0.08f, true,  0.35f, 0.05f, true,  false, 0.00f }, // 8-Bit kristallin/metallisch
        { "MELLOTRON", "MELLOTRON",  true,  0.55f, 0.10f, false, 0.00f, 0.20f, false, false, 0.55f }, // Bandeiern + warme Saettigung, sanfter Tiefpass
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

    bit8Button.setClickingTogglesState (true);
    bit8Button.onClick = [this]
    {
        const bool on = bit8Button.getToggleState();
        proc.editSample (slot, [on] (TrackerEngine::Instrument& i) { i.akai8bit = on; });
        previewNote();
    };
    addAndMakeVisible (bit8Button);

    compButton.setClickingTogglesState (true);
    compButton.onClick = [this]
    {
        const bool on = compButton.getToggleState();
        proc.editSample (slot, [on] (TrackerEngine::Instrument& i) { i.companding = on; });
        previewNote();
    };
    addAndMakeVisible (compButton);

    revButton.setClickingTogglesState (true);
    revButton.onClick = [this]
    {
        const bool on = revButton.getToggleState();
        proc.editSample (slot, [on] (TrackerEngine::Instrument& i) { i.reverse = on; });
        previewNote();
    };
    addAndMakeVisible (revButton);

    vintButton.setClickingTogglesState (true);
    vintButton.onClick = [this]
    {
        const bool on = vintButton.getToggleState();
        proc.editSample (slot, [on] (TrackerEngine::Instrument& i) { i.vintagePitch = on; });
        previewNote();
    };
    addAndMakeVisible (vintButton);

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
    setupSlider (driveSlider,  driveLabel);
    setupSlider (crossSlider,  crossLabel);
    setupSlider (wowSlider,    wowLabel);

    // ADSR-Huellkurve + Lautstaerke.
    envButton.setClickingTogglesState (true);
    envButton.onClick = [this]
    {
        const bool on = envButton.getToggleState();
        proc.editSample (slot, [on] (TrackerEngine::Instrument& i) { i.ampEnv = on; });
        updateButtons();
        previewNote();
    };
    addAndMakeVisible (envButton);
    setupSlider (attSlider, attLabel);
    setupSlider (decSlider, decLabel);
    setupSlider (susSlider, susLabel);
    setupSlider (relSlider, relLabel);
    setupSlider (volSlider, volLabel);
    volSlider.setRange (0.0, 200.0, 1.0); // Lautstaerke 0..200 %

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
    presetLabel.setText (loc::t ("VINTAGE-CHARAKTER", "VINTAGE CHARACTER"), juce::dontSendNotification);
    for (int i = 0; i < presetButtons.size() && i < kNumAkaiPresets; ++i)
        presetButtons[i]->setButtonText (loc::t (kAkaiPresets[i].nameDe, kAkaiPresets[i].nameEn));

    onButton.setButtonText  (loc::t ("FILTER AN", "FILTER ON"));
    bitButton.setButtonText (loc::t ("12-BIT", "12-BIT"));
    bitButton.setTooltip (loc::t ("12-Bit-Crunch: der koernige lo-fi-Klang der alten Sampler",
                                  "12-bit crunch: the gritty lo-fi sound of the old samplers"));
    bit8Button.setButtonText (loc::t ("8-BIT", "8-BIT"));
    bit8Button.setTooltip (loc::t ("8-Bit: noch groeber als 12-Bit - roh, koernig, kristallin (Mirage/Fairlight)",
                                   "8-bit: even coarser than 12-bit - raw, gritty, crystalline (Mirage/Fairlight)"));
    compButton.setButtonText (loc::t ("KOMPANDER", "COMPANDER"));
    compButton.setTooltip (loc::t ("EMU-II-Kompander: leise Anteile feiner, laute saettigen weich - warm + druckvoll",
                                   "EMU II compander: quiet parts finer, loud parts saturate softly - warm + punchy"));
    revButton.setButtonText (loc::t ("REVERSE", "REVERSE"));
    revButton.setTooltip (loc::t ("Sample rueckwaerts abspielen",
                                  "Play the sample backwards"));
    loopLabel.setText (loc::t ("LOOP", "LOOP"), juce::dontSendNotification);
    loopOff.setButtonText  (loc::t ("AUS", "OFF"));
    loopFwd.setButtonText  (loc::t ("VORWAERTS", "FORWARD"));
    loopPing.setButtonText (loc::t ("PING-PONG", "PING-PONG"));
    loopPing.setTooltip (loc::t ("Sample laeuft vor und zurueck in der Schleife (knackfrei)",
                                 "Sample runs forward and back in a loop (click-free)"));
    crossLabel.setText (loc::t ("GLAETTEN", "SMOOTH"), juce::dontSendNotification);
    crossSlider.setTooltip (loc::t ("Loop-Crossfade: blendet das Schleifen-Ende in den Anfang - kurze Samples loopen smooth statt abgehackt (nur Vorwaerts-Loop)",
                                    "Loop crossfade: blends the loop end into the start - short samples loop smoothly instead of choppy (forward loop only)"));
    envButton.setButtonText (loc::t ("HUELLKURVE", "ENVELOPE"));
    envButton.setTooltip (loc::t ("Huellkurve (ADSR) fuers Sample an/aus - Klang formen wie an einem Sampler",
                                  "Amplitude envelope (ADSR) for the sample on/off - shape it like a sampler"));
    attLabel.setText (loc::t ("ATTACK", "ATTACK"), juce::dontSendNotification);
    decLabel.setText (loc::t ("DECAY", "DECAY"), juce::dontSendNotification);
    susLabel.setText (loc::t ("SUSTAIN", "SUSTAIN"), juce::dontSendNotification);
    relLabel.setText (loc::t ("RELEASE", "RELEASE"), juce::dontSendNotification);
    volLabel.setText (loc::t ("LAUTSTAERKE", "VOLUME"), juce::dontSendNotification);
    vintButton.setButtonText (loc::t ("VINTAGE", "VINTAGE"));
    vintButton.setTooltip (loc::t ("Vintage-Pitch: rohe Wandlung ohne Glaettung - crunchy beim Pitchen (Fairlight/Emulator)",
                                   "Vintage pitch: raw conversion without smoothing - crunchy when pitched (Fairlight/Emulator)"));
    driveLabel.setText (loc::t ("DRIVE", "DRIVE"), juce::dontSendNotification);
    driveSlider.setTooltip (loc::t ("Weiche Saettigung wie die analogen Sampler-Filter - Waerme + Druck/Punch",
                                    "Soft saturation like the analog sampler filters - warmth + punch"));
    cutoffLabel.setText (loc::t ("GRENZE", "CUTOFF"), juce::dontSendNotification);
    resoLabel.setText   (loc::t ("RESONANZ", "RESONANCE"), juce::dontSendNotification);
    grainLabel.setText  (loc::t ("KOERNUNG", "GRAIN"), juce::dontSendNotification);
    grainSlider.setTooltip (loc::t ("Sample-Rate-Reduktion: rauer, koerniger lo-fi-Klang (Decimator)",
                                    "Sample-rate reduction: rough, gritty lo-fi sound (decimator)"));
    wowLabel.setText  (loc::t ("BAND-EIERN", "TAPE WOW"), juce::dontSendNotification);
    wowSlider.setTooltip (loc::t ("Tape-Wow/Flutter wie beim Mellotron: langsames Band-Eiern + leichtes Flattern - die Tonhoehe schwankt sanft (jede Note eiert eigen)",
                                  "Tape wow/flutter like a Mellotron: slow pitch drift + gentle flutter - the pitch wavers softly (each note wobbles on its own)"));
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
    driveSlider.setValue  (have ? s.drive         * 100.0 :   0.0, juce::dontSendNotification);
    crossSlider.setValue  (have ? s.loopXfade     * 100.0 :   0.0, juce::dontSendNotification);
    wowSlider.setValue    (have ? s.tapeWow       * 100.0 :   0.0, juce::dontSendNotification);
    attSlider.setValue (have ? juce::jlimit (0.0, 100.0, s.attack  / 2.0 * 100.0) :   2.0, juce::dontSendNotification);
    decSlider.setValue (have ? juce::jlimit (0.0, 100.0, s.decay   / 2.0 * 100.0) :   9.0, juce::dontSendNotification);
    susSlider.setValue (have ? juce::jlimit (0.0, 100.0, s.sustain * 100.0)        :  65.0, juce::dontSendNotification);
    relSlider.setValue (have ? juce::jlimit (0.0, 100.0, s.release / 3.0 * 100.0)  :   8.0, juce::dontSendNotification);
    volSlider.setValue (have ? juce::jlimit (0.0, 200.0, s.gain    * 100.0)        : 100.0, juce::dontSendNotification);
    onButton.setToggleState  (have && s.akaiOn,       juce::dontSendNotification);
    bitButton.setToggleState (have && s.akai12bit,    juce::dontSendNotification);
    bit8Button.setToggleState(have && s.akai8bit,     juce::dontSendNotification);
    compButton.setToggleState(have && s.companding,   juce::dontSendNotification);
    revButton.setToggleState (have && s.reverse,      juce::dontSendNotification);
    vintButton.setToggleState(have && s.vintagePitch, juce::dontSendNotification);
    envButton.setToggleState (have && s.ampEnv,       juce::dontSendNotification);
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

    // Loop-Crossfade wirkt nur beim Vorwaerts-Loop - sonst ausgrauen.
    crossSlider.setEnabled (m == Loop::Forward);

    // ADSR-Regler nur aktiv, wenn die Huellkurve an ist; Lautstaerke immer.
    const bool env = envButton.getToggleState();
    attSlider.setEnabled (env);
    decSlider.setEnabled (env);
    susSlider.setEnabled (env);
    relSlider.setEnabled (env);
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
        i.srReduction   = p.grain;
        i.drive         = p.drive;
        i.vintagePitch  = p.vintage;
        i.companding    = p.comp;
        i.tapeWow       = p.wow;
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
    const float drv   = (float) (driveSlider.getValue()  / 100.0);
    const float cross = (float) (crossSlider.getValue()  / 100.0);
    const float wow   = (float) (wowSlider.getValue()    / 100.0);
    const float att = (float) (attSlider.getValue() / 100.0 * 2.0);
    const float dec = (float) (decSlider.getValue() / 100.0 * 2.0);
    const float sus = (float) (susSlider.getValue() / 100.0);
    const float rel = (float) (relSlider.getValue() / 100.0 * 3.0);
    const float vol = (float) (volSlider.getValue() / 100.0);
    proc.editSample (slot, [=] (TrackerEngine::Instrument& i)
    {
        i.attack = att; i.decay = dec; i.sustain = sus; i.release = rel; i.gain = vol;
        i.akaiCutoff    = cut;
        i.akaiResonance = res;
        i.srReduction   = grain;
        i.drive         = drv;
        i.loopXfade     = cross;
        i.tapeWow       = wow;
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

    // Reihe 1: FILTER AN + 12-BIT + 8-BIT + KOMPANDER (Wandler-Charakter).
    {
        auto row = area.removeFromTop (32);
        onButton.setBounds  (row.removeFromLeft (150));
        row.removeFromLeft (10);
        bitButton.setBounds (row.removeFromLeft (110));
        row.removeFromLeft (10);
        bit8Button.setBounds (row.removeFromLeft (110));
        row.removeFromLeft (10);
        compButton.setBounds (row.removeFromLeft (140));
    }
    area.removeFromTop (8);
    // Reihe 2: REVERSE + VINTAGE (Abspiel-Charakter).
    {
        auto row = area.removeFromTop (32);
        revButton.setBounds (row.removeFromLeft (120));
        row.removeFromLeft (10);
        vintButton.setBounds (row.removeFromLeft (120));
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
    area.removeFromTop (8);
    sliderRow (driveLabel, driveSlider);
    area.removeFromTop (8);
    sliderRow (wowLabel, wowSlider);
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
    area.removeFromTop (8);
    sliderRow (crossLabel, crossSlider); // Loop-Crossfade (GLAETTEN)
    area.removeFromTop (14);

    // Huellkurve (ADSR) + Lautstaerke.
    { auto row = area.removeFromTop (30); envButton.setBounds (row.removeFromLeft (150)); }
    area.removeFromTop (8);
    sliderRow (attLabel, attSlider);
    area.removeFromTop (8);
    sliderRow (decLabel, decSlider);
    area.removeFromTop (8);
    sliderRow (susLabel, susSlider);
    area.removeFromTop (8);
    sliderRow (relLabel, relSlider);
    area.removeFromTop (8);
    sliderRow (volLabel, volSlider);
    area.removeFromTop (14);

    hintLabel.setBounds (area.removeFromTop (40));

    // Test + Schliessen unten.
    auto bottom = area.removeFromBottom (32);
    closeButton.setBounds (bottom.removeFromRight (140));
    bottom.removeFromRight (10);
    testButton.setBounds  (bottom.removeFromRight (110));
}
