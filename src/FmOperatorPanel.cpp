#include "FmOperatorPanel.h"

// Kurzbeschreibung je Algorithmus, fuer den Live-Hilfetext unter dem Regler.
// Muss inhaltlich zu TrackerEngine::fmAlgorithm() passen (dort steht die
// eigentliche Verschaltung) - hier nur die Worte fuer Menschen dazu.
namespace
{
    struct AlgoText { const char* de; const char* en; };
    const AlgoText kAlgoText[8] =
    {
        { "1->2->3->4, nur 4 hoerbar: die klassische Kette, kraeftigste FM.",
          "1->2->3->4, only 4 audible: the classic chain, the hardest-hitting FM." },
        { "(1,2)->3->4: zwei Modulatoren treffen sich, dann weiter.",
          "(1,2)->3->4: two modulators meet, then continue on." },
        { "1->4 und 2->3->4: zwei Wege auf denselben Traeger.",
          "1->4 and 2->3->4: two paths onto the same carrier." },
        { "1->2->4 und 3->4.",
          "1->2->4 and 3->4." },
        { "1->2 und 3->4, beide hoerbar: zwei eigene Paare - E-Piano-Land.",
          "1->2 and 3->4, both audible: two separate pairs - e-piano territory." },
        { "1 moduliert 2, 3, 4 - alle drei hoerbar: glockig, breit.",
          "1 modulates 2, 3, 4 - all three audible: bell-like, wide." },
        { "1->2 hoerbar, 3 und 4 einfach dazu: Mischung aus FM und Orgel.",
          "1->2 audible, 3 and 4 simply added: a mix of FM and organ." },
        { "Alle vier parallel, keiner moduliert: rein additiv (Orgel).",
          "All four in parallel, none modulating: purely additive (organ)." },
    };
}

