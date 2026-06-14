#include "PluginEditor.h"

RetroTraxEditor::RetroTraxEditor (RetroTraxProcessor& p)
    : AudioProcessorEditor (&p), proc (p), grid (p), diskBrowser (p)
{
    loc::load(); // gespeicherte Sprache (oder Systemsprache) bestimmen

    setLookAndFeel (&lnf);

    addAndMakeVisible (grid);
    addChildComponent (diskBrowser); // unsichtbar, bis SAMPLES gedrueckt wird
    addChildComponent (helpPanel);   // unsichtbar, bis ? gedrueckt wird
    addAndMakeVisible (playButton);
    addAndMakeVisible (stopButton);
    addAndMakeVisible (loadButton);
    addAndMakeVisible (stDisksButton);
    addAndMakeVisible (saveSongButton);
    addAndMakeVisible (loadSongButton);
    addAndMakeVisible (helpButton);
    addAndMakeVisible (langButton);
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

    helpButton.onClick = [this]
    {
        helpPanel.setVisible (true);
        helpPanel.toFront (false);
        helpPanel.grabKeyboardFocus();
    };
    helpPanel.onClose = [this]
    {
        helpPanel.setVisible (false);
        grid.grabKeyboardFocus();
    };

    langButton.onClick = [this]
    {
        loc::toggle();
        applyLanguage();
    };

    // Der Knopf OEFFNET nur - ein versehentlicher Doppelklick schliesst nichts.
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
        instrumentBox.addItem (juce::String::formatted ("%02d ", i) + loc::t ("(leer)", "(empty)"), i);
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

    applyLanguage(); // alle Beschriftungen in der aktuellen Sprache setzen

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
    // Auswahl merken und die Liste komplett neu aufbauen. So zeigt die
    // zugeklappte Box sofort den aktuellen Namen - nicht erst nach dem
    // Aufklappen (changeItemText aktualisiert die Anzeige naemlich nicht).
    int sel = instrumentBox.getSelectedId();
    if (sel <= 0)
        sel = proc.currentInstrument.load() + 1;

    instrumentBox.clear (juce::dontSendNotification);
    for (int i = 0; i < TrackerEngine::kInstruments; ++i)
    {
        auto name = proc.engine.getInstrumentName (i);
        instrumentBox.addItem (juce::String::formatted ("%02d ", i + 1)
                                   + (name.isEmpty() ? loc::t ("(leer)", "(empty)") : name),
                               i + 1);
    }
    instrumentBox.setSelectedId (sel, juce::dontSendNotification);
}

void RetroTraxEditor::loadSampleClicked()
{
    chooser = std::make_unique<juce::FileChooser> (
        loc::t ("Sample auswaehlen (WAV, AIFF, FLAC, OGG, MP3, Amiga 8SVX/IFF)",
                "Choose a sample (WAV, AIFF, FLAC, OGG, MP3, Amiga 8SVX/IFF)"),
        juce::File::getSpecialLocation (juce::File::userMusicDirectory),
        "*.wav;*.aif;*.aiff;*.flac;*.ogg;*.mp3;*.iff;*.8svx;*.svx");

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
                hintLabel.setText (loc::t ("Sample \"", "Sample \"") + file.getFileName()
                                       + loc::t ("\" in Slot ", "\" loaded into slot ")
                                       + juce::String::formatted ("%02d", slot + 1)
                                       + loc::t (" geladen. Klick ins Grid und tippe Noten!",
                                                 ". Click into the grid and type notes!"),
                                   juce::dontSendNotification);
            }
            else
            {
                hintLabel.setText ("\"" + file.getFileName() + loc::t ("\" konnte nicht geladen werden.",
                                                                       "\" could not be loaded."),
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
                           : songsFolder().getChildFile (loc::t ("Mein Song", "My Song") + ".retrotrax");

    songChooser = std::make_unique<juce::FileChooser> (
        loc::t ("Song speichern", "Save song"), start, "*.retrotrax");

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
                hintLabel.setText (loc::t ("Song \"", "Song \"") + file.getFileNameWithoutExtension()
                                       + loc::t ("\" gespeichert.", "\" saved."),
                                   juce::dontSendNotification);
            }
            else
            {
                hintLabel.setText (loc::t ("Song konnte nicht gespeichert werden - Schreibrechte pruefen.",
                                           "Could not save the song - check write permissions."),
                                   juce::dontSendNotification);
            }
        });
}

