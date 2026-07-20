#include "SpeechPanel.h"

SpeechPanel::SpeechPanel (RetroTraxProcessor& processor) : proc (processor)
{
    setWantsKeyboardFocus (true);

    titleLabel.setFont (rt::mono (16.0f, true));
    titleLabel.setColour (juce::Label::textColourId, rt::cursor);
    addAndMakeVisible (titleLabel);

    slotLabel.setFont (rt::mono (13.0f, true));
    slotLabel.setColour (juce::Label::textColourId, rt::text);
    addAndMakeVisible (slotLabel);

    charLabel.setFont (rt::mono (12.0f, true));
    charLabel.setColour (juce::Label::textColourId, rt::textDim);
    addAndMakeVisible (charLabel);
    addAndMakeVisible (charSam);
    addAndMakeVisible (charNarrator);
    charSam.onClick      = [this] { selectCharacter (0); };
    charNarrator.onClick = [this] { selectCharacter (1); };

    textLabel.setFont (rt::mono (12.0f, true));
    textLabel.setColour (juce::Label::textColourId, rt::textDim);
    addAndMakeVisible (textLabel);

    textBox.setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xff262c38));
    textBox.setColour (juce::TextEditor::textColourId, rt::text);
    textBox.setColour (juce::TextEditor::outlineColourId, rt::steel.withAlpha (0.5f));
    textBox.setColour (juce::TextEditor::focusedOutlineColourId, rt::cursor);
    textBox.setMultiLine (false);
    textBox.setReturnKeyStartsNewLine (false);
    textBox.onReturnKey = [this] { writeParams(); previewNote(); };
    textBox.onFocusLost = [this] { writeParams(); };
    addAndMakeVisible (textBox);

    auto setupSlider = [this] (juce::Slider& s, juce::Label& lab, double lo, double hi,
                               double interval, const juce::String& suffix, int decimals)
    {
        s.setSliderStyle (juce::Slider::LinearBar);
        s.setRange (lo, hi, interval);
        s.setTextValueSuffix (suffix);
        s.setNumDecimalPlacesToDisplay (decimals);
        // Wirkt sofort (neu gerendert bei jeder Aenderung - Saetze sind kurz genug,
        // dass das nicht spuerbar dauert), TEST spielt das Ergebnis hoerbar ab.
        s.onValueChange = [this] { writeParams(); };
        s.onDragEnd     = [this] { previewNote(); };
        addAndMakeVisible (s);

        lab.setFont (rt::mono (12.0f, true));
        lab.setColour (juce::Label::textColourId, rt::textDim);
        addAndMakeVisible (lab);
    };
    setupSlider (speedSlider,  speedLabel,  50.0, 200.0, 1.0, " %",  0);
    setupSlider (pitchSlider,  pitchLabel,  60.0, 260.0, 1.0, " Hz", 0);
    setupSlider (throatSlider, throatLabel, -100.0, 100.0, 1.0, " %", 0);
    setupSlider (mouthSlider,  mouthLabel,  0.0, 100.0, 1.0, " %",  0);

    hintLabel.setFont (rt::mono (12.0f));
    hintLabel.setColour (juce::Label::textColourId, rt::textDim);
    hintLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (hintLabel);

    speakButton.onClick = [this] { writeParams(); previewNote(); };
    addAndMakeVisible (speakButton);

    testButton.onClick = [this] { previewNote(); };
    addAndMakeVisible (testButton);

    closeButton.onClick = [this] { if (onClose) onClose(); };
    addAndMakeVisible (closeButton);

    applyLanguage();
}

void SpeechPanel::applyLanguage()
{
    titleLabel.setText (loc::t ("SPRACH-SYNTHESE", "SPEECH SYNTHESIS"), juce::dontSendNotification);
    charLabel.setText (loc::t ("CHARAKTER", "CHARACTER"), juce::dontSendNotification);
    charSam.setTooltip (loc::t ("Schrill/blechern - C64-SAM-Vibe",
                                "Shrill/tinny - C64 SAM vibe"));
    charNarrator.setTooltip (loc::t ("Weicher/dumpfer - Amiga-Narrator-Vibe",
                                     "Softer/duller - Amiga narrator vibe"));

    textLabel.setText (loc::t ("TEXT (ENGLISCH KLINGT AM BESTEN)", "TEXT (ENGLISH SOUNDS BEST)"),
                       juce::dontSendNotification);
    textBox.setTextToShowWhenEmpty (loc::t ("z.B. Hello world", "e.g. Hello world"), rt::textDim);

    speedLabel.setText  (loc::t ("SPRECHTEMPO", "SPEECH SPEED"), juce::dontSendNotification);
    pitchLabel.setText  (loc::t ("TONHOEHE", "PITCH"), juce::dontSendNotification);
    throatLabel.setText (loc::t ("STIMMLAGE (HALS)", "THROAT"), juce::dontSendNotification);
    mouthLabel.setText  (loc::t ("MUNDOEFFNUNG", "MOUTH"), juce::dontSendNotification);

    hintLabel.setText (loc::t (
        "Text tippen + ENTER oder SPRECHEN. Ergebnis ist ein normales Sample - FX-Knopf filtert mit.",
        "Type text + ENTER or SPEAK. Result is a normal sample - the FX button filters it too."),
        juce::dontSendNotification);
    speakButton.setButtonText (loc::t ("SPRECHEN", "SPEAK"));
    speakButton.setTooltip (loc::t ("Text neu rendern und anspielen", "Re-render the text and play it"));
    testButton.setButtonText (loc::t ("TEST", "TEST"));
    testButton.setTooltip (loc::t ("Aktuelles Ergebnis nochmal anspielen", "Play the current result again"));
    closeButton.setButtonText (loc::t ("SCHLIESSEN", "CLOSE"));
}

