#include "SampleEditPanel.h"

// Time-Stretch ohne Tonhoehenaenderung: granulares Overlap-Add (OLA). Koernchen
// mit Hann-Fenster, 50 % Ueberlappung (Cosinus-Fenster summieren sich dann zu ~1).
// factor > 1 = laenger/langsamer, < 1 = kuerzer/schneller - die Tonhoehe bleibt,
// weil die Koernchen selbst unveraendert gelesen werden. Klang ist bewusst
// einfach/lo-fi (passt zum Vintage-Charakter), kein Studio-Phase-Vocoder.
static juce::AudioBuffer<float> timeStretch (const juce::AudioBuffer<float>& in, double factor)
{
    factor = juce::jlimit (0.25, 4.0, factor);
    const int C = in.getNumChannels();
    const int N = in.getNumSamples();
    if (N < 8 || C < 1)
        return in;

    const int outLen = juce::jmax (8, (int) std::round (N * factor));
    const int G      = juce::jlimit (256, 4096, N / 8); // Koerngroesse
    const int hopOut = G / 2;                           // 50 % Ueberlappung

    std::vector<float> win ((size_t) G);
    for (int i = 0; i < G; ++i)
        win[(size_t) i] = 0.5f - 0.5f * std::cos (2.0f * juce::MathConstants<float>::pi
                                                  * (float) i / (float) (G - 1));

    juce::AudioBuffer<float> out (C, outLen);
    out.clear();
    for (int k = 0; ; ++k)
    {
        const int outStart = k * hopOut;
        if (outStart >= outLen)
            break;
        const int inStart = (int) std::round (outStart / factor);
        for (int c = 0; c < C; ++c)
        {
            const float* src = in.getReadPointer (c);
            float*       dst = out.getWritePointer (c);
            for (int i = 0; i < G; ++i)
            {
                const int oi = outStart + i;
                if (oi >= outLen) break;
                const int si = inStart + i;
                if (si >= 0 && si < N)
                    dst[oi] += src[si] * win[(size_t) i];
            }
        }
    }
    return out;
}

