#include "SidPanel.h"

// --- Werks-Presets ----------------------------------------------------------
// Ein Preset ist nur ein Satz SID-Reglerwerte mit Namen - eine gute Auswahl an
// Startklaengen zum Anklicken. Der Klangmotor (Klassisch/Echter Chip) bleibt
// unangetastet, damit jeder Preset mit dem gewaehlten Motor klingt.
namespace
{
    using Wave   = TrackerEngine::Instrument::Wave;
    using Filter = TrackerEngine::Instrument::Filter;

    struct SidPreset
    {
        const char* nameDe;  const char* nameEn;
        Wave  wave;   float pw;                    // Wellenform + Pulsweite
        float a, d, s, r;                          // Huellkurve (S als Pegel 0..1)
        Filter filter; float cutoff, reso;         // Filter
        bool  ring, sync; float modTune;           // 2. Oszillator
        float pwmRate, pwmDepth;                    // Pulsweiten-LFO
    };

    const SidPreset kSidPresets[] =
    {
        // Name           Welle           pw     A      D     S     R     Filter            cut   res   ring   sync  mTun  pwmR pwmD
        { "BASS","BASS",   Wave::Pulse,    0.50f, 0.002f,0.12f,0.55f,0.12f, Filter::LowPass,  0.32f,0.25f, false, false, 12,  0.0f,0.0f },
        { "LEAD","LEAD",   Wave::Saw,      0.50f, 0.004f,0.15f,0.80f,0.20f, Filter::LowPass,  0.72f,0.15f, false, false, 12,  0.0f,0.0f },
        { "GLOCKE","BELL", Wave::Triangle, 0.50f, 0.001f,0.50f,0.15f,0.60f, Filter::Off,      0.70f,0.12f, true,  false,  7,  0.0f,0.0f },
        { "DRUMS","DRUMS", Wave::Noise,    0.50f, 0.000f,0.10f,0.00f,0.08f, Filter::BandPass, 0.55f,0.30f, false, false, 12,  0.0f,0.0f },
        { "PAD","PAD",     Wave::Pulse,    0.50f, 0.250f,0.40f,0.80f,0.50f, Filter::LowPass,  0.50f,0.10f, false, false, 12,  0.6f,0.50f },
        { "SYNC-LEAD","SYNC LEAD", Wave::Saw, 0.50f, 0.004f,0.20f,0.85f,0.20f, Filter::Off,   0.70f,0.12f, false, true,  19,  0.0f,0.0f },
        { "BLIP","BLIP",   Wave::Pulse,    0.50f, 0.000f,0.06f,0.00f,0.05f, Filter::Off,      0.70f,0.12f, false, false, 12,  0.0f,0.0f },
    };
    constexpr int kNumPresets = (int) (sizeof (kSidPresets) / sizeof (kSidPresets[0]));
}

