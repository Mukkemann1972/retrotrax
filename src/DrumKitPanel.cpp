#include "DrumKitPanel.h"

DrumKitPanel::DrumKitPanel (RetroTraxProcessor& processor) : proc (processor)
{
    setWantsKeyboardFocus (true);

    titleLabel.setFont (rt::mono (16.0f, true));
    titleLabel.setColour (juce::Label::textColourId, rt::text);
    addAndMakeVisible (titleLabel);

    addAndMakeVisible (drumInputButton);
    drumInputButton.setClickingTogglesState (true);
    drumInputButton.onClick = [this]
    {
        const bool on = drumInputButton.getToggleState();
        proc.drumInput = on;
        setHint (on ? "Drum-Eingabe AN: Tasten 1234/QWER/ASDF/YXCV legen die Pads in die Spur (Panel schliessen). Erst ALLE IN SLOTS!"
                    : "Drum-Eingabe AUS: die Tasten sind wieder normale Noten.",
                 on ? "Drum input ON: keys 1234/QWER/ASDF/ZXCV put pads into the track (close the panel). First ALL TO SLOTS!"
                    : "Drum input OFF: the keys are normal notes again.");
    };

    addAndMakeVisible (kitsButton);
    kitsButton.onClick = [this]
    {
        juce::PopupMenu m;
        m.addItem (1, loc::t ("Kit speichern ...", "Save kit ..."));
        m.addItem (2, loc::t ("Kit laden ...", "Load kit ..."));
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&kitsButton),
            [this] (int r) { if (r == 1) saveKitDialog(); else if (r == 2) loadKitDialog(); });
    };

    auto setupButton = [this] (juce::TextButton& b) { addAndMakeVisible (b); };
    setupButton (loadButton);
    setupButton (clearButton);
    setupButton (allSlotsButton);
    setupButton (toSlotButton);
    setupButton (fromSlotBtn);
    setupButton (closeButton);

    loadButton.onClick   = [this] { loadIntoSelected(); };
    clearButton.onClick  = [this] { proc.clearPad (selected); refresh(); repaint(); };
    allSlotsButton.onClick = [this]
    {
        int n = 0;
        for (int i = 0; i < TrackerEngine::kPads; ++i)
            if (proc.padToSlot (i, i)) ++n; // Pad i -> Instrument-Slot i
        setHint (juce::String (n) + " Pads in die Slots 1-16 gelegt (fuer die Drum-Spur).",
                 juce::String (n) + " pads copied into slots 1-16 (for the drum track).");
    };
    toSlotButton.onClick = [this]
    {
        const int slot = proc.currentInstrument.load();
        if (proc.padToSlot (selected, slot))
            setHint ("Pad " + juce::String (selected + 1) + " in Slot "
                         + juce::String (slot + 1) + " kopiert.",
                     "Pad " + juce::String (selected + 1) + " copied to slot "
                         + juce::String (slot + 1) + ".");
        else
            setHint ("Dieses Pad ist leer.", "This pad is empty.");
    };
    fromSlotBtn.onClick = [this]
    {
        const int slot = proc.currentInstrument.load();
        if (proc.slotToPad (slot, selected))
        {
            refresh(); repaint();
            setHint ("Slot " + juce::String (slot + 1) + " in Pad "
                         + juce::String (selected + 1) + " kopiert.",
                     "Slot " + juce::String (slot + 1) + " copied to pad "
                         + juce::String (selected + 1) + ".");
        }
        else
            setHint ("Im aktuellen Slot liegt kein Sample.", "No sample in the current slot.");
    };
    closeButton.onClick = [this] { if (onClose) onClose(); };

    hintLabel.setFont (rt::mono (12.0f, false));
    hintLabel.setColour (juce::Label::textColourId, rt::textDim);
    hintLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (hintLabel);

    // --- Charakter-Regler fuers gewaehlte Pad (SP-1200/Emu) ------------------
    selLabel.setFont (rt::mono (12.0f, true));
    selLabel.setColour (juce::Label::textColourId, rt::steelHi);
    addAndMakeVisible (selLabel);

    auto setupCharSlider = [this] (juce::Slider& s, juce::Label& lab, double lo, double hi)
    {
        s.setSliderStyle (juce::Slider::LinearBar);
        s.setRange (lo, hi, hi - lo > 80 ? 1.0 : 0.5);
        s.onValueChange = [this] { writePadParams(); };
        s.onDragEnd     = [this] { triggerPad (selected); };
        addAndMakeVisible (s);
        lab.setFont (rt::mono (12.0f, true));
        lab.setColour (juce::Label::textColourId, rt::textDim);
        addAndMakeVisible (lab);
    };
    setupCharSlider (tuneSlider, tuneLabel, -24.0, 24.0); // Halbtoene
    setupCharSlider (gritSlider, gritLabel,   0.0, 100.0); // SR-Reduktion %
    tuneSlider.setTextValueSuffix (" st");
    gritSlider.setTextValueSuffix (" %");

    bitButton.setClickingTogglesState (true);
    bitButton.onClick = [this] { writePadParams(); triggerPad (selected); };
    addAndMakeVisible (bitButton);
    spButton.onClick  = [this] { applySP1200(); };
    addAndMakeVisible (spButton);

    refresh();
    setSelected (0);
    applyLanguage();
    startTimerHz (30);
}