void SpeechPanel::refresh()
{
    slot = proc.currentInstrument.load();
    slotLabel.setText (loc::t ("Slot ", "Slot ") + juce::String::formatted ("%02d", slot + 1),
                       juce::dontSendNotification);

    TrackerEngine::Instrument s;
    if (! proc.getSample (slot, s))
        return; // Slot ist (noch) kein Sprach-Instrument

    loading = true; // Regler setzen, ohne dass die Callbacks zurueckschreiben
    textBox.setText (s.speechText, juce::dontSendNotification);
    speedSlider.setValue  (s.speechSpeed * 100.0, juce::dontSendNotification);
    pitchSlider.setValue  (s.speechPitch, juce::dontSendNotification);
    throatSlider.setValue (s.speechThroat * 100.0, juce::dontSendNotification);
    mouthSlider.setValue  (s.speechMouth * 100.0, juce::dontSendNotification);
    loading = false;

    updateCharButtons();
}

void SpeechPanel::updateCharButtons()
{
    TrackerEngine::Instrument s;
    const int character = proc.getSample (slot, s) ? s.speechCharacter : 0;
    charSam.setToggleState      (character == 0, juce::dontSendNotification);
    charNarrator.setToggleState (character == 1, juce::dontSendNotification);
}

void SpeechPanel::selectCharacter (int character)
{
    proc.editSample (slot, [character] (TrackerEngine::Instrument& i)
    {
        i.speechCharacter = character;
    });
    updateCharButtons();
    proc.renderSpeech (slot);
    if (onChanged) onChanged();
    previewNote();
}

void SpeechPanel::writeParams()
{
    if (loading || ! proc.isSampleSlot (slot))
        return;

    const auto text   = textBox.getText();
    const float speed  = (float) speedSlider.getValue()  / 100.0f;
    const float pitch  = (float) pitchSlider.getValue();
    const float throat = (float) throatSlider.getValue() / 100.0f;
    const float mouth  = (float) mouthSlider.getValue()  / 100.0f;

    proc.editSample (slot, [&] (TrackerEngine::Instrument& i)
    {
        i.speechText   = text;
        i.speechSpeed  = speed;
        i.speechPitch  = pitch;
        i.speechThroat = throat;
        i.speechMouth  = mouth;
    });
    proc.renderSpeech (slot);
    if (onChanged) onChanged();
}

void SpeechPanel::previewNote()
{
    if (! proc.isSampleSlot (slot))
        return;
    proc.engine.audition (60, slot); // C-5, einmal durchspielen
}

bool SpeechPanel::keyPressed (const juce::KeyPress& key)
{
    if (key.getKeyCode() == juce::KeyPress::escapeKey && onClose != nullptr)
    {
        onClose();
        return true;
    }
    return false;
}

void SpeechPanel::paint (juce::Graphics& g)
{
    g.fillAll (rt::bg);
    g.setColour (rt::steel.withAlpha (0.7f));
    g.drawRect (getLocalBounds(), 1);
}

void SpeechPanel::resized()
{
    auto area = getLocalBounds().reduced (14);

    auto top = area.removeFromTop (26);
    titleLabel.setBounds (top.removeFromLeft (240));
    slotLabel.setBounds  (top.removeFromLeft (120));
    area.removeFromTop (10);

    charLabel.setBounds (area.removeFromTop (16));
    {
        auto row = area.removeFromTop (30);
        const int bw = (row.getWidth() - 6) / 2;
        charSam.setBounds (row.removeFromLeft (bw));
        row.removeFromLeft (6);
        charNarrator.setBounds (row);
    }
    area.removeFromTop (12);

    textLabel.setBounds (area.removeFromTop (16));
    textBox.setBounds (area.removeFromTop (32));
    area.removeFromTop (12);

    auto sliderRow = [] (juce::Rectangle<int>& col, juce::Label& lab, juce::Slider& s)
    {
        auto row = col.removeFromTop (26);
        lab.setBounds (row.removeFromLeft (juce::jmin (160, row.getWidth() / 2)));
        row.removeFromLeft (6);
        s.setBounds (row);
        col.removeFromTop (8);
    };
    sliderRow (area, speedLabel,  speedSlider);
    sliderRow (area, pitchLabel,  pitchSlider);
    sliderRow (area, throatLabel, throatSlider);
    sliderRow (area, mouthLabel,  mouthSlider);

    auto bottom = getLocalBounds().reduced (14).removeFromBottom (30);
    closeButton.setBounds (bottom.removeFromRight (120).reduced (0, 2));
    bottom.removeFromRight (8);
    testButton.setBounds (bottom.removeFromRight (90).reduced (0, 2));
    bottom.removeFromRight (8);
    speakButton.setBounds (bottom.removeFromRight (110).reduced (0, 2));
    bottom.removeFromRight (12);
    hintLabel.setBounds (bottom);
}