SidPanel::SidPanel (RetroTraxProcessor& processor) : proc (processor)
{
    setWantsKeyboardFocus (true);

    titleLabel.setFont (rt::mono (16.0f, true));
    titleLabel.setColour (juce::Label::textColourId, rt::cursor);
    addAndMakeVisible (titleLabel);

    slotLabel.setFont (rt::mono (13.0f, true));
    slotLabel.setColour (juce::Label::textColourId, rt::text);
    addAndMakeVisible (slotLabel);

    engineLabel.setFont (rt::mono (12.0f, true));
    engineLabel.setColour (juce::Label::textColourId, rt::textDim);
    addAndMakeVisible (engineLabel);
    addAndMakeVisible (engineClassic);
    addAndMakeVisible (engineChip);
    engineClassic.onClick = [this] { selectEngine (Engine::Classic); };
    engineChip.onClick    = [this] { selectEngine (Engine::RealChip); };

    // Werks-Presets: eine Reihe Startklaenge. Knoepfe aus der Tabelle erzeugen.
    presetLabel.setFont (rt::mono (12.0f, true));
    presetLabel.setColour (juce::Label::textColourId, rt::textDim);
    addAndMakeVisible (presetLabel);
    for (int i = 0; i < kNumPresets; ++i)
    {
        auto* b = presetButtons.add (new juce::TextButton());
        b->onClick = [this, i] { applyPreset (i); };
        addAndMakeVisible (*b);
    }

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
        // Werte wirken live (ohne Ton), das fertige Ergebnis wird beim Loslassen
        // einmal angespielt - so hoert man die Huellkurve sauber statt zu stottern.
        s.onValueChange = [this] { writeParams(); };
        s.onDragEnd     = [this] { previewNote(); };
        addAndMakeVisible (s);

        lab.setFont (rt::mono (12.0f, true));
        lab.setColour (juce::Label::textColourId, rt::textDim);
        addAndMakeVisible (lab);
    };
    setupSlider (pwSlider,      pwLabel,      5.0,  95.0, 1.0,  " %", 0);
    setupSlider (cutoffSlider,  cutoffLabel,  0.0, 100.0, 1.0,  " %", 0);
    setupSlider (resoSlider,    resoLabel,    0.0, 100.0, 1.0,  " %", 0);
    setupSlider (modTuneSlider, modTuneLabel, 0.0,  36.0, 1.0,  " HT", 0);
    setupSlider (pwmRateSlider, pwmRateLabel, 0.0,  12.0, 0.1,  " Hz", 1);
    setupSlider (pwmDepthSlider,pwmDepthLabel,0.0, 100.0, 1.0,  " %", 0);
    setupSlider (attackSlider,  attackLabel,  0.0,  1.50, 0.005, " s", 3);
    setupSlider (decaySlider,   decayLabel,   0.0,  2.00, 0.005, " s", 3);
    setupSlider (sustainSlider, sustainLabel, 0.0, 100.0, 1.0,  " %", 0);
    setupSlider (releaseSlider, releaseLabel, 0.0,  2.00, 0.005, " s", 3);

    filterLabel.setFont (rt::mono (12.0f, true));
    filterLabel.setColour (juce::Label::textColourId, rt::textDim);
    addAndMakeVisible (filterLabel);
    for (auto* b : { &filtOff, &filtLow, &filtHigh, &filtBand })
        addAndMakeVisible (*b);
    filtOff.onClick  = [this] { selectFilter (Filter::Off); };
    filtLow.onClick  = [this] { selectFilter (Filter::LowPass); };
    filtHigh.onClick = [this] { selectFilter (Filter::HighPass); };
    filtBand.onClick = [this] { selectFilter (Filter::BandPass); };

    modLabel.setFont (rt::mono (12.0f, true));
    modLabel.setColour (juce::Label::textColourId, rt::textDim);
    addAndMakeVisible (modLabel);
    addAndMakeVisible (ringButton);
    addAndMakeVisible (syncButton);
    ringButton.onClick = [this] { toggleRing(); };
    syncButton.onClick = [this] { toggleSync(); };

    hintLabel.setFont (rt::mono (12.0f));
    hintLabel.setColour (juce::Label::textColourId, rt::textDim);
    hintLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (hintLabel);

    testButton.onClick = [this] { previewNote(); };
    addAndMakeVisible (testButton);

    closeButton.onClick = [this] { if (onClose) onClose(); };
    addAndMakeVisible (closeButton);

    applyLanguage();
}