DrumKitPanel::~DrumKitPanel()
{
    stopTimer();
}

void DrumKitPanel::applyLanguage()
{
    titleLabel.setText (loc::t ("DRUMSAMPLER (16 PADS)", "DRUMSAMPLER (16 PADS)"),
                        juce::dontSendNotification);
    kitsButton.setButtonText (loc::t ("KITS", "KITS"));
    kitsButton.setTooltip (loc::t ("Ganzes Kit speichern/laden (.retrokit, mit Samples)",
                                   "Save/load the whole kit (.retrokit, with samples)"));
    drumInputButton.setButtonText (loc::t ("DRUM-EINGABE", "DRUM INPUT"));
    drumInputButton.setTooltip (loc::t ("Tasten 1234/QWER/ASDF/YXCV legen die Pads direkt in die Spur (Panel schliessen, dann tippen). Erst ALLE IN SLOTS.",
                                        "Keys 1234/QWER/ASDF/ZXCV put the pads straight into the track (close the panel, then type). Do ALL TO SLOTS first."));
    loadButton.setButtonText   (loc::t ("LADEN", "LOAD"));
    loadButton.setTooltip (loc::t ("Sample-Datei in das gewaehlte Pad laden",
                                   "Load a sample file into the selected pad"));
    clearButton.setButtonText  (loc::t ("LEEREN", "CLEAR"));
    allSlotsButton.setButtonText (loc::t ("ALLE IN SLOTS", "ALL TO SLOTS"));
    allSlotsButton.setTooltip (loc::t ("Alle 16 Pads in die Instrument-Slots 1-16 legen - dann mit DRUM-Eingabe als eine Spur spielen",
                                       "Put all 16 pads into instrument slots 1-16 - then play as one track with DRUM input"));
    toSlotButton.setButtonText (loc::t ("PAD IN SLOT", "PAD TO SLOT"));
    toSlotButton.setTooltip (loc::t ("Das gewaehlte Pad in den aktuellen Spur-Slot kopieren (fuer den Tracker)",
                                     "Copy the selected pad into the current track slot (for the tracker)"));
    fromSlotBtn.setButtonText  (loc::t ("SLOT IN PAD", "SLOT TO PAD"));
    fromSlotBtn.setTooltip (loc::t ("Das Sample aus dem aktuellen Spur-Slot in dieses Pad holen",
                                    "Take the sample from the current track slot into this pad"));
    closeButton.setButtonText  (loc::t ("SCHLIESSEN", "CLOSE"));
    tuneLabel.setText (loc::t ("STIMMUNG", "TUNE"), juce::dontSendNotification);
    tuneSlider.setTooltip (loc::t ("Stimmung des Pads in Halbtoenen (wie SP-1200/MPC) - runter = dicker/crunchy",
                                   "Pad tuning in semitones (like SP-1200/MPC) - down = fatter/crunchier"));
    gritLabel.setText (loc::t ("GRIT", "GRIT"), juce::dontSendNotification);
    gritSlider.setTooltip (loc::t ("Koernung/Sample-Rate-Reduktion - der dreckige SP-1200/Emu-Charakter",
                                   "Grain/sample-rate reduction - the dirty SP-1200/Emu character"));
    bitButton.setButtonText (loc::t ("12-BIT", "12-BIT"));
    bitButton.setTooltip (loc::t ("12-Bit-Crunch wie die alten 12-Bit-Drum-Sampler",
                                  "12-bit crunch like the old 12-bit drum samplers"));
    spButton.setButtonText (loc::t ("SP-1200", "SP-1200"));
    spButton.setTooltip (loc::t ("Ein Klick: klassischer 12-Bit-SP-1200-Crunch aufs Pad",
                                 "One click: classic 12-bit SP-1200 crunch on the pad"));
    refreshPadControls();
    setHint ("Pad klicken oder per Tastatur (1234/QWER/ASDF/YXCV) trommeln.",
             "Click a pad or finger-drum on the keyboard (1234/QWER/ASDF/ZXCV).");
}