SampleEditPanel::SampleEditPanel (RetroTraxProcessor& processor) : proc (processor)
{
    setWantsKeyboardFocus (true);

    titleLabel.setFont (rt::mono (16.0f, true));
    titleLabel.setColour (juce::Label::textColourId, juce::Colour (0xff39ff7a)); // Fairlight-Gruen
    addAndMakeVisible (titleLabel);

    for (auto* b : { &waveButton, &trimButton, &cutButton, &normButton, &revButton, &drawButton,
                     &chopButton, &chopPatButton, &loopButton, &exportButton,
                     &previewButton, &applyButton, &closeButton })
        addAndMakeVisible (b);

    cutButton.onClick = [this] { cutSelection(); };
    waveButton.onClick = [this]
    {
        juce::PopupMenu m;
        m.addItem (1, loc::t ("Sinus", "Sine"));
        m.addItem (2, loc::t ("Saege", "Saw"));
        m.addItem (3, loc::t ("Rechteck", "Square"));
        m.addItem (4, loc::t ("Dreieck", "Triangle"));
        m.addItem (5, loc::t ("Puls 25%", "Pulse 25%"));
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&waveButton),
            [this] (int r) { if (r >= 1) fillWave (r - 1); });
    };

    drawButton.setClickingTogglesState (true);
    startTimerHz (30); // Laufmarke beim Vorhoeren
    loopButton.setClickingTogglesState (true);
    loopButton.onClick = [this]
    {
        loopOn = loopButton.getToggleState();
        setHint (loopOn ? "Loop: das Sample laeuft in der Schleife." : "One-Shot: einmal abspielen.",
                 loopOn ? "Loop: the sample plays in a loop." : "One-shot: play once.");
    };
    exportButton.onClick = [this]
    {
        auto dir = juce::File::getSpecialLocation (juce::File::userMusicDirectory)
                       .getChildFile ("RetroTrax").getChildFile ("Samples");
        dir.createDirectory();
        chooser = std::make_unique<juce::FileChooser> (
            loc::t ("Sample speichern", "Save sample"), dir.getChildFile ("Sample.wav"), "*.wav");
        chooser->launchAsync (juce::FileBrowserComponent::saveMode
                                  | juce::FileBrowserComponent::canSelectFiles,
            [this] (const juce::FileChooser& fc)
            {
                const auto file = fc.getResult();
                if (file == juce::File()) return;
                juce::String msg;
                proc.exportSample (work, rate, file, msg);
                setHint (msg, msg);
            });
    };

    trimButton.onClick = [this]
    {
        if (! hasSel || work.getNumSamples() < 4)
        {
            setHint ("Erst einen Bereich markieren (ziehen).", "Select a range first (drag).");
            return;
        }
        const int len = work.getNumSamples();
        const int a = juce::jlimit (0, len - 1, (int) (juce::jmin (selStart, selEnd) * len));
        const int b = juce::jlimit (a + 1, len, (int) (juce::jmax (selStart, selEnd) * len));
        const int n = b - a;
        juce::AudioBuffer<float> cut (work.getNumChannels(), n);
        for (int c = 0; c < work.getNumChannels(); ++c)
            cut.copyFrom (c, 0, work, c, a, n);
        work = std::move (cut);
        hasSel = false;
        setHint ("Auf die Auswahl getrimmt.", "Trimmed to selection.");
        repaint();
    };

    normButton.onClick = [this]
    {
        float peak = 0.0f;
        for (int c = 0; c < work.getNumChannels(); ++c)
            peak = juce::jmax (peak, work.getMagnitude (c, 0, work.getNumSamples()));
        if (peak > 1.0e-6f)
        {
            work.applyGain (0.98f / peak);
            setHint ("Normalisiert.", "Normalised.");
            repaint();
        }
    };

    revButton.onClick = [this]
    {
        for (int c = 0; c < work.getNumChannels(); ++c)
            work.reverse (c, 0, work.getNumSamples());
        setHint ("Umgekehrt.", "Reversed.");
        repaint();
    };

    drawButton.onClick = [this]
    {
        freehand = drawButton.getToggleState();
        if (freehand)
            setHint ("FREIHAND an: mit der Maus die Wellenform zeichnen (Lichtgriffel).",
                     "FREEHAND on: draw the waveform with the mouse (light pen).");
        else
            setHint ("FREIHAND aus: Ziehen markiert wieder einen Bereich.",
                     "FREEHAND off: dragging selects a range again.");
    };

    chopButton.onClick = [this]
    {
        juce::String msg;
        const int n = proc.chopToKit (work, rate, 16, "Chop", msg);
        setHint (msg, msg);
        if (n > 0)
            setHint (msg + " (im KIT zu sehen)", msg + " (see the KIT)");
    };

    chopPatButton.onClick = [this]
    {
        juce::String msg;
        proc.sliceToPattern (work, rate, 16, "Slice", msg);
        setHint (msg, msg);
    };

    previewButton.onClick = [this] { proc.previewBuffer (work, rate, loopOn); };

    applyButton.onClick = [this]
    {
        juce::String msg;
        proc.applyEditedSample (slot, work, rate, msg, loopOn);
        setHint (msg, msg);
    };

    closeButton.onClick = [this] { if (onClose) onClose(); };

    // Time-Stretch-Reihe.
    stretchLabel.setFont (rt::mono (12.0f, true));
    stretchLabel.setColour (juce::Label::textColourId, rt::textDim);
    addAndMakeVisible (stretchLabel);
    stretchSlider.setSliderStyle (juce::Slider::LinearBar);
    stretchSlider.setRange (0.5, 2.0, 0.01);
    stretchSlider.setValue (1.0, juce::dontSendNotification);
    stretchSlider.setTextValueSuffix ("x");
    stretchSlider.setNumDecimalPlacesToDisplay (2);
    addAndMakeVisible (stretchSlider);
    addAndMakeVisible (stretchButton);
    stretchButton.onClick = [this]
    {
        const double f = stretchSlider.getValue();
        if (std::abs (f - 1.0) < 0.005 || work.getNumSamples() < 8)
        {
            setHint ("Faktor einstellen (z.B. 2x laenger, 0.5x kuerzer).",
                     "Set a factor first (e.g. 2x longer, 0.5x shorter).");
            return;
        }
        work = timeStretch (work, f);
        stretchSlider.setValue (1.0, juce::dontSendNotification);
        hasSel = false;
        setHint ("Time-Stretch angewandt (Tonhoehe bleibt).",
                 "Time-stretch applied (pitch unchanged).");
        repaint();
    };

    hintLabel.setFont (rt::mono (12.0f, false));
    hintLabel.setColour (juce::Label::textColourId, rt::textDim);
    hintLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (hintLabel);

    refresh();
    applyLanguage();
}

