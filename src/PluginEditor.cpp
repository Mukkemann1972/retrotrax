#include "PluginEditor.h"

RetroTraxEditor::RetroTraxEditor (RetroTraxProcessor& p)
    : AudioProcessorEditor (&p), proc (p), grid (p), diskBrowser (p)
{
    setLookAndFeel (&lnf);

    addAndMakeVisible (grid);
    addChildComponent (diskBrowser); // unsichtbar, bis SAMPLES gedrueckt wird
    addAndMakeVisible (playButton);
    addAndMakeVisible (stopButton);
    addAndMakeVisible (loadButton);
    addAndMakeVisible (stDisksButton);
    addAndMakeVisible (saveSongButton);
    addAndMakeVisible (loadSongButton);
    addAndMakeVisible (bpmSlider);
    addAndMakeVisible (instrumentBox);
    addAndMakeVisible (octaveBox);
    addAndMakeVisible (instLabel);
    addAndMakeVisible (octLabel);
    addAndMakeVisible (hintLabel);
    addAndMakeVisible (instDot);
    instDot.colour = rt::instColour (proc.currentInstrument.load());

    playButton.onClick = [this] { proc.engine.play(); updateTransportButtons(); };
    stopButton.onClick = [this] { proc.engine.stop(); updateTransportButtons(); };
    loadButton.onClick = [this] { loadSampleClicked(); };
    saveSongButton.onClick = [this] { saveSongClicked(); };
    loadSongButton.onClick = [this] { loadSongClicked(); };

    // Der Knopf OEFFNET nur — ein versehentlicher Doppelklick schliesst nichts.
    // Zu ist der Browser erst per SCHLIESSEN-Knopf oder ESC.
    stDisksButton.onClick = [this]
    {
        diskBrowser.setVisible (true);
        stDisksButton.setToggleState (true, juce::dontSendNotification);
        diskBrowser.toFront (false);
    };
    diskBrowser.onClose = [this]
    {
        diskBrowser.setVisible (false);
        stDisksButton.setToggleState (false, juce::dontSendNotification);
        grid.grabKeyboardFocus();
    };
    diskBrowser.onSampleLoaded = [this] (const juce::String& name, int slot)
    {
        refreshInstrumentBox();
        instDot.colour = rt::instColour (proc.currentInstrument.load());
        instDot.repaint();
        hintLabel.setText ("ST-Sample \"" + name + "\" in Slot "
                               + juce::String::formatted ("%02d", slot + 1)
                               + " geladen und angespielt.",
                           juce::dontSendNotification);
    };

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
        instDot.colour = rt::instColour (proc.currentInstrument.load());
        instDot.repaint();
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
    hintLabel.setText ("Noten: YXCVBNM (+ SDGHJ = Halbtoene) | Q2W3ER... = Oktave hoeher | Pfeile = Cursor | "
                       "Tab = Spur | Leertaste = Play/Stop | Entf = Loeschen | +/- = Oktave",
                       juce::dontSendNotification);

    grid.onTransportChange = [this] { updateTransportButtons(); };
    updateTransportButtons();

    setResizable (true, true);
    setResizeLimits (960, 520, 1920, 1200);
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

    // changeItemText aktualisiert die zugeklappte Box nicht — Text von Hand nachziehen,
    // sonst steht dort "(leer)", obwohl laengst ein Sample im Slot ist
    const int sel = instrumentBox.getSelectedId();
    if (sel > 0)
        instrumentBox.setText (instrumentBox.getItemText (sel - 1), juce::dontSendNotification);
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

juce::File RetroTraxEditor::songsFolder() const
{
    auto dir = juce::File::getSpecialLocation (juce::File::userMusicDirectory)
                   .getChildFile ("RetroTrax");
    dir.createDirectory();
    return dir;
}

void RetroTraxEditor::saveSongClicked()
{
    // Vorgeschlagener Name: der aktuelle Song, sonst ein frischer Standardname.
    const auto start = currentSongFile.existsAsFile()
                           ? currentSongFile
                           : songsFolder().getChildFile ("Mein Song.retrotrax");

    songChooser = std::make_unique<juce::FileChooser> (
        "Song speichern", start, "*.retrotrax");

    songChooser->launchAsync (juce::FileBrowserComponent::saveMode
                                  | juce::FileBrowserComponent::warnAboutOverwriting,
        [this] (const juce::FileChooser& fc)
        {
            auto file = fc.getResult();
            if (file == juce::File())
                return; // abgebrochen

            if (file.getFileExtension().isEmpty())
                file = file.withFileExtension ("retrotrax");

            if (proc.saveSong (file))
            {
                currentSongFile = file;
                hintLabel.setText ("Song \"" + file.getFileNameWithoutExtension()
                                       + "\" gespeichert.",
                                   juce::dontSendNotification);
            }
            else
            {
                hintLabel.setText ("Song konnte nicht gespeichert werden — Schreibrechte pruefen.",
                                   juce::dontSendNotification);
            }
        });
}

void RetroTraxEditor::loadSongClicked()
{
    const auto start = currentSongFile.existsAsFile() ? currentSongFile : songsFolder();

    songChooser = std::make_unique<juce::FileChooser> (
        "Song oeffnen", start, "*.retrotrax");

    songChooser->launchAsync (juce::FileBrowserComponent::openMode
                                  | juce::FileBrowserComponent::canSelectFiles,
        [this] (const juce::FileChooser& fc)
        {
            const auto file = fc.getResult();
            if (file == juce::File())
                return; // abgebrochen

            juce::StringArray missing;
            if (! proc.loadSong (file, missing))
            {
                hintLabel.setText ("\"" + file.getFileName() + "\" ist keine gueltige RetroTrax-Datei.",
                                   juce::dontSendNotification);
                return;
            }

            currentSongFile = file;
            syncUiFromState();

            juce::String msg = "Song \"" + file.getFileNameWithoutExtension() + "\" geoeffnet.";
            if (! missing.isEmpty())
                msg << "  Achtung: " << missing.size() << " Sample(s) nicht gefunden ("
                    << missing.joinIntoString (", ") << ") — Slot bleibt leer.";
            hintLabel.setText (msg, juce::dontSendNotification);
        });
}

void RetroTraxEditor::syncUiFromState()
{
    bpmSlider.setValue ((double) proc.engine.bpm.load(), juce::dontSendNotification);
    octaveBox.setSelectedId (proc.currentOctave.load(), juce::dontSendNotification);
    instrumentBox.setSelectedId (proc.currentInstrument.load() + 1, juce::dontSendNotification);
    refreshInstrumentBox();
    instDot.colour = rt::instColour (proc.currentInstrument.load());
    instDot.repaint();
    updateTransportButtons();
    grid.repaint();
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

    // Tagline mittig im freien Bereich zwischen Titel und den Song-Knoepfen.
    g.setFont (rt::mono (12.0f));
    g.setColour (rt::text.withAlpha (0.85f));
    g.drawText ("v0.3 | Sampler - Songs - SID kommt!",
                360, 0, juce::jmax (0, getWidth() - 360 - 310), header.getHeight(),
                juce::Justification::centred);
}

void RetroTraxEditor::resized()
{
    auto area = getLocalBounds();

    // Song-Knoepfe rechts in der Titelzeile (klar getrennt vom Sample-Laden).
    auto songRow = juce::Rectangle<int> (0, 0, getWidth(), 54).reduced (12, 14);
    loadSongButton.setBounds (songRow.removeFromRight (124));
    songRow.removeFromRight (8);
    saveSongButton.setBounds (songRow.removeFromRight (150));

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
    instDot.setBounds (controls.removeFromLeft (22).reduced (0, 4));
    controls.removeFromLeft (4);
    instrumentBox.setBounds (controls.removeFromLeft (190));
    controls.removeFromLeft (10);
    loadButton.setBounds (controls.removeFromLeft (130));
    controls.removeFromLeft (6);
    stDisksButton.setBounds (controls.removeFromLeft (96));

    hintLabel.setBounds (area.removeFromBottom (26));
    const auto gridArea = area.reduced (8, 4);
    grid.setBounds (gridArea);
    diskBrowser.setBounds (gridArea); // liegt als Overlay genau ueber dem Grid
}