void DrumKitPanel::refresh()
{
    for (int p = 0; p < TrackerEngine::kPads; ++p)
    {
        juce::String name;
        padFilled[p] = proc.getPadName (p, name);
        padNames[p]  = name;
    }
    drumInputButton.setToggleState (proc.drumInput.load(), juce::dontSendNotification);
    refreshPadControls();
}

void DrumKitPanel::setHint (const juce::String& de, const juce::String& en)
{
    hintLabel.setText (loc::t (de, en), juce::dontSendNotification);
}

void DrumKitPanel::setSelected (int pad)
{
    selected = juce::jlimit (0, TrackerEngine::kPads - 1, pad);
    refreshPadControls();
}

void DrumKitPanel::refreshPadControls()
{
    selLabel.setText (loc::t ("PAD ", "PAD ") + juce::String (selected + 1),
                      juce::dontSendNotification);

    TrackerEngine::Instrument s;
    const bool have = proc.getPad (selected, s);

    loadingCtl = true;
    tuneSlider.setValue (have ? (double) s.tuneSemis   :  0.0, juce::dontSendNotification);
    gritSlider.setValue (have ? (double) s.srReduction * 100.0 : 0.0, juce::dontSendNotification);
    bitButton.setToggleState (have && s.akai12bit, juce::dontSendNotification);
    loadingCtl = false;

    tuneSlider.setEnabled (have);
    gritSlider.setEnabled (have);
    bitButton.setEnabled (have);
    spButton.setEnabled (have);
}

void DrumKitPanel::writePadParams()
{
    if (loadingCtl)
        return;
    const float tune = (float) tuneSlider.getValue();
    const float grit = (float) (gritSlider.getValue() / 100.0);
    const bool  bit  = bitButton.getToggleState();
    proc.editPad (selected, [=] (TrackerEngine::Instrument& i)
    {
        i.tuneSemis   = tune;
        i.srReduction = grit;
        i.akai12bit   = bit;
    });
}

void DrumKitPanel::applySP1200()
{
    // Klassischer 12-Bit-SP-1200-Crunch: 12-Bit an, etwas Koernung, rohe
    // Wandlung beim Pitchen (vintage) - der dreckige, druckvolle Drum-Charakter.
    proc.editPad (selected, [] (TrackerEngine::Instrument& i)
    {
        i.akai12bit    = true;
        i.srReduction  = juce::jmax (i.srReduction, 0.30f);
        i.vintagePitch = true;
    });
    refreshPadControls();
    triggerPad (selected);
    setHint ("SP-1200-Crunch auf Pad " + juce::String (selected + 1) + " gelegt.",
             "SP-1200 crunch applied to pad " + juce::String (selected + 1) + ".");
}

// MPC-Layout: Pad 1 liegt UNTEN LINKS. Sichtzeile 0 (oben) zeigt Pad 13..16.
int DrumKitPanel::padAtIndex (int visCol, int visRow) const
{
    return (3 - visRow) * 4 + visCol;
}

juce::Rectangle<int> DrumKitPanel::padBounds (int pad) const
{
    const int visCol = pad % 4;
    const int visRow = 3 - (pad / 4);
    const int cw = gridRect.getWidth()  / 4;
    const int ch = gridRect.getHeight() / 4;
    return { gridRect.getX() + visCol * cw, gridRect.getY() + visRow * ch, cw, ch };
}

int DrumKitPanel::padFromKey (const juce::KeyPress& key) const
{
    const auto c = (int) juce::CharacterFunctions::toLowerCase ((juce::juce_wchar) key.getTextCharacter());
    static const char* const rows[4] = { "1234", "qwer", "asdf", "yxcv" };
    for (int r = 0; r < 4; ++r)
        for (int col = 0; col < 4; ++col)
            if (c == (int) rows[r][col])
                return padAtIndex (col, r);
    if (c == (int) 'z') return padAtIndex (0, 3); // QWERTY-Variante fuer unten links
    return -1;
}