void SampleEditPanel::applyLanguage()
{
    titleLabel.setText (loc::t ("FAIRLIGHT - SAMPLE-WERKZEUG", "FAIRLIGHT - SAMPLE TOOL"),
                        juce::dontSendNotification);
    waveButton.setButtonText   (loc::t ("WELLE", "WAVE"));
    waveButton.setTooltip (loc::t ("Eine Single-Cycle-Welle erzeugen (Sinus/Saege/Rechteck/Dreieck/Puls) zum Weiterbearbeiten",
                                   "Generate a single-cycle wave (sine/saw/square/triangle/pulse) to edit further"));
    trimButton.setButtonText   (loc::t ("TRIMMEN", "TRIM"));
    trimButton.setTooltip (loc::t ("Auf die Markierung zuschneiden (Rest verwerfen)",
                                   "Crop to the selection (discard the rest)"));
    cutButton.setButtonText    (loc::t ("AUSSCHNEIDEN", "CUT"));
    cutButton.setTooltip (loc::t ("Den markierten Bereich herausschneiden (Rest zusammenfuegen)",
                                  "Cut out the selected range (join the rest)"));
    normButton.setButtonText   (loc::t ("NORMAL.", "NORMAL."));
    normButton.setTooltip (loc::t ("Auf Vollpegel anheben", "Boost to full level"));
    revButton.setButtonText    (loc::t ("UMKEHREN", "REVERSE"));
    drawButton.setButtonText   (loc::t ("FREIHAND", "FREEHAND"));
    drawButton.setTooltip (loc::t ("Wellenform mit der Maus zeichnen - das Lichtgriffel-Gefuehl des Fairlight",
                                   "Draw the waveform with the mouse - the Fairlight light-pen feel"));
    chopButton.setButtonText   (loc::t ("IN KIT (16)", "TO KIT (16)"));
    chopButton.setTooltip (loc::t ("Sample in 16 gleiche Scheiben schneiden und auf die Drum-Pads legen",
                                   "Slice the sample into 16 equal pieces onto the drum pads"));
    chopPatButton.setButtonText (loc::t ("-> PATTERN", "-> PATTERN"));
    chopPatButton.setTooltip (loc::t ("Sample in 16 Scheiben schneiden, in Slots legen UND als Noten ins Pattern (Break wird spielbar/umbaubar)",
                                      "Slice into 16, put into slots AND as notes in the pattern (break becomes playable/rearrangeable)"));
    loopButton.setButtonText (loc::t ("LOOP", "LOOP"));
    loopButton.setTooltip (loc::t ("One-Shot oder Loop: legt fest, ob das Sample einmal spielt oder in der Schleife laeuft",
                                   "One-shot or loop: whether the sample plays once or loops"));
    exportButton.setButtonText (loc::t ("SPEICHERN", "SAVE"));
    exportButton.setTooltip (loc::t ("Das (bearbeitete) Sample als WAV-Datei auf die Platte speichern",
                                     "Save the (edited) sample as a WAV file to disk"));
    previewButton.setButtonText (loc::t ("VORHOEREN", "PREVIEW"));
    applyButton.setButtonText  (loc::t ("UEBERNEHMEN", "APPLY"));
    applyButton.setTooltip (loc::t ("Bearbeitetes Sample zurueck in den Slot legen (bleibt im Song)",
                                    "Put the edited sample back into the slot (kept with the song)"));
    closeButton.setButtonText  (loc::t ("SCHLIESSEN", "CLOSE"));
    stretchLabel.setText (loc::t ("TIME-STRETCH", "TIME-STRETCH"), juce::dontSendNotification);
    stretchButton.setButtonText (loc::t ("DEHNEN", "STRETCH"));
    stretchButton.setTooltip (loc::t ("Sample laenger/kuerzer machen OHNE die Tonhoehe zu aendern",
                                      "Make the sample longer/shorter WITHOUT changing the pitch"));
    setHint ("Bereich ziehen zum Markieren. FREIHAND zeichnet die Welle. IN KIT schneidet auf die Pads.",
             "Drag to select. FREEHAND draws the wave. TO KIT slices onto the pads.");
}