void RetroTraxEditor::loadSongClicked()
{
    const auto start = currentSongFile.existsAsFile() ? currentSongFile : songsFolder();

    songChooser = std::make_unique<juce::FileChooser> (
        loc::t ("Song oeffnen", "Open song"), start, "*.retrotrax");

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
                hintLabel.setText ("\"" + file.getFileName()
                                       + loc::t ("\" ist keine gueltige RetroTrax-Datei.",
                                                 "\" is not a valid RetroTrax file."),
                                   juce::dontSendNotification);
                return;
            }

            currentSongFile = file;
            syncUiFromState();

            juce::String msg = loc::t ("Song \"", "Song \"") + file.getFileNameWithoutExtension()
                                   + loc::t ("\" geoeffnet.", "\" opened.");
            if (! missing.isEmpty())
                msg << loc::t ("  Achtung: ", "  Note: ") << missing.size()
                    << loc::t (" Sample(s) nicht gefunden (", " sample(s) not found (")
                    << missing.joinIntoString (", ")
                    << loc::t (") - Slot bleibt leer.", ") - slot stays empty.");
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

void RetroTraxEditor::applyLanguage()
{
    langButton.setButtonText (loc::code()); // zeigt die aktuelle Sprache (DE/EN)
    loadButton.setButtonText     (loc::t ("SAMPLE LADEN", "LOAD SAMPLE"));
    saveSongButton.setButtonText (loc::t ("SONG SPEICHERN", "SAVE SONG"));
    loadSongButton.setButtonText (loc::t ("SONG OEFFNEN", "OPEN SONG"));
    instLabel.setText (loc::t ("INSTR", "INSTR"), juce::dontSendNotification);
    octLabel.setText  (loc::t ("OKTAVE", "OCTAVE"), juce::dontSendNotification);
    hintLabel.setText (loc::t (
        "Noten: YXCVBNM (+ SDGHJ = Halbtoene) | Q2W3ER... = Oktave hoeher | Pfeile = Cursor | "
        "Tab = Spur | Leertaste = Play/Stop | Entf = Loeschen | +/- = Oktave | "
        "Strg+Z/Y = Rueckgaengig | Strg+C/V/X = Spur kopieren/einfuegen/ausschneiden",
        "Notes: YXCVBNM (+ SDGHJ = semitones) | Q2W3ER... = octave up | Arrows = cursor | "
        "Tab = track | Space = play/stop | Del = clear | +/- = octave | "
        "Ctrl+Z/Y = undo/redo | Ctrl+C/V/X = copy/paste/cut track"),
        juce::dontSendNotification);

    refreshInstrumentBox();   // "(leer)"/"(empty)" nachziehen
    diskBrowser.applyLanguage();
    helpPanel.applyLanguage();
    repaint();
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
    g.drawText (loc::t ("v0.6 | Sampler - 8SVX - SoundFonts - SID kommt!",
                        "v0.6 | sampler - 8SVX - SoundFonts - SID coming!"),
                360, 0, juce::jmax (0, getWidth() - 360 - 392), header.getHeight(),
                juce::Justification::centred);
}

void RetroTraxEditor::resized()
{
    auto area = getLocalBounds();

    // Titelzeile rechts: ganz aussen Hilfe + Sprache, dann die Song-Knoepfe
    // (klar getrennt vom Sample-Laden in der Steuerzeile darunter).
    auto songRow = juce::Rectangle<int> (0, 0, getWidth(), 54).reduced (12, 14);
    helpButton.setBounds (songRow.removeFromRight (36));
    songRow.removeFromRight (6);
    langButton.setBounds (songRow.removeFromRight (46));
    songRow.removeFromRight (12);
    loadSongButton.setBounds (songRow.removeFromRight (118));
    songRow.removeFromRight (8);
    saveSongButton.setBounds (songRow.removeFromRight (140));

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
    helpPanel.setBounds (gridArea);   // ebenfalls Overlay ueber dem Grid
}