void DrumKitPanel::triggerPad (int pad, int velocity)
{
    if (pad < 0 || pad >= TrackerEngine::kPads)
        return;
    if (pad != selected)
        setSelected (pad);
    velocity = juce::jlimit (1, 64, velocity);
    proc.engine.auditionPad (pad, 60, -1, velocity); // C-5, One-Shot, mit Anschlag
    padGlow[pad] = 0.35f + 0.65f * (float) velocity / 64.0f; // harter Schlag = heller
    repaint();
}

void DrumKitPanel::mouseDown (const juce::MouseEvent& e)
{
    for (int p = 0; p < TrackerEngine::kPads; ++p)
    {
        const auto r = padBounds (p);
        if (r.contains (e.getPosition()))
        {
            setSelected (p);
            if (padFilled[p])
            {
                // Anschlagdynamik aus der Klickhoehe: oben = hart/laut, unten = leise.
                const float rel = juce::jlimit (0.0f, 1.0f,
                    (float) (r.getBottom() - e.y) / (float) juce::jmax (1, r.getHeight()));
                triggerPad (p, 10 + (int) (rel * 54.0f));
            }
            else if (e.getNumberOfClicks() >= 2)
                loadIntoSelected();        // Doppelklick auf leeres Pad = laden
            repaint();
            return;
        }
    }
}

bool DrumKitPanel::keyPressed (const juce::KeyPress& key)
{
    if (key.getKeyCode() == juce::KeyPress::escapeKey)
    {
        if (onClose) onClose();
        return true;
    }
    const int pad = padFromKey (key);
    if (pad >= 0)
    {
        triggerPad (pad);
        return true;
    }
    return false;
}

void DrumKitPanel::loadIntoSelected()
{
    chooser = std::make_unique<juce::FileChooser> (
        loc::t ("Sample fuer Pad ", "Sample for pad ") + juce::String (selected + 1),
        juce::File::getSpecialLocation (juce::File::userMusicDirectory),
        "*.wav;*.aiff;*.aif;*.flac;*.ogg;*.mp3;*.iff;*.8svx");

    chooser->launchAsync (juce::FileBrowserComponent::openMode
                              | juce::FileBrowserComponent::canSelectFiles,
        [this] (const juce::FileChooser& fc)
        {
            const auto file = fc.getResult();
            if (file == juce::File())
                return;
            if (proc.loadPad (selected, file))
            {
                refresh();
                repaint();
                triggerPad (selected); // gleich anspielen
            }
            else
                setHint ("Konnte das Sample nicht laden.", "Could not load the sample.");
        });
}

void DrumKitPanel::saveKitDialog()
{
    auto dir = juce::File::getSpecialLocation (juce::File::userMusicDirectory)
                   .getChildFile ("RetroTrax").getChildFile ("Kits");
    dir.createDirectory();
    chooser = std::make_unique<juce::FileChooser> (
        loc::t ("Kit speichern", "Save kit"), dir.getChildFile ("Mein Kit.retrokit"), "*.retrokit");
    chooser->launchAsync (juce::FileBrowserComponent::saveMode
                              | juce::FileBrowserComponent::canSelectFiles,
        [this] (const juce::FileChooser& fc)
        {
            const auto file = fc.getResult();
            if (file == juce::File()) return;
            juce::String msg;
            proc.saveKit (file.withFileExtension ("retrokit"), msg);
            setHint (msg, msg);
        });
}

void DrumKitPanel::loadKitDialog()
{
    auto dir = juce::File::getSpecialLocation (juce::File::userMusicDirectory)
                   .getChildFile ("RetroTrax").getChildFile ("Kits");
    chooser = std::make_unique<juce::FileChooser> (
        loc::t ("Kit laden", "Load kit"), dir, "*.retrokit");
    chooser->launchAsync (juce::FileBrowserComponent::openMode
                              | juce::FileBrowserComponent::canSelectFiles,
        [this] (const juce::FileChooser& fc)
        {
            const auto file = fc.getResult();
            if (file == juce::File()) return;
            juce::String msg;
            proc.loadKit (file, msg);
            refresh();
            repaint();
            setHint (msg, msg);
        });
}

void DrumKitPanel::timerCallback()
{
    bool any = false;
    for (auto& gphase : padGlow)
    {
        if (gphase > 0.0f)
        {
            gphase = juce::jmax (0.0f, gphase - 0.10f);
            any = true;
        }
    }
    if (any)
        repaint (gridRect);
}