void SampleEditPanel::refresh()
{
    slot = proc.currentInstrument.load();
    if (! proc.getSampleCopy (slot, work, rate))
    {
        // Kein Sample im Slot -> leerer Puffer zum Zeichnen (Fairlight von Null).
        work.setSize (1, 8000);
        work.clear();
        rate = 8363.0;
        setHint ("Slot leer - mit FREIHAND eine Welle zeichnen oder ein Sample laden.",
                 "Slot empty - draw a wave with FREEHAND or load a sample.");
    }
    hasSel = false;
    selStart = selEnd = 0.0;
    repaint();
}

void SampleEditPanel::cutSelection()
{
    const int len = work.getNumSamples();
    if (! hasSel || len < 4)
    {
        setHint ("Erst einen Bereich markieren (ziehen).", "Select a range first (drag).");
        return;
    }
    const int a = juce::jlimit (0, len - 1, (int) (juce::jmin (selStart, selEnd) * len));
    const int b = juce::jlimit (a + 1, len, (int) (juce::jmax (selStart, selEnd) * len));
    const int rest = len - (b - a);
    if (rest < 2) { setHint ("Da bliebe nichts uebrig.", "Nothing would be left."); return; }

    juce::AudioBuffer<float> out (work.getNumChannels(), rest);
    for (int c = 0; c < work.getNumChannels(); ++c)
    {
        out.copyFrom (c, 0,    work, c, 0, a);          // vor der Auswahl
        out.copyFrom (c, a,    work, c, b, len - b);    // nach der Auswahl
    }
    work = std::move (out);
    hasSel = false;
    setHint ("Auswahl herausgeschnitten.", "Selection cut out.");
    repaint();
}

void SampleEditPanel::fillWave (int type)
{
    const int len = 600; // eine Schwingung
    work.setSize (1, len);
    auto* d = work.getWritePointer (0);
    const double twoPi = 2.0 * juce::MathConstants<double>::pi;
    for (int i = 0; i < len; ++i)
    {
        const double ph = (double) i / (double) len;
        float v = 0.0f;
        switch (type)
        {
            case 1:  v = (float) (2.0 * ph - 1.0); break;                   // Saege
            case 2:  v = ph < 0.5 ? 1.0f : -1.0f; break;                   // Rechteck
            case 3:  v = (float) (1.0 - 4.0 * std::abs (ph - 0.5)); break; // Dreieck
            case 4:  v = ph < 0.25 ? 1.0f : -1.0f; break;                  // Puls 25%
            case 0:
            default: v = (float) std::sin (twoPi * ph); break;             // Sinus
        }
        d[i] = v * 0.9f;
    }
    rate = 261.63 * len; // Note 60 (C-5) ~ 261,6 Hz
    loopOn = true;
    loopButton.setToggleState (true, juce::dontSendNotification);
    hasSel = false;
    setHint ("Wellenform erzeugt - mit FREIHAND verformen, LOOP an = Oszillator.",
             "Waveform generated - reshape with FREEHAND, LOOP on = oscillator.");
    repaint();
}

void SampleEditPanel::timerCallback()
{
    // Laufmarke nur neu zeichnen, wenn die Vorschau gerade laeuft.
    if (proc.engine.previewPos() >= 0.0)
        repaint (waveRect);
}

void SampleEditPanel::setHint (const juce::String& de, const juce::String& en)
{
    hintLabel.setText (loc::t (de, en), juce::dontSendNotification);
}

int SampleEditPanel::xToIndex (int x) const
{
    if (waveRect.getWidth() <= 0)
        return 0;
    const double f = (double) (x - waveRect.getX()) / (double) waveRect.getWidth();
    return juce::jlimit (0, work.getNumSamples() - 1,
                         (int) (juce::jlimit (0.0, 1.0, f) * work.getNumSamples()));
}