void SidPanel::applyLanguage()
{
    titleLabel.setText (loc::t ("SID-SYNTHESIZER", "SID SYNTHESIZER"), juce::dontSendNotification);
    engineLabel.setText (loc::t ("KLANG-MOTOR", "SOUND ENGINE"), juce::dontSendNotification);
    engineClassic.setButtonText (loc::t ("KLASSISCH", "CLASSIC"));
    engineChip.setButtonText    (loc::t ("ECHTER CHIP", "REAL CHIP"));
    engineClassic.setTooltip (loc::t ("Selbstgebauter Synth - der vertraute RetroTrax-Klang",
                                      "Self-built synth - the familiar RetroTrax sound"));
    engineChip.setTooltip (loc::t ("Echte reSIDfp-Emulation des MOS-6581-Chips (originaler C64-Sound)",
                                   "Real reSIDfp emulation of the MOS 6581 chip (authentic C64 sound)"));
    presetLabel.setText (loc::t ("WERKS-PRESETS", "FACTORY PRESETS"), juce::dontSendNotification);
    for (int i = 0; i < presetButtons.size() && i < kNumPresets; ++i)
    {
        presetButtons[i]->setButtonText (loc::t (kSidPresets[i].nameDe, kSidPresets[i].nameEn));
        presetButtons[i]->setTooltip (loc::t ("Fertigen Startklang laden - danach frei weiterregeln",
                                              "Load a ready-made starting sound - then tweak freely"));
    }

    waveLabel.setText  (loc::t ("WELLENFORM", "WAVEFORM"), juce::dontSendNotification);
    waveTri.setButtonText   (loc::t ("DREIECK", "TRIANGLE"));
    waveSaw.setButtonText   (loc::t ("SAEGE", "SAW"));
    wavePulse.setButtonText (loc::t ("PULS", "PULSE"));
    waveNoise.setButtonText (loc::t ("RAUSCHEN", "NOISE"));

    filterLabel.setText (loc::t ("FILTER", "FILTER"), juce::dontSendNotification);
    filtOff.setButtonText  (loc::t ("AUS", "OFF"));
    filtLow.setButtonText  (loc::t ("TIEFPASS", "LOW-PASS"));
    filtHigh.setButtonText (loc::t ("HOCHPASS", "HIGH-PASS"));
    filtBand.setButtonText (loc::t ("BANDPASS", "BAND-PASS"));
    cutoffLabel.setText  (loc::t ("GRENZE (CUTOFF)", "CUTOFF"), juce::dontSendNotification);
    resoLabel.setText    (loc::t ("RESONANZ", "RESONANCE"), juce::dontSendNotification);

    modLabel.setText (loc::t ("MODULATION (2. OSZILLATOR)", "MODULATION (2ND OSC)"), juce::dontSendNotification);
    ringButton.setButtonText (loc::t ("RING-MOD", "RING MOD"));
    syncButton.setButtonText (loc::t ("HARD-SYNC", "HARD SYNC"));
    ringButton.setTooltip (loc::t ("Ring-Modulation: metallischer, glockiger Klang",
                                   "Ring modulation: metallic, bell-like tone"));
    syncButton.setTooltip (loc::t ("Hard-Sync: zerreissende, schreiende Leads",
                                   "Hard sync: tearing, screaming leads"));
    modTuneLabel.setText (loc::t ("MOD-TONHOEHE", "MOD PITCH"), juce::dontSendNotification);

    pwLabel.setText      (loc::t ("PULSWEITE", "PULSE WIDTH"), juce::dontSendNotification);
    pwmRateLabel.setText (loc::t ("PWM-TEMPO", "PWM RATE"), juce::dontSendNotification);
    pwmDepthLabel.setText(loc::t ("PWM-TIEFE", "PWM DEPTH"), juce::dontSendNotification);
    attackLabel.setText  (loc::t ("ANSTIEG (A)", "ATTACK (A)"), juce::dontSendNotification);
    decayLabel.setText   (loc::t ("ABFALL (D)", "DECAY (D)"), juce::dontSendNotification);
    sustainLabel.setText (loc::t ("HALTEN (S)", "SUSTAIN (S)"), juce::dontSendNotification);
    releaseLabel.setText (loc::t ("AUSKLANG (R)", "RELEASE (R)"), juce::dontSendNotification);

    hintLabel.setText (loc::t (
        "Regler loslassen oder TEST = anspielen (mit Ausklang).  Im Grid: Taste 1 = Note aus.",
        "Release a slider or press TEST to play (with fade-out).  In the grid: key 1 = note off."),
        juce::dontSendNotification);
    testButton.setButtonText (loc::t ("TEST", "TEST"));
    testButton.setTooltip (loc::t ("Aktuellen SID-Klang anspielen - anschlagen, halten, loslassen",
                                   "Play the current SID sound - attack, hold, release"));
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
    pwmRateSlider.setValue (s.pwmRate, juce::dontSendNotification);
    pwmDepthSlider.setValue(s.pwmDepth * 100.0, juce::dontSendNotification);
    cutoffSlider.setValue  (s.cutoff * 100.0,  juce::dontSendNotification);
    resoSlider.setValue    (s.resonance * 100.0, juce::dontSendNotification);
    modTuneSlider.setValue (s.modTune, juce::dontSendNotification);
    attackSlider.setValue  (s.attack,  juce::dontSendNotification);
    decaySlider.setValue   (s.decay,   juce::dontSendNotification);
    sustainSlider.setValue (s.sustain * 100.0, juce::dontSendNotification);
    releaseSlider.setValue (s.release, juce::dontSendNotification);
    loading = false;

    updateWaveButtons();
    updateFilterButtons();
    updateModButtons();
    updateEngineButtons();
}

