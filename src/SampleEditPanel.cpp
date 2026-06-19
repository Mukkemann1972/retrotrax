#include "SampleEditPanel.h"

SampleEditPanel::SampleEditPanel (RetroTraxProcessor& processor) : proc (processor)
{
    setWantsKeyboardFocus (true);

    titleLabel.setFont (rt::mono (16.0f, true));
    titleLabel.setColour (juce::Label::textColourId, juce::Colour (0xff39ff7a)); // Fairlight-Gruen
    addAndMakeVisible (titleLabel);

    for (auto* b : { &trimButton, &normButton, &revButton, &drawButton,
                     &chopButton, &previewButton, &applyButton, &closeButton })
        addAndMakeVisible (b);

    drawButton.setClickingTogglesState (true);

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

    previewButton.onClick = [this] { proc.previewBuffer (work, rate); };

    applyButton.onClick = [this]
    {
        juce::String msg;
        proc.applyEditedSample (slot, work, rate, msg);
        setHint (msg, msg);
    };

    closeButton.onClick = [this] { if (onClose) onClose(); };

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
    trimButton.setButtonText   (loc::t ("TRIMMEN", "TRIM"));
    normButton.setButtonText   (loc::t ("NORMAL.", "NORMAL."));
    normButton.setTooltip (loc::t ("Auf Vollpegel anheben", "Boost to full level"));
    revButton.setButtonText    (loc::t ("UMKEHREN", "REVERSE"));
    drawButton.setButtonText   (loc::t ("FREIHAND", "FREEHAND"));
    drawButton.setTooltip (loc::t ("Wellenform mit der Maus zeichnen - das Lichtgriffel-Gefuehl des Fairlight",
                                   "Draw the waveform with the mouse - the Fairlight light-pen feel"));
    chopButton.setButtonText   (loc::t ("IN KIT (16)", "TO KIT (16)"));
    chopButton.setTooltip (loc::t ("Sample in 16 gleiche Scheiben schneiden und auf die Drum-Pads legen",
                                   "Slice the sample into 16 equal pieces onto the drum pads"));
    previewButton.setButtonText (loc::t ("VORHOEREN", "PREVIEW"));
    applyButton.setButtonText  (loc::t ("UEBERNEHMEN", "APPLY"));
    applyButton.setTooltip (loc::t ("Bearbeitetes Sample zurueck in den Slot legen (bleibt im Song)",
                                    "Put the edited sample back into the slot (kept with the song)"));
    closeButton.setButtonText  (loc::t ("SCHLIESSEN", "CLOSE"));
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

    // Gruener CRT-Rahmen ueber allem.
    g.setColour (phosphor.withAlpha (0.5f));
    g.drawRect (waveRect, 1);
}

void SampleEditPanel::resized()
{
    auto area = getLocalBounds().reduced (14);

    titleLabel.setBounds (area.removeFromTop (26));
    area.removeFromTop (8);

    // Knopfreihe unten + Hinweiszeile darueber.
    auto bottom = area.removeFromBottom (34);
    {
        closeButton.setBounds (bottom.removeFromRight (130));
        bottom.removeFromRight (10);
        const int n = 7; // trim, norm, rev, draw, chop, preview, apply
        const int bw = (bottom.getWidth() - (n - 1) * 6) / n;
        for (auto* b : { &trimButton, &normButton, &revButton, &drawButton,
                         &chopButton, &previewButton, &applyButton })
        {
            b->setBounds (bottom.removeFromLeft (bw));
            bottom.removeFromLeft (6);
        }
    }
    area.removeFromBottom (6);
    hintLabel.setBounds (area.removeFromBottom (20));
    area.removeFromBottom (8);

    waveRect = area; // der Rest ist das Wellenform-Feld
}