float SampleEditPanel::yToValue (int y) const
{
    const float mid = (float) waveRect.getCentreY();
    const float half = (float) waveRect.getHeight() * 0.5f;
    if (half <= 0.0f) return 0.0f;
    return juce::jlimit (-1.0f, 1.0f, (mid - (float) y) / half);
}

void SampleEditPanel::selectAt (const juce::MouseEvent& e)
{
    if (! waveRect.contains (e.getPosition()))
        return;
    const double f = (double) (e.x - waveRect.getX()) / (double) juce::jmax (1, waveRect.getWidth());
    selEnd = juce::jlimit (0.0, 1.0, f);
    hasSel = std::abs (selEnd - selStart) > 0.002;
    repaint();
}

void SampleEditPanel::drawAt (const juce::MouseEvent& e)
{
    if (work.getNumSamples() < 2)
        return;
    const int idx = xToIndex (e.x);
    const float val = yToValue (e.y);
    // Von der letzten Position bis hier linear fuellen -> durchgehende Linie.
    int from = (lastDrawIdx < 0) ? idx : lastDrawIdx;
    int to   = idx;
    if (from > to) std::swap (from, to);
    for (int c = 0; c < work.getNumChannels(); ++c)
    {
        auto* d = work.getWritePointer (c);
        for (int i = from; i <= to; ++i)
            d[i] = val;
    }
    lastDrawIdx = idx;
    repaint (waveRect);
}

void SampleEditPanel::mouseDown (const juce::MouseEvent& e)
{
    if (! waveRect.contains (e.getPosition()))
        return;
    if (freehand)
    {
        lastDrawIdx = -1;
        drawAt (e);
    }
    else
    {
        const double f = (double) (e.x - waveRect.getX()) / (double) juce::jmax (1, waveRect.getWidth());
        selStart = juce::jlimit (0.0, 1.0, f);
        selEnd = selStart;
        hasSel = false;
        repaint();
    }
}

void SampleEditPanel::mouseDrag (const juce::MouseEvent& e)
{
    if (freehand) drawAt (e);
    else          selectAt (e);
}

bool SampleEditPanel::keyPressed (const juce::KeyPress& key)
{
    if (key.getKeyCode() == juce::KeyPress::escapeKey && onClose != nullptr)
    {
        onClose();
        return true;
    }
    return false;
}