FmOperatorPanel::FmOperatorPanel (RetroTraxProcessor& processor) : proc (processor)
{
    setWantsKeyboardFocus (true);

    titleLabel.setFont (rt::mono (16.0f, true));
    titleLabel.setColour (juce::Label::textColourId, rt::cursor);
    addAndMakeVisible (titleLabel);

    slotLabel.setFont (rt::mono (13.0f, true));
    slotLabel.setColour (juce::Label::textColourId, rt::text);
    addAndMakeVisible (slotLabel);

    // Algorithmus + Rueckkopplung - dieselben zwei Werte wie im SynthPanel,
    // hier zusammen mit den Operatoren und einem Erklaertext.
    auto setupTopSlider = [this] (juce::Slider& s, juce::Label& lab,
                                  double lo, double hi, double step, const juce::String& suffix)
    {
        s.setSliderStyle (juce::Slider::LinearBar);
        s.setRange (lo, hi, step);
        s.setTextValueSuffix (suffix);
        s.onValueChange = [this] { if (! loading) writeAlgoFeedback(); };
        addAndMakeVisible (s);
        lab.setFont (rt::mono (12.0f, true));
        lab.setColour (juce::Label::textColourId, rt::textDim);
        addAndMakeVisible (lab);
    };
    setupTopSlider (algoSlider, algoLabel, 0.0, 7.0, 1.0, "");
    setupTopSlider (fbSlider,   fbLabel,   0.0, 100.0, 1.0, " %");

    algoTextLabel.setFont (rt::mono (11.5f));
    algoTextLabel.setColour (juce::Label::textColourId, rt::textDim);
    algoTextLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (algoTextLabel);

    // Vier Operatoren, jeder mit Kopf (Nummer + TRAEGER/MODULATOR-Tag) und
    // sechs Reglern. Werte wirken sofort (live, ohne Ton); beim Loslassen
    // einmal anspielen, damit man die Huellkurve durchhoert - gleiches
    // Prinzip wie beim SID-Editor.
    auto setupOpSlider = [this] (juce::Slider& s, juce::Label& lab, const juce::String& text,
                                 double lo, double hi, double step, const juce::String& suffix,
                                 int decimals, int op)
    {
        s.setSliderStyle (juce::Slider::LinearBar);
        s.setRange (lo, hi, step);
        s.setTextValueSuffix (suffix);
        s.setNumDecimalPlacesToDisplay (decimals);
        s.onValueChange = [this, op] { if (! loading) writeOperator (op); };
        s.onDragEnd     = [this] { previewNote(); };
        addAndMakeVisible (s);
        lab.setText (text, juce::dontSendNotification);
        lab.setFont (rt::mono (11.0f, true));
        lab.setColour (juce::Label::textColourId, rt::textDim);
        addAndMakeVisible (lab);
    };

    for (int op = 0; op < kOps; ++op)
    {
        auto& u = ops[(size_t) op];

        u.headerLabel.setText ("OP " + juce::String (op + 1), juce::dontSendNotification);
        u.headerLabel.setFont (rt::mono (13.0f, true));
        u.headerLabel.setColour (juce::Label::textColourId, rt::cursor);
        u.headerLabel.setJustificationType (juce::Justification::centred);
        addAndMakeVisible (u.headerLabel);

        u.tagLabel.setFont (rt::mono (10.0f, true));
        u.tagLabel.setColour (juce::Label::textColourId, rt::textDim);
        u.tagLabel.setJustificationType (juce::Justification::centred);
        addAndMakeVisible (u.tagLabel);

        setupOpSlider (u.ratioSlider, u.ratioLabel, loc::t ("RATIO", "RATIO"),
                       0.1, 16.0, 0.01, "", 2, op);
        setupOpSlider (u.levelSlider, u.levelLabel, loc::t ("PEGEL", "LEVEL"),
                       0.0, 100.0, 1.0, " %", 0, op);
        setupOpSlider (u.aSlider, u.aLabel, "A", 0.0, 3.0, 0.005, " s", 3, op);
        setupOpSlider (u.dSlider, u.dLabel, "D", 0.0, 3.0, 0.005, " s", 3, op);
        setupOpSlider (u.sSlider, u.sLabel, "S", 0.0, 100.0, 1.0, " %", 0, op);
        setupOpSlider (u.rSlider, u.rLabel, "R", 0.0, 3.0, 0.005, " s", 3, op);
    }

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

void FmOperatorPanel::applyLanguage()
{
    titleLabel.setText (loc::t ("FM-OPERATOREN", "FM OPERATORS"), juce::dontSendNotification);
    algoLabel.setText (loc::t ("ALGORITHMUS", "ALGORITHM"), juce::dontSendNotification);
    fbLabel.setText   (loc::t ("RUECKKOPPLUNG", "FEEDBACK"), juce::dontSendNotification);
    algoSlider.setTooltip (loc::t ("Wer moduliert wen - acht feste Verschaltungen, s. Text darunter",
                                   "Who modulates whom - eight fixed routings, see text below"));
    fbSlider.setTooltip (loc::t ("Operator 1 auf sich selbst - von sanft nach saegezahnartig/rau",
                                 "Operator 1 onto itself - from gentle to sawtooth-like and rough"));
    for (int op = 0; op < kOps; ++op)
    {
        auto& u = ops[(size_t) op];
        u.ratioSlider.setTooltip (loc::t ("Tonhoehe dieses Operators als Vielfaches der Note - "
                                          "ganze Zahlen klingen harmonisch, krumme metallisch/glockig",
                                          "This operator's pitch as a multiple of the note - "
                                          "whole numbers sound harmonic, odd ones metallic/bell-like"));
        u.levelSlider.setTooltip (loc::t ("Wie stark er wirkt - bei einem Modulator die Helligkeit/"
                                          "Schaerfe, bei einem Traeger die Lautstaerke",
                                          "How strongly it acts - brightness/edge for a modulator, "
                                          "loudness for a carrier"));
        u.aSlider.setTooltip (loc::t ("Attack dieses Operators", "This operator's attack"));
        u.dSlider.setTooltip (loc::t ("Decay dieses Operators", "This operator's decay"));
        u.sSlider.setTooltip (loc::t ("Sustain-Pegel dieses Operators", "This operator's sustain level"));
        u.rSlider.setTooltip (loc::t ("Release dieses Operators", "This operator's release"));
    }
    hintLabel.setText (loc::t (
        "Traeger sind hoerbar, Modulatoren faerben nur den Klang. "
        "Kurze Huellkurve am Modulator = heller Anschlag, der weich wird (E-Piano-Trick).",
        "Carriers are audible, modulators only colour the sound. "
        "A short envelope on a modulator = a bright attack that softens (the e-piano trick)."),
        juce::dontSendNotification);
    testButton.setButtonText (loc::t ("TEST", "TEST"));
    testButton.setTooltip (loc::t ("Aktuellen FM-Klang anspielen - anschlagen, halten, loslassen",
                                   "Play the current FM sound - attack, hold, release"));
    closeButton.setButtonText (loc::t ("SCHLIESSEN", "CLOSE"));
    updateAlgoText();
}

void FmOperatorPanel::refresh()
{
    slot = proc.currentInstrument.load();
    slotLabel.setText (loc::t ("Slot ", "Slot ") + juce::String::formatted ("%02d", slot + 1),
                       juce::dontSendNotification);

    TrackerEngine::Instrument s;
    if (! proc.getSid (slot, s))
        return; // Slot ist (noch) kein Synth-Instrument

    loading = true;
    algoSlider.setValue ((double) s.fmAlgo,             juce::dontSendNotification);
    fbSlider.setValue   ((double) s.fmFeedback * 100.0, juce::dontSendNotification);
    for (int op = 0; op < kOps; ++op)
    {
        auto& u = ops[(size_t) op];
        u.ratioSlider.setValue  ((double) s.fmRatio[op],           juce::dontSendNotification);
        u.levelSlider.setValue  ((double) s.fmLevel[op] * 100.0,   juce::dontSendNotification);
        u.aSlider.setValue      ((double) s.fmAttack[op],          juce::dontSendNotification);
        u.dSlider.setValue      ((double) s.fmDecay[op],           juce::dontSendNotification);
        u.sSlider.setValue      ((double) s.fmSustain[op] * 100.0, juce::dontSendNotification);
        u.rSlider.setValue      ((double) s.fmRelease[op],         juce::dontSendNotification);
    }
    loading = false;

    updateAlgoText();
}

void FmOperatorPanel::writeAlgoFeedback()
{
    if (! proc.isSid (slot))
        return;
    const int   algo = (int) algoSlider.getValue();
    const float fb   = (float) (fbSlider.getValue() / 100.0);
    proc.editSid (slot, [algo, fb] (TrackerEngine::Instrument& i)
    {
        i.fmAlgo     = algo;
        i.fmFeedback = fb;
    });
    updateAlgoText();
    if (onChanged) onChanged();
}

void FmOperatorPanel::writeOperator (int op)
{
    if (! proc.isSid (slot))
        return;
    auto& u = ops[(size_t) op];
    const float ratio   = (float) u.ratioSlider.getValue();
    const float level   = (float) (u.levelSlider.getValue() / 100.0);
    const float a       = (float) u.aSlider.getValue();
    const float d       = (float) u.dSlider.getValue();
    const float sVal    = (float) (u.sSlider.getValue() / 100.0);
    const float r       = (float) u.rSlider.getValue();
    proc.editSid (slot, [op, ratio, level, a, d, sVal, r] (TrackerEngine::Instrument& i)
    {
        i.fmRatio[op]   = ratio;
        i.fmLevel[op]   = level;
        i.fmAttack[op]  = a;
        i.fmDecay[op]   = d;
        i.fmSustain[op] = sVal;
        i.fmRelease[op] = r;
    });
    if (onChanged) onChanged();
}

// Beschreibungstext + TRAEGER/MODULATOR-Tags aus der Algorithmus-Verschaltung
// ableiten (TrackerEngine::fmAlgorithm() - dieselbe Quelle, die auch der
// Klangmotor selbst benutzt, damit hier nie etwas anderes steht als das,
// was tatsaechlich klingt).
void FmOperatorPanel::updateAlgoText()
{
    const int algo = juce::jlimit (0, 7, (int) algoSlider.getValue());
    algoTextLabel.setText (loc::t (kAlgoText[algo].de, kAlgoText[algo].en),
                           juce::dontSendNotification);

    const auto routing = TrackerEngine::fmAlgorithm (algo);
    for (int op = 0; op < kOps; ++op)
        ops[(size_t) op].tagLabel.setText (
            routing.carrier[op] ? loc::t ("TRAEGER", "CARRIER")
                                 : loc::t ("MODULATOR", "MODULATOR"),
            juce::dontSendNotification);
}

void FmOperatorPanel::previewNote()
{
    if (! proc.isSid (slot))
        return;

    double sr = proc.getSampleRate();
    if (sr <= 0.0)
        sr = 44100.0;

    // Haltezeit an den langsamsten Operator anlehnen, damit man Anstieg und
    // Abfall aller vier sicher durchhoert, bevor automatisch losgelassen wird.
    double slowestAd = 0.0;
    for (const auto& u : ops)
        slowestAd = juce::jmax (slowestAd, u.aSlider.getValue() + u.dSlider.getValue());
    const double hold = slowestAd + 0.45;
    const int    gate = (int) (hold * sr);

    proc.engine.audition (60, slot, gate); // C-5 anschlagen, halten, loslassen
}

bool FmOperatorPanel::keyPressed (const juce::KeyPress& key)
{
    if (key.getKeyCode() == juce::KeyPress::escapeKey && onClose != nullptr)
    {
        onClose();
        return true;
    }
    return false;
}

void FmOperatorPanel::paint (juce::Graphics& g)
{
    g.fillAll (rt::bg);
    g.setColour (rt::steel.withAlpha (0.7f));
    g.drawRect (getLocalBounds(), 1);
}

void FmOperatorPanel::resized()
{
    auto area = getLocalBounds().reduced (14);

    auto top = area.removeFromTop (26);
    titleLabel.setBounds (top.removeFromLeft (240));
    slotLabel.setBounds  (top.removeFromLeft (120));
    area.removeFromTop (10);

    // Algorithmus + Rueckkopplung nebeneinander, darunter der Erklaertext.
    {
        auto row = area.removeFromTop (26);
        const int half = (row.getWidth() - 18) / 2;
        auto algoCol = row.removeFromLeft (half);
        row.removeFromLeft (18);
        auto fbCol = row;

        algoLabel.setBounds  (algoCol.removeFromLeft (juce::jmin (110, algoCol.getWidth() / 2)));
        algoSlider.setBounds (algoCol);
        fbLabel.setBounds    (fbCol.removeFromLeft (juce::jmin (110, fbCol.getWidth() / 2)));
        fbSlider.setBounds   (fbCol);
    }
    area.removeFromTop (6);
    algoTextLabel.setBounds (area.removeFromTop (18));
    area.removeFromTop (14);

    // Platz fuer die untere Zeile (TEST/SCHLIESSEN/Hinweis) reservieren.
    area.removeFromBottom (36);

    // Vier gleich breite Operator-Spalten.
    const int gap = 16;
    const int colW = (area.getWidth() - (kOps - 1) * gap) / kOps;
    for (int op = 0; op < kOps; ++op)
    {
        auto col = area.removeFromLeft (colW);
        if (op < kOps - 1)
            area.removeFromLeft (gap);
        auto& u = ops[(size_t) op];

        u.headerLabel.setBounds (col.removeFromTop (20));
        u.tagLabel.setBounds    (col.removeFromTop (16));
        col.removeFromTop (8);

        auto sliderRow = [&col] (juce::Label& lab, juce::Slider& s)
        {
            auto row = col.removeFromTop (26);
            lab.setBounds (row.removeFromLeft (44));
            row.removeFromLeft (4);
            s.setBounds (row);
            col.removeFromTop (6);
        };
        sliderRow (u.ratioLabel, u.ratioSlider);
        sliderRow (u.levelLabel, u.levelSlider);
        col.removeFromTop (4);
        sliderRow (u.aLabel, u.aSlider);
        sliderRow (u.dLabel, u.dSlider);
        sliderRow (u.sLabel, u.sSlider);
        sliderRow (u.rLabel, u.rSlider);
    }

    auto bottom = getLocalBounds().reduced (14).removeFromBottom (30);
    closeButton.setBounds (bottom.removeFromRight (120).reduced (0, 2));
    bottom.removeFromRight (8);
    testButton.setBounds (bottom.removeFromRight (90).reduced (0, 2));
    bottom.removeFromRight (12);
    hintLabel.setBounds (bottom);
}