void SidPanel::applyPreset (int index)
{
    if (index < 0 || index >= kNumPresets || ! proc.isSid (slot))
        return;

    const auto& p = kSidPresets[index];
    proc.editSid (slot, [&p] (TrackerEngine::Instrument& i)
    {
        // Klangmotor (i.engine) und Name bleiben, wie sie sind - nur der Klang wechselt.
        i.wave       = p.wave;   i.pulseWidth = p.pw;
        i.attack     = p.a;      i.decay      = p.d;
        i.sustain    = p.s;      i.release    = p.r;
        i.filter     = p.filter; i.cutoff     = p.cutoff; i.resonance = p.reso;
        i.ringMod    = p.ring;   i.sync       = p.sync;   i.modTune   = p.modTune;
        i.pwmRate    = p.pwmRate; i.pwmDepth  = p.pwmDepth;
    });

    refresh();              // alle Regler und Knoepfe auf die Preset-Werte ziehen
    if (onChanged) onChanged();
    previewNote();          // Preset gleich anspielen
}

void SidPanel::selectEngine (Engine e)
{
    proc.editSid (slot, [e] (TrackerEngine::Instrument& i) { i.engine = e; });
    updateEngineButtons();
    if (onChanged) onChanged();
    previewNote(); // neuen Klangmotor gleich hoeren
}

void SidPanel::updateEngineButtons()
{
    TrackerEngine::Instrument s;
    const Engine e = proc.getSid (slot, s) ? s.engine : Engine::Classic;
    engineClassic.setToggleState (e == Engine::Classic,  juce::dontSendNotification);
    engineChip.setToggleState    (e == Engine::RealChip, juce::dontSendNotification);
}

void SidPanel::selectWave (Wave w)
{
    proc.editSid (slot, [w] (TrackerEngine::Instrument& i) { i.wave = w; });
    updateWaveButtons();
    if (onChanged) onChanged();
    previewNote(); // neue Wellenform gleich hoeren
}

void SidPanel::updateWaveButtons()
{
    TrackerEngine::Instrument s;
    const Wave w = proc.getSid (slot, s) ? s.wave : Wave::Pulse;
    waveTri.setToggleState   (w == Wave::Triangle, juce::dontSendNotification);
    waveSaw.setToggleState   (w == Wave::Saw,      juce::dontSendNotification);
    wavePulse.setToggleState (w == Wave::Pulse,    juce::dontSendNotification);
    waveNoise.setToggleState (w == Wave::Noise,    juce::dontSendNotification);

    // Pulsweite und PWM sind nur bei der Puls-Welle wirksam.
    const bool pulse = (w == Wave::Pulse);
    pwSlider.setEnabled (pulse);
    pwmRateSlider.setEnabled (pulse);
    pwmDepthSlider.setEnabled (pulse);
}