void SampleEditPanel::paint (juce::Graphics& g)
{
    // Fairlight-CMI-Optik: leuchtendes Phosphor-Gruen auf fast schwarzem Grund
    // (Page-R-/Vektor-Monitor-Look).
    const juce::Colour crtBg    { 0xff03130a }; // sehr dunkles Gruen-Schwarz
    const juce::Colour phosphor { 0xff39ff7a }; // helles Phosphor-Gruen
    const juce::Colour phosDim   = phosphor.withAlpha (0.30f);
    const juce::Colour phosFaint = phosphor.withAlpha (0.10f);

    g.fillAll (rt::bg);
    g.setColour (phosphor.withAlpha (0.6f));
    g.drawRect (getLocalBounds(), 1);

    // Wellenform-Feld = der gruene CRT.
    g.setColour (crtBg);
    g.fillRect (waveRect);

    const int len = work.getNumSamples();
    if (len > 1 && waveRect.getWidth() > 2)
    {
        // Dezentes CRT-Raster (gruene Linien).
        for (int gx = 1; gx < 8; ++gx)
        {
            const int x = waveRect.getX() + waveRect.getWidth() * gx / 8;
            g.setColour (phosFaint);
            g.drawVerticalLine (x, (float) waveRect.getY(), (float) waveRect.getBottom());
        }
        for (int gy = 1; gy < 4; ++gy)
        {
            const int y = waveRect.getY() + waveRect.getHeight() * gy / 4;
            g.setColour (phosFaint);
            g.drawHorizontalLine (y, (float) waveRect.getX(), (float) waveRect.getRight());
        }

        // Auswahl markieren (heller gruener Schleier).
        if (hasSel)
        {
            const int xa = waveRect.getX() + (int) (juce::jmin (selStart, selEnd) * waveRect.getWidth());
            const int xb = waveRect.getX() + (int) (juce::jmax (selStart, selEnd) * waveRect.getWidth());
            g.setColour (phosphor.withAlpha (0.22f));
            g.fillRect (xa, waveRect.getY(), juce::jmax (1, xb - xa), waveRect.getHeight());
        }

        // Mittellinie.
        const int mid = waveRect.getCentreY();
        g.setColour (phosDim);
        g.drawHorizontalLine (mid, (float) waveRect.getX(), (float) waveRect.getRight());

        // Wellenform als Min/Max pro Pixelspalte - leuchtendes Gruen.
        const float half = waveRect.getHeight() * 0.5f;
        const auto* d = work.getReadPointer (0);
        g.setColour (phosphor);
        for (int x = 0; x < waveRect.getWidth(); ++x)
        {
            const int i0 = (int) ((double) x       / waveRect.getWidth() * len);
            const int i1 = (int) ((double) (x + 1) / waveRect.getWidth() * len);
            float lo = 0.0f, hi = 0.0f;
            for (int i = i0; i < juce::jmax (i0 + 1, i1) && i < len; ++i)
            {
                lo = juce::jmin (lo, d[i]);
                hi = juce::jmax (hi, d[i]);
            }
            const int yTop = mid - (int) (hi * half);
            const int yBot = mid - (int) (lo * half);
            g.drawVerticalLine (waveRect.getX() + x, (float) juce::jmin (yTop, yBot),
                                (float) juce::jmax (yTop, yBot) + 1.0f);
        }
    }

    // Laufmarke: zeigt beim Vorhoeren, wo im Sample gerade gespielt wird.
    {
        const double pp = proc.engine.previewPos();
        const int wl = work.getNumSamples();
        if (pp >= 0.0 && wl > 1)
        {
            const int x = waveRect.getX()
                        + (int) (juce::jlimit (0.0, 1.0, pp / (double) wl) * waveRect.getWidth());
            g.setColour (juce::Colour (0xffffe080)); // helle Laufmarke
            g.drawVerticalLine (x, (float) waveRect.getY(), (float) waveRect.getBottom());
        }
    }

    // Gruener CRT-Rahmen ueber allem.
    g.setColour (phosphor.withAlpha (0.5f));
    g.drawRect (waveRect, 1);
}

void SampleEditPanel::resized()
{
    auto area = getLocalBounds().reduced (14);

    titleLabel.setBounds (area.removeFromTop (26));
    area.removeFromTop (8);

    // Zwei aufgeraeumte Knopfreihen unten. Unterste: Schneiden/Abspielen + Schliessen.
    auto rowB = area.removeFromBottom (32);
    {
        closeButton.setBounds (rowB.removeFromRight (120));
        rowB.removeFromRight (8);
        const int n = 4; // chop, chopPat, preview, apply
        const int bw = (rowB.getWidth() - (n - 1) * 6) / n;
        for (auto* b : { &chopButton, &chopPatButton, &previewButton, &applyButton })
        {
            b->setBounds (rowB.removeFromLeft (bw));
            rowB.removeFromLeft (6);
        }
    }
    area.removeFromBottom (6);
    // Darueber: Bearbeiten-Werkzeuge.
    auto rowA = area.removeFromBottom (30);
    {
        const int n = 8; // wave, trim, cut, norm, rev, draw, loop, export
        const int bw = (rowA.getWidth() - (n - 1) * 6) / n;
        for (auto* b : { &waveButton, &trimButton, &cutButton, &normButton, &revButton,
                         &drawButton, &loopButton, &exportButton })
        {
            b->setBounds (rowA.removeFromLeft (bw));
            rowA.removeFromLeft (6);
        }
    }
    area.removeFromBottom (6);
    hintLabel.setBounds (area.removeFromBottom (20));
    area.removeFromBottom (6);

    // Time-Stretch-Reihe.
    {
        auto row = area.removeFromBottom (26);
        stretchLabel.setBounds (row.removeFromLeft (110));
        row.removeFromLeft (8);
        stretchButton.setBounds (row.removeFromRight (110));
        row.removeFromRight (8);
        stretchSlider.setBounds (row);
    }
    area.removeFromBottom (8);

    waveRect = area; // der Rest ist das Wellenform-Feld
}
