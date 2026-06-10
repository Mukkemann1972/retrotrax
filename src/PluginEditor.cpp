#include "PluginEditor.h"

RetroTraxEditor::RetroTraxEditor (RetroTraxProcessor& p)
    : AudioProcessorEditor (&p), proc (p), grid (p)
{
    setLookAndFeel (&lnf);

    addAndMakeVisible (grid);
    addAndMakeVisible (playButton);
    addAndMakeVisible (stopButton);
    addAndMakeVisible (loadButton);
    addAndMakeVisible (bpmSlider);
    addAndMakeVisible (instrumentBox);
    addAndMakeVisible (octaveBox);
    addAndMakeVisible (instLabel);
    addAndMakeVisible (octLabel);
    addAndMakeVisible (hintLabel);

    playButton.onClick = [this] { proc.engine.play(); updateTransportButtons(); };
    stopButton.onClick = [this] { proc.engine.stop(); updateTransportButtons(); };
    loadButton.onClick = [this] { loadSampleClicked(); };

    bpmSlider.setSliderStyle (juce::Slider::LinearBar);
    bpmSlider.setRange (60.0, 220.0, 1.0);
    bpmSlider.setValue ((double) proc.engine.bpm.load(), juce::dontSendNotification);
    bpmSlider.setTextValueSuffix (" BPM");
    bpmSlider.onValueChange = [this] { proc.engine.bpm = (float) bpmSlider.getValue(); };

    for (int i = 1; i <= TrackerEngine::kInstruments; ++i)
        instrumentBox.addItem (juce::String::formatted ("%02d (leer)", i), i);
    instrumentBox.setSelectedId (proc.currentInstrument.load() + 1, juce::dontSendNotification);
    instrumentBox.onChange = [this]
    {
        proc.currentInstrument = juce::jlimit (0, TrackerEngine::kInstruments - 1,
                                               instrumentBox.getSelectedId() - 1);
    };
    refreshInstrumentBox();

    for (int o = 1; o <= 8; ++o)
        octaveBox.addItem (juce::String (o), o);
    octaveBox.setSelectedId (proc.currentOctave.load(), juce::dontSendNotification);
    octaveBox.onChange = [this] { proc.currentOctave = octaveBox.getSelectedId(); };

    instLabel.setFont (rt::mono (12.0f, true));
    octLabel.setFont (rt::mono (12.0f, true));
    instLabel.setColour (juce::Label::textColourId, rt::textDim);
    octLabel.setColour (juce::Label::textColourId, rt::textDim);

    hintLabel.setFont (rt::mono (13.0f));
    hintLabel.setColour (juce::Label::textColourId, rt::textDim);
    hintLabel.setJustificationType (juce::Justification::centred);
    hintLabel.setText ("Noten: YXCVBNM (+ SDGHJ = Halbtoene) · Q2W3ER... = Oktave hoeher · Pfeile = Cursor · "
                       "Tab = Spur · Leertaste = Play/Stop · Entf = Loeschen · +/- = Oktave",
                       juce::dontSendNotification);

    grid.onTransportChange = [this] { updateTransportButtons(); };
    updateTransportButtons();

    setResizable (true, true);
    setResizeLimits (840, 520, 1920, 1200);
    setSize (1000, 640);

    juce::MessageManager::callAsync ([sp = juce::Component::SafePointer<PatternGrid> (&grid)]
    {
        if (sp != nullptr)
            sp->grabKeyboardFocus();
    });
}

RetroTraxEditor::~RetroTraxEditor()
{
    setLookAndFeel (nullptr);
}

void RetroTraxEditor::updateTransportButtons()
{
    const bool playing = proc.engine.playing.load();
    playButton.setToggleState (playing, juce::dontSendNotification);
    stopButton.setToggleState (! playing, juce::dontSendNotification);
}

void RetroTraxEditor::refreshInstrumentBox()
{
    for (int i = 0; i < TrackerEngine::kInstruments; ++i)
    {
        auto name = proc.engine.getInstrumentName (i);
        instrumentBox.changeItemText (i + 1,
            juce::String::formatted ("%02d ", i + 1) + (name.isEmpty() ? "(leer)" : name));
    }
}

void RetroTraxEditor::loadSampleClicked()
{
    chooser = std::make_unique<juce::FileChooser> (
        "Sample auswaehlen (WAV, AIFF, FLAC, OGG, MP3)",
        juce::File::getSpecialLocation (juce::File::userMusicDirectory),
        "*.wav;*.aif;*.aiff;*.flac;*.ogg;*.mp3");

    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [this] (const juce::FileChooser& fc)
        {
            const auto file = fc.getResult();
            if (file == juce::File())
                return;

            const int slot = proc.currentInstrument.load();
            if (proc.loadInstrument (slot, file))
            {
                refreshInstrumentBox();
                hintLabel.setText ("Sample \"" + file.getFileName() + "\" in Slot "
                                       + juce::String::formatted ("%02d", slot + 1)
                                       + " geladen. Klick ins Grid und tippe Noten!",
                                   juce::dontSendNotification);
            }
            else
            {
                hintLabel.setText ("Konnte \"" + file.getFileName() + "\" nicht laden.",
                                   juce::dontSendNotification);
            }
        });
}

void RetroTraxEditor::paint (juce::Graphics& g)
{
    g.fillAll (rt::bg);

    auto header = juce::Rectangle<int> (0, 0, getWidth(), 54);
    g.setGradientFill (juce::ColourGradient (rt::panel.brighter (0.15f), 0.0f, 0.0f,
                                             rt::panel.darker (0.2f), 0.0f, (float) header.getBottom(), false));
    g.fillRect (header);
    g.setColour (rt::steel.withAlpha (0.7f));
    g.drawHorizontalLine (header.getBottom(), 0.0f, (float) getWidth());

    g.setFont (rt::mono (24.0f, true));
    g.setColour (juce::Colour (0xff0e1118));
    g.drawText ("MUKKEMANN RETROTRAX", 17, 1, 420, header.getHeight(), juce::Justification::centredLeft);
    g.setColour (rt::cursor);
    g.drawText ("MUKKEMANN RETROTRAX", 16, 0, 420, header.getHeight(), juce::Justification::centredLeft);

    g.setFont (rt::mono (13.0f));
    g.setColour (rt::text);
    g.drawText ("v0.1 · Etappe 1: Sampler · SID kommt!", getWidth() - 320, 0, 304, header.getHeight(),
                juce::Justification::centredRight);
}

void RetroTraxEditor::resized()
{
    auto area = getLocalBounds();
    area.removeFromTop (54); // Titelzeile (nur paint)

    auto controls = area.removeFromTop (40).reduced (8, 5);
    playButton.setBounds (controls.removeFromLeft (74));
    controls.removeFromLeft (6);
    stopButton.setBounds (controls.removeFromLeft (74));
    controls.removeFromLeft (14);
    bpmSlider.setBounds (controls.removeFromLeft (150));
    controls.removeFromLeft (14);
    octLabel.setBounds (controls.removeFromLeft (52));
    octaveBox.setBounds (controls.removeFromLeft (58));
    controls.removeFromLeft (14);
    instLabel.setBounds (controls.removeFromLeft (46));
    instrumentBox.setBounds (controls.removeFromLeft (190));
    controls.removeFromLeft (10);
    loadButton.setBounds (controls.removeFromLeft (130));

    hintLabel.setBounds (area.removeFromBottom (26));
    grid.setBounds (area.reduced (8, 4));
}