void DrumKitPanel::paint (juce::Graphics& g)
{
    g.fillAll (rt::bg);
    g.setColour (rt::steel.withAlpha (0.7f));
    g.drawRect (getLocalBounds(), 1);

    for (int p = 0; p < TrackerEngine::kPads; ++p)
    {
        auto r = padBounds (p).toFloat().reduced (5.0f);

        // Grundfarbe: dunkles Pad, leicht in der Instrument-Palettenfarbe getoent;
        // belegte Pads etwas heller. Beim Anschlag in Richtung Bernstein aufleuchten.
        auto base = (padFilled[p] ? rt::panel : rt::panel.darker (0.55f))
                        .interpolatedWith (rt::instColour (p), padFilled[p] ? 0.22f : 0.09f);
        if (padGlow[p] > 0.0f)
            base = base.interpolatedWith (rt::cursor, 0.65f * padGlow[p]);

        g.setColour (base);
        g.fillRoundedRectangle (r, 7.0f);

        // dezenter Hochglanz oben (Bevel) fuer das Geraete-Gefuehl.
        g.setColour (juce::Colours::white.withAlpha (0.06f));
        g.fillRoundedRectangle (r.withHeight (r.getHeight() * 0.45f).reduced (2.0f), 6.0f);

        // Rahmen - gewaehltes Pad hell hervorheben.
        const bool sel = (p == selected);
        g.setColour (sel ? rt::steelHi : rt::steel.withAlpha (0.45f));
        g.drawRoundedRectangle (r, 7.0f, sel ? 2.2f : 1.0f);

        // Pad-Nummer oben links.
        g.setColour (rt::textDim);
        g.setFont (rt::mono (11.0f, true));
        g.drawText (juce::String (p + 1),
                    r.reduced (7.0f).removeFromTop (14.0f).toNearestInt(),
                    juce::Justification::topLeft);

        // Sample-Name (oder "leer") mittig.
        g.setColour (padFilled[p] ? rt::text : rt::textDim);
        g.setFont (rt::mono (12.0f, false));
        const auto nm = padFilled[p] ? padNames[p] : loc::t ("leer", "empty");
        g.drawFittedText (nm, r.reduced (8.0f).toNearestInt(),
                          juce::Justification::centred, 2);
    }
}

void DrumKitPanel::resized()
{
    auto area = getLocalBounds().reduced (14);

    {
        auto top = area.removeFromTop (26);
        kitsButton.setBounds (top.removeFromRight (90));
        top.removeFromRight (8);
        drumInputButton.setBounds (top.removeFromRight (150));
        titleLabel.setBounds (top);
    }
    area.removeFromTop (8);

    // Unten: Knopfreihe + Hinweiszeile darueber.
    auto bottom = area.removeFromBottom (34);
    {
        closeButton.setBounds (bottom.removeFromRight (140));
        bottom.removeFromRight (10);
        const int bw = (bottom.getWidth() - 4 * 8) / 5;
        loadButton.setBounds     (bottom.removeFromLeft (bw)); bottom.removeFromLeft (8);
        clearButton.setBounds    (bottom.removeFromLeft (bw)); bottom.removeFromLeft (8);
        allSlotsButton.setBounds (bottom.removeFromLeft (bw)); bottom.removeFromLeft (8);
        toSlotButton.setBounds   (bottom.removeFromLeft (bw)); bottom.removeFromLeft (8);
        fromSlotBtn.setBounds    (bottom.removeFromLeft (bw));
    }
    area.removeFromBottom (6);
    hintLabel.setBounds (area.removeFromBottom (20));
    area.removeFromBottom (8);

    // Charakter-Reihe fuers gewaehlte Pad: PAD n | STIMMUNG | GRIT | 12-BIT | SP-1200.
    {
        auto row = area.removeFromBottom (28);
        selLabel.setBounds (row.removeFromLeft (66));
        row.removeFromLeft (8);
        spButton.setBounds  (row.removeFromRight (96));
        row.removeFromRight (8);
        bitButton.setBounds (row.removeFromRight (84));
        row.removeFromRight (12);
        const int half = (row.getWidth() - 12) / 2;
        tuneLabel.setBounds (row.removeFromLeft (74));
        tuneSlider.setBounds (row.removeFromLeft (half - 74));
        row.removeFromLeft (12);
        gritLabel.setBounds (row.removeFromLeft (52));
        gritSlider.setBounds (row);
    }
    area.removeFromBottom (10);

    gridRect = area; // der Rest ist das 4x4-Pad-Feld
}