void SidPanel::selectFilter (Filter f)
{
    proc.editSid (slot, [f] (TrackerEngine::Instrument& i) { i.filter = f; });
    updateFilterButtons();
    if (onChanged) onChanged();
    previewNote();
}

void SidPanel::updateFilterButtons()
{
    TrackerEngine::Instrument s;
    const Filter f = proc.getSid (slot, s) ? s.filter : Filter::Off;
    filtOff.setToggleState  (f == Filter::Off,      juce::dontSendNotification);
    filtLow.setToggleState  (f == Filter::LowPass,  juce::dontSendNotification);
    filtHigh.setToggleState (f == Filter::HighPass, juce::dontSendNotification);
    filtBand.setToggleState (f == Filter::BandPass, juce::dontSendNotification);

    // Cutoff/Resonanz nur sinnvoll, wenn ein Filter aktiv ist.
    const bool on = (f != Filter::Off);
    cutoffSlider.setEnabled (on);
    resoSlider.setEnabled (on);
}

void SidPanel::toggleRing()
{
    const bool on = ! ringButton.getToggleState();
    proc.editSid (slot, [on] (TrackerEngine::Instrument& i) { i.ringMod = on; });
    updateModButtons();
    if (onChanged) onChanged();
    previewNote();
}

void SidPanel::toggleSync()
{
    const bool on = ! syncButton.getToggleState();
    proc.editSid (slot, [on] (TrackerEngine::Instrument& i) { i.sync = on; });
    updateModButtons();
    if (onChanged) onChanged();
    previewNote();
}

void SidPanel::updateModButtons()
{
    TrackerEngine::Instrument s;
    const bool have = proc.getSid (slot, s);
    const bool ring = have && s.ringMod;
    const bool sync = have && s.sync;
    ringButton.setToggleState (ring, juce::dontSendNotification);
    syncButton.setToggleState (sync, juce::dontSendNotification);

    // Mod-Tonhoehe nur sinnvoll, wenn Ring oder Sync an ist.
    modTuneSlider.setEnabled (ring || sync);
}

void SidPanel::writeParams()
{
    if (loading)
        return;

    const float pw  = (float) (pwSlider.getValue()      / 100.0);
    const float pwmR= (float)  pwmRateSlider.getValue();
    const float pwmD= (float) (pwmDepthSlider.getValue()/ 100.0);
    const float cut = (float) (cutoffSlider.getValue()  / 100.0);
    const float res = (float) (resoSlider.getValue()    / 100.0);
    const float mt  = (float)  modTuneSlider.getValue();
    const float a   = (float)  attackSlider.getValue();
    const float d   = (float)  decaySlider.getValue();
    const float sus = (float) (sustainSlider.getValue() / 100.0);
    const float rel = (float)  releaseSlider.getValue();

    proc.editSid (slot, [=] (TrackerEngine::Instrument& i)
    {
        i.pulseWidth = pw;
        i.pwmRate    = pwmR;
        i.pwmDepth   = pwmD;
        i.cutoff     = cut;
        i.resonance  = res;
        i.modTune    = mt;
        i.attack     = a;
        i.decay      = d;
        i.sustain    = sus;
        i.release    = rel;
    });
    if (onChanged) onChanged();
}

