#include "PluginEditor.h"

RetroTraxEditor::RetroTraxEditor (RetroTraxProcessor& p)
    : AudioProcessorEditor (&p), proc (p), grid (p), diskBrowser (p), sidPanel (p)
{
    loc::load(); // gespeicherte Sprache (oder Systemsprache) bestimmen

    setLookAndFeel (&lnf);

    addAndMakeVisible (grid);
    addChildComponent (diskBrowser); // unsichtbar, bis SAMPLES gedrueckt wird
    addChildComponent (helpPanel);   // unsichtbar, bis ? gedrueckt wird
    addChildComponent (sidPanel);    // unsichtbar, bis SID gedrueckt wird
    addAndMakeVisible (playButton);
    addAndMakeVisible (stopButton);
    addAndMakeVisible (loadButton);
    addAndMakeVisible (stDisksButton);
    addAndMakeVisible (sidButton);
    addAndMakeVisible (saveSongButton);
    addAndMakeVisible (loadSongButton);
    addAndMakeVisible (helpButton);
    addAndMakeVisible (liveHelpButton);
    addAndMakeVisible (langButton);
    addAndMakeVisible (patPrevButton);
    addAndMakeVisible (patNextButton);
    addAndMakeVisible (songModeButton);
    addAndMakeVisible (orderAddButton);
    addAndMakeVisible (orderDelButton);
    addAndMakeVisible (patLabel);
    addAndMakeVisible (orderLabel);
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

    // Live-Hilfe-Zeile an/abschalten: an -> der Cursor erklaert sich unten selbst,
    // aus -> wieder die feste Tastenkuerzel-Zeile.
    grid.onCursorInfo = [this] (const juce::String& text)
    {
        hintLabel.setText (text, juce::dontSendNotification);
    };
    liveHelpButton.onClick = [this]
    {
        const bool on = ! grid.liveHelpOn();
        grid.setLiveHelp (on); // zeigt bei "an" sofort die aktuelle Stelle
        liveHelpButton.setToggleState (on, juce::dontSendNotification);
        if (! on)
            setDefaultHint();
        grid.grabKeyboardFocus();
    };

    // --- Song-Modus-Leiste -------------------------------------------------
    patPrevButton.onClick = [this]
    {
        proc.engine.setEditPattern (proc.engine.editPattern.load() - 1);
        updateSongUi(); grid.repaint(); grid.grabKeyboardFocus();
    };
    patNextButton.onClick = [this]
    {
        proc.engine.setEditPattern (proc.engine.editPattern.load() + 1);
        updateSongUi(); grid.repaint(); grid.grabKeyboardFocus();
    };
    songModeButton.onClick = [this]
    {
        proc.engine.songMode = ! proc.engine.songMode.load();
        updateSongUi(); grid.grabKeyboardFocus();
    };
    orderAddButton.onClick = [this]
    {
        if (proc.engine.orderLen < TrackerEngine::kMaxOrder)
            proc.engine.order[proc.engine.orderLen++] = proc.engine.editPattern.load();
        updateSongUi(); grid.grabKeyboardFocus();
    };
    orderDelButton.onClick = [this]
    {
        if (proc.engine.orderLen > 1)
            --proc.engine.orderLen;
        updateSongUi(); grid.grabKeyboardFocus();
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

    // SID: aktuellen Slot zu einem SID-Synth machen und den Editor oeffnen.
    sidButton.onClick = [this]
    {
        const int slot = proc.currentInstrument.load();
        if (! proc.isSid (slot))
            proc.makeSidInstrument (slot); // ersetzt den Slot-Inhalt durch einen SID
        refreshInstrumentBox();
        instDot.colour = rt::instColour (slot);
        instDot.repaint();

        sidPanel.refresh();
        sidPanel.setVisible (true);
        sidButton.setToggleState (true, juce::dontSendNotification);
        sidPanel.toFront (false);
        sidPanel.grabKeyboardFocus();
    };
    sidPanel.onClose = [this]
    {
        sidPanel.setVisible (false);
        sidButton.setToggleState (false, juce::dontSendNotification);
        grid.grabKeyboardFocus();
    };
    sidPanel.onChanged = [this]
    {
        refreshInstrumentBox();
        instDot.colour = rt::instColour (proc.currentInstrument.load());
        instDot.repaint();
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

    patLabel.setFont (rt::mono (14.0f, true));
    patLabel.setColour (juce::Label::textColourId, rt::cursor);
    patLabel.setJustificationType (juce::Justification::centred);
    orderLabel.setFont (rt::mono (13.0f));
    orderLabel.setColour (juce::Label::textColourId, rt::fxCol);
    orderLabel.setJustificationType (juce::Justification::centredLeft);

    applyLanguage(); // alle Beschriftungen in der aktuellen Sprache setzen

    grid.onTransportChange = [this] { updateTransportButtons(); };
    updateTransportButtons();

    // Live-Hilfe zu Beginn an - Einsteiger sehen sofort, was die Stelle bedeutet.
    grid.setLiveHelp (true);
    liveHelpButton.setToggleState (true, juce::dontSendNotification);

    updateSongUi();

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
    updateSongUi();
    grid.repaint();
}

void RetroTraxEditor::updateSongUi()
{
    auto& e = proc.engine;
    patLabel.setText (loc::t ("PATTERN ", "PATTERN ")
                      + juce::String::formatted ("%02d", e.editPattern.load() + 1),
                      juce::dontSendNotification);

    const bool song = e.songMode.load();
    songModeButton.setButtonText (song ? juce::String ("SONG") : juce::String ("LOOP"));
    songModeButton.setToggleState (song, juce::dontSendNotification);

    juce::StringArray ord;
    for (int i = 0; i < e.orderLen; ++i)
        ord.add (juce::String::formatted ("%02d", e.order[i] + 1));
    orderLabel.setText (loc::t ("Reihe: ", "Order: ") + ord.joinIntoString (" "),
                        juce::dontSendNotification);
}

void RetroTraxEditor::setDefaultHint()
{
    hintLabel.setText (loc::t (
        "Noten: YXCVBNM (+ SDGHJ = Halbtoene) | Q2W3ER... = Oktave hoeher | Pfeile = Cursor | "
        "Tab = Spur | Leertaste = Play/Stop | Entf = Loeschen | +/- = Oktave | "
        "Strg+Z/Y = Rueckgaengig | Strg+C/V/X = Spur kopieren/einfuegen/ausschneiden",
        "Notes: YXCVBNM (+ SDGHJ = semitones) | Q2W3ER... = octave up | Arrows = cursor | "
        "Tab = track | Space = play/stop | Del = clear | +/- = octave | "
        "Ctrl+Z/Y = undo/redo | Ctrl+C/V/X = copy/paste/cut track"),
        juce::dontSendNotification);
}

void RetroTraxEditor::applyLanguage()
{
    langButton.setButtonText (loc::code()); // zeigt die aktuelle Sprache (DE/EN)
    loadButton.setButtonText     (loc::t ("SAMPLE LADEN", "LOAD SAMPLE"));
    saveSongButton.setButtonText (loc::t ("SONG SPEICHERN", "SAVE SONG"));
    loadSongButton.setButtonText (loc::t ("SONG OEFFNEN", "OPEN SONG"));
    instLabel.setText (loc::t ("INSTR", "INSTR"), juce::dontSendNotification);
    octLabel.setText  (loc::t ("OKTAVE", "OCTAVE"), juce::dontSendNotification);
    liveHelpButton.setButtonText (loc::t ("TIPP", "TIP"));
    liveHelpButton.setTooltip (loc::t ("Hilfe-Zeile an/aus - erklaert die Stelle unterm Cursor",
                                       "Help line on/off - explains the spot under the cursor"));

    songModeButton.setTooltip (loc::t ("LOOP = aktuelles Pattern wiederholen | SONG = die ganze Reihe abspielen",
                                       "LOOP = repeat current pattern | SONG = play the whole order"));
    orderAddButton.setTooltip (loc::t ("Aktuelles Pattern hinten an die Reihenfolge anhaengen",
                                       "Append the current pattern to the order"));
    orderDelButton.setTooltip (loc::t ("Letzten Eintrag aus der Reihenfolge nehmen",
                                       "Remove the last entry from the order"));
    updateSongUi();

    // Bei aktiver Live-Hilfe die Cursor-Erklaerung in neuer Sprache neu setzen,
    // sonst die feste Tastenkuerzel-Zeile.
    if (grid.liveHelpOn())
        grid.setLiveHelp (true);
    else
        setDefaultHint();

    sidButton.setTooltip (loc::t ("Aktuellen Slot zu einem SID-Synth machen (Wellenform + Huellkurve)",
                                  "Turn the current slot into a SID synth (waveform + envelope)"));

    refreshInstrumentBox();   // "(leer)"/"(empty)" nachziehen
    diskBrowser.applyLanguage();
    helpPanel.applyLanguage();
    sidPanel.applyLanguage();
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
    g.drawText (loc::t ("v0.11 | SID-Synth - Wellenformen, Huellkurve & Filter",
                        "v0.11 | SID synth - waveforms, envelope & filter"),
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
    songRow.removeFromRight (6);
    liveHelpButton.setBounds (songRow.removeFromRight (60));
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
    bpmSlider.setBounds (controls.removeFromLeft (110));
    controls.removeFromLeft (14);
    octLabel.setBounds (controls.removeFromLeft (52));
    octaveBox.setBounds (controls.removeFromLeft (58));
    controls.removeFromLeft (14);
    instLabel.setBounds (controls.removeFromLeft (46));
    instDot.setBounds (controls.removeFromLeft (22).reduced (0, 4));
    controls.removeFromLeft (4);
    instrumentBox.setBounds (controls.removeFromLeft (150));
    controls.removeFromLeft (10);
    loadButton.setBounds (controls.removeFromLeft (130));
    controls.removeFromLeft (6);
    stDisksButton.setBounds (controls.removeFromLeft (84));
    controls.removeFromLeft (6);
    sidButton.setBounds (controls.removeFromLeft (64));

    // Song-Modus-Leiste: Pattern waehlen, LOOP/SONG, Reihenfolge bearbeiten.
    auto song = area.removeFromTop (32).reduced (8, 3);
    patPrevButton.setBounds (song.removeFromLeft (58));
    song.removeFromLeft (4);
    patLabel.setBounds (song.removeFromLeft (104));
    song.removeFromLeft (4);
    patNextButton.setBounds (song.removeFromLeft (58));
    song.removeFromLeft (16);
    songModeButton.setBounds (song.removeFromLeft (72));
    song.removeFromLeft (16);
    orderAddButton.setBounds (song.removeFromLeft (66));
    song.removeFromLeft (4);
    orderDelButton.setBounds (song.removeFromLeft (66));
    song.removeFromLeft (12);
    orderLabel.setBounds (song);

    hintLabel.setBounds (area.removeFromBottom (26));
    const auto gridArea = area.reduced (8, 4);
    grid.setBounds (gridArea);
    diskBrowser.setBounds (gridArea); // liegt als Overlay genau ueber dem Grid
    helpPanel.setBounds (gridArea);   // ebenfalls Overlay ueber dem Grid
    sidPanel.setBounds (gridArea);    // ebenfalls Overlay ueber dem Grid
}