void SidPanel::previewNote()
{
    if (! proc.isSid (slot))
        return;

    // Halte-Zeit so waehlen, dass man Anstieg + Abfall sicher durchhoert,
    // bevor automatisch losgelassen wird - dann klingt der Ausklang (R) aus.
    double sr = proc.getSampleRate();
    if (sr <= 0.0)
        sr = 44100.0;
    const double a    = attackSlider.getValue();
    const double d    = decaySlider.getValue();
    const double hold = a + d + 0.45;          // Sekunden bis zum Note-Aus
    const int    gate = (int) (hold * sr);

    proc.engine.audition (60, slot, gate); // C-5 anschlagen, halten, loslassen
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
    // Klangmotor-Umschalter rechts in der Kopfzeile.
    engineChip.setBounds    (top.removeFromRight (120));
    top.removeFromRight (6);
    engineClassic.setBounds (top.removeFromRight (120));
    top.removeFromRight (10);
    engineLabel.setBounds   (top.removeFromRight (juce::jmax (0, juce::jmin (120, top.getWidth()))));
    area.removeFromTop (10);

    // Werks-Presets: eine Reihe gleich breiter Knoepfe quer ueber die Breite.
    presetLabel.setBounds (area.removeFromTop (16));
    {
        auto row = area.removeFromTop (28);
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
    area.removeFromTop (12);

    // Platz fuer die untere Zeile (TEST/SCHLIESSEN/Hinweis) reservieren.
    area.removeFromBottom (36);

    // Zwei Spalten: links Oszillator, rechts Filter + Huellkurve.
    auto left  = area.removeFromLeft ((area.getWidth() - 18) / 2);
    area.removeFromLeft (18);
    auto right = area;

    // Eine Reihe mit gleich breiten Knoepfen (Wellenform / Filter / Modulation).
    auto buttonRow = [] (juce::Rectangle<int>& col, juce::Label& lab,
                         std::initializer_list<juce::TextButton*> btns)
    {
        lab.setBounds (col.removeFromTop (16));
        auto row = col.removeFromTop (30);
        const int n  = (int) btns.size();
        const int bw = (row.getWidth() - (n - 1) * 6) / n;
        for (auto* b : btns)
        {
            b->setBounds (row.removeFromLeft (bw));
            row.removeFromLeft (6);
        }
        col.removeFromTop (10);
    };

    // Ein Regler je Zeile: links Beschriftung, rechts der Balken.
    auto sliderRow = [] (juce::Rectangle<int>& col, juce::Label& lab, juce::Slider& s)
    {
        auto row = col.removeFromTop (26);
        lab.setBounds (row.removeFromLeft (juce::jmin (140, row.getWidth() / 2)));
        row.removeFromLeft (6);
        s.setBounds (row);
        col.removeFromTop (8);
    };

    // Linke Spalte: Klangerzeugung.
    buttonRow (left, waveLabel, { &waveTri, &waveSaw, &wavePulse, &waveNoise });
    sliderRow (left, pwLabel,       pwSlider);
    sliderRow (left, pwmRateLabel,  pwmRateSlider);
    sliderRow (left, pwmDepthLabel, pwmDepthSlider);
    left.removeFromTop (6);
    buttonRow (left, modLabel, { &ringButton, &syncButton });
    sliderRow (left, modTuneLabel, modTuneSlider);

    // Rechte Spalte: Filter und Huellkurve.
    buttonRow (right, filterLabel, { &filtOff, &filtLow, &filtHigh, &filtBand });
    sliderRow (right, cutoffLabel, cutoffSlider);
    sliderRow (right, resoLabel,   resoSlider);
    right.removeFromTop (6);
    sliderRow (right, attackLabel,  attackSlider);
    sliderRow (right, decayLabel,   decaySlider);
    sliderRow (right, sustainLabel, sustainSlider);
    sliderRow (right, releaseLabel, releaseSlider);

    auto bottom = getLocalBounds().reduced (14).removeFromBottom (30);
    closeButton.setBounds (bottom.removeFromRight (120).reduced (0, 2));
    bottom.removeFromRight (8);
    testButton.setBounds (bottom.removeFromRight (90).reduced (0, 2));
    bottom.removeFromRight (12);
    hintLabel.setBounds (bottom);
}
