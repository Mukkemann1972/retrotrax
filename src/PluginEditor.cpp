#include "PluginEditor.h"

RetroTraxEditor::RetroTraxEditor (RetroTraxProcessor& p)
    : AudioProcessorEditor (&p), proc (p), grid (p), diskBrowser (p), sidPanel (p), akaiPanel (p),
      kitPanel (p), spectrumPanel (p)
{
    loc::load(); // gespeicherte Sprache (oder Systemsprache) bestimmen

    setLookAndFeel (&lnf);

    addAndMakeVisible (grid);
    addChildComponent (diskBrowser); // unsichtbar, bis SAMPLES gedrueckt wird
    addChildComponent (helpPanel);   // unsichtbar, bis ? gedrueckt wird
    addChildComponent (sidPanel);    // unsichtbar, bis SID gedrueckt wird
    addChildComponent (akaiPanel);   // unsichtbar, bis AKAI gedrueckt wird
    addChildComponent (kitPanel);    // unsichtbar, bis KIT gedrueckt wird
    addChildComponent (spectrumPanel); // unsichtbar, bis SPEKTRUM gedrueckt wird
    addAndMakeVisible (playButton);
    addAndMakeVisible (stopButton);
    addAndMakeVisible (loadMenuButton);
    addAndMakeVisible (sidButton);
    addAndMakeVisible (akaiButton);
    addAndMakeVisible (kitButton);
    addAndMakeVisible (saveSongButton);
    addAndMakeVisible (wavButton);
    addAndMakeVisible (helpButton);
    addAndMakeVisible (liveHelpButton);
    addAndMakeVisible (spectrumButton);
    addAndMakeVisible (langButton);
    addAndMakeVisible (patPrevButton);
    addAndMakeVisible (patNextButton);
    addAndMakeVisible (songModeButton);
    addAndMakeVisible (orderAddButton);
    addAndMakeVisible (orderDelButton);
    addAndMakeVisible (quantBox);
    addAndMakeVisible (quantButton);
    // Raster-Auswahl: ID = Schrittweite in Zeilen (4 Zeilen/Beat = 16tel-Standard).
    quantBox.addItem ("1/8", 2);
    quantBox.addItem ("1/4", 4);
    quantBox.addItem ("1/2", 8);
    quantBox.addItem ("1/1", 16);
    quantBox.setSelectedId (2, juce::dontSendNotification); // Achtel als Standard
    quantButton.onClick = [this]
    {
        const int step = quantBox.getSelectedId();
        grid.quantize (step);
        hintLabel.setText (loc::t ("Pattern quantisiert (Strg+Z macht es rueckgaengig)",
                                   "Pattern quantised (Ctrl+Z to undo)"),
                           juce::dontSendNotification);
        grid.grabKeyboardFocus();
    };
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
    saveSongButton.onClick = [this] { saveSongClicked(); };
    wavButton.onClick = [this] { exportWavClicked(); };
    // EIN Lade-Knopf statt vieler: oeffnet ein Aufklapp-Menue mit allem zum Laden
    // (Sample, Sample-Browser, Song) und einem Untermenue "Importieren" (MOD/XM/TFMX).
    loadMenuButton.onClick = [this]
    {
        juce::PopupMenu m;
        m.addItem (1, loc::t ("Sample laden ...", "Load sample ..."));
        m.addItem (2, loc::t ("Sample-Browser (ST-Disks) ...", "Sample browser (ST-Disks) ..."));
        m.addSeparator();
        m.addItem (3, loc::t ("Song oeffnen ...", "Open song ..."));
        m.addSeparator();
        juce::PopupMenu imp;
        imp.addItem (10, loc::t ("Amiga MOD (.mod) ...", "Amiga MOD (.mod) ..."));
        imp.addItem (11, loc::t ("FastTracker XM (.xm) ...", "FastTracker XM (.xm) ..."));
        imp.addItem (13, loc::t ("Scream Tracker 3 (.s3m) ...", "Scream Tracker 3 (.s3m) ..."));
        imp.addItem (14, loc::t ("Impulse Tracker (.it) ...", "Impulse Tracker (.it) ..."));
        imp.addItem (12, loc::t ("TFMX - Huelsbeck (.tfmx/.mdat) ...", "TFMX - Huelsbeck (.tfmx/.mdat) ..."));
        m.addSubMenu (loc::t ("Importieren", "Import"), imp);
        m.addSeparator();
        m.addItem (20, loc::t ("TFMX-Samples entnehmen (Grabber) ...",
                               "Grab samples from TFMX ..."));
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&loadMenuButton),
            [this] (int r)
            {
                switch (r)
                {
                    case 1:  loadSampleClicked(); break;
                    case 2:  diskBrowser.setVisible (true); diskBrowser.toFront (false); break;
                    case 3:  loadSongClicked(); break;
                    case 10: loadModClicked(); break;
                    case 11: loadXmClicked(); break;
                    case 12: loadTfmxClicked(); break;
                    case 13: loadS3mClicked(); break;
                    case 14: loadItClicked(); break;
                    case 20: grabTfmxClicked(); break;
                    default: break;
                }
            });
    };

    helpButton.onClick = [this]
    {
        helpPanel.setVisible (true);
        helpPanel.toFront (false);
        helpPanel.grabKeyboardFocus();
    };

    // Kein eigener Vollbild-Knopf mehr: das Maximieren-Viereck oben rechts im
    // Fensterrahmen (neben - und X) macht das ohnehin - spart Platz in der Leiste.
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

    // Der Sample-Browser oeffnet ueber das LADEN-Menue; zu ist er erst per
    // SCHLIESSEN-Knopf oder ESC.
    diskBrowser.onClose = [this]
    {
        diskBrowser.setVisible (false);
        grid.grabKeyboardFocus();
    };

    // Spektrum-Anzeige (Frequenzbalken) als Overlay ein-/ausblenden.
    spectrumButton.onClick = [this]
    {
        const bool show = ! spectrumPanel.isVisible();
        spectrumPanel.setVisible (show);
        spectrumButton.setToggleState (show, juce::dontSendNotification);
        if (show) spectrumPanel.toFront (false);
        else      grid.grabKeyboardFocus();
    };
    spectrumPanel.onClose = [this]
    {
        spectrumPanel.setVisible (false);
        spectrumButton.setToggleState (false, juce::dontSendNotification);
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

    akaiButton.onClick = [this]
    {
        const int slot = proc.currentInstrument.load();
        if (! proc.isSampleSlot (slot))
        {
            // Kein Sample im Slot -> kurz erklaeren statt ein leeres Panel zu zeigen.
            hintLabel.setText (loc::t ("AKAI-Filter braucht ein Sample im Slot - erst ein Sample laden.",
                                       "Akai filter needs a sample in the slot - load a sample first."),
                               juce::dontSendNotification);
            return;
        }
        akaiPanel.refresh();
        akaiPanel.setVisible (true);
        akaiButton.setToggleState (true, juce::dontSendNotification);
        akaiPanel.toFront (false);
        akaiPanel.grabKeyboardFocus();
    };
    akaiPanel.onClose = [this]
    {
        akaiPanel.setVisible (false);
        akaiButton.setToggleState (false, juce::dontSendNotification);
        grid.grabKeyboardFocus();
    };

    kitButton.onClick = [this]
    {
        kitPanel.refresh();
        kitPanel.setVisible (true);
        kitButton.setToggleState (true, juce::dontSendNotification);
        kitPanel.toFront (false);
        kitPanel.grabKeyboardFocus();
    };
    kitPanel.onClose = [this]
    {
        kitPanel.setVisible (false);
        kitButton.setToggleState (false, juce::dontSendNotification);
        grid.grabKeyboardFocus();
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
    setResizeLimits (960, 520, 3840, 2160); // grosses Maximum, damit Maximieren den Schirm fuellt
    setSize (1240, 720); // etwas breiter: Platz fuer den AKAI-Knopf, Spalten gleich lesbar

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

void RetroTraxEditor::exportWavClicked()
{
    // Vorschlag: Songname (falls gespeichert), sonst Standardname, als .wav.
    const auto base = currentSongFile.existsAsFile()
                          ? currentSongFile.getFileNameWithoutExtension()
                          : loc::t ("Mein Song", "My Song");
    const auto start = songsFolder().getChildFile (base + ".wav");

    songChooser = std::make_unique<juce::FileChooser> (
        loc::t ("Song als WAV exportieren", "Export song as WAV"), start, "*.wav");

    songChooser->launchAsync (juce::FileBrowserComponent::saveMode
                                  | juce::FileBrowserComponent::warnAboutOverwriting,
        [this] (const juce::FileChooser& fc)
        {
            auto file = fc.getResult();
            if (file == juce::File())
                return; // abgebrochen
            if (file.getFileExtension().isEmpty())
                file = file.withFileExtension ("wav");

            juce::String msg;
            const bool ok = proc.renderSongToWav (file, msg);
            hintLabel.setText (ok ? msg
                                  : loc::t ("WAV-Export fehlgeschlagen: ", "WAV export failed: ") + msg,
                               juce::dontSendNotification);
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

void RetroTraxEditor::loadModClicked()
{
    const auto start = currentSongFile.existsAsFile()
                         ? currentSongFile.getParentDirectory() : songsFolder();

    songChooser = std::make_unique<juce::FileChooser> (
        loc::t ("Amiga-MOD importieren", "Import Amiga MOD"), start, "*.mod;*.MOD");

    songChooser->launchAsync (juce::FileBrowserComponent::openMode
                                  | juce::FileBrowserComponent::canSelectFiles,
        [this] (const juce::FileChooser& fc)
        {
            const auto file = fc.getResult();
            if (file == juce::File())
                return; // abgebrochen

            juce::String message;
            if (! proc.loadMod (file, message))
            {
                hintLabel.setText (loc::t ("MOD-Import fehlgeschlagen: ", "MOD import failed: ") + message,
                                   juce::dontSendNotification);
                return;
            }

            currentSongFile = juce::File(); // ein importiertes MOD ist (noch) keine .retrotrax-Datei
            syncUiFromState();
            hintLabel.setText (loc::t ("MOD importiert - ", "MOD imported - ") + message,
                               juce::dontSendNotification);
        });
}

void RetroTraxEditor::loadXmClicked()
{
    const auto start = currentSongFile.existsAsFile()
                         ? currentSongFile.getParentDirectory() : songsFolder();

    songChooser = std::make_unique<juce::FileChooser> (
        loc::t ("FastTracker-XM importieren", "Import FastTracker XM"), start, "*.xm;*.XM");

    songChooser->launchAsync (juce::FileBrowserComponent::openMode
                                  | juce::FileBrowserComponent::canSelectFiles,
        [this] (const juce::FileChooser& fc)
        {
            const auto file = fc.getResult();
            if (file == juce::File())
                return; // abgebrochen

            juce::String message;
            if (! proc.loadXm (file, message))
            {
                hintLabel.setText (loc::t ("XM-Import fehlgeschlagen: ", "XM import failed: ") + message,
                                   juce::dontSendNotification);
                return;
            }

            currentSongFile = juce::File(); // ein importiertes XM ist (noch) keine .retrotrax-Datei
            syncUiFromState();
            hintLabel.setText (loc::t ("XM importiert - ", "XM imported - ") + message,
                               juce::dontSendNotification);
        });
}

void RetroTraxEditor::loadS3mClicked()
{
    const auto start = currentSongFile.existsAsFile()
                         ? currentSongFile.getParentDirectory() : songsFolder();

    songChooser = std::make_unique<juce::FileChooser> (
        loc::t ("Scream-Tracker-S3M importieren", "Import Scream Tracker S3M"), start, "*.s3m;*.S3M");

    songChooser->launchAsync (juce::FileBrowserComponent::openMode
                                  | juce::FileBrowserComponent::canSelectFiles,
        [this] (const juce::FileChooser& fc)
        {
            const auto file = fc.getResult();
            if (file == juce::File())
                return;

            juce::String message;
            if (! proc.loadS3m (file, message))
            {
                hintLabel.setText (loc::t ("S3M-Import fehlgeschlagen: ", "S3M import failed: ") + message,
                                   juce::dontSendNotification);
                return;
            }
            currentSongFile = juce::File();
            syncUiFromState();
            hintLabel.setText (loc::t ("S3M importiert - ", "S3M imported - ") + message,
                               juce::dontSendNotification);
        });
}

void RetroTraxEditor::loadItClicked()
{
    const auto start = currentSongFile.existsAsFile()
                         ? currentSongFile.getParentDirectory() : songsFolder();

    songChooser = std::make_unique<juce::FileChooser> (
        loc::t ("Impulse-Tracker-IT importieren", "Import Impulse Tracker IT"), start, "*.it;*.IT");

    songChooser->launchAsync (juce::FileBrowserComponent::openMode
                                  | juce::FileBrowserComponent::canSelectFiles,
        [this] (const juce::FileChooser& fc)
        {
            const auto file = fc.getResult();
            if (file == juce::File())
                return;

            juce::String message;
            if (! proc.loadIt (file, message))
            {
                hintLabel.setText (loc::t ("IT-Import fehlgeschlagen: ", "IT import failed: ") + message,
                                   juce::dontSendNotification);
                return;
            }
            currentSongFile = juce::File();
            syncUiFromState();
            hintLabel.setText (loc::t ("IT importiert - ", "IT imported - ") + message,
                               juce::dontSendNotification);
        });
}

void RetroTraxEditor::loadTfmxClicked()
{
    const auto start = currentSongFile.existsAsFile()
                         ? currentSongFile.getParentDirectory() : songsFolder();

    // TFMX-Dateien heissen je nach Quelle "mdat.songname" (Modland), "songname.mdat"
    // oder "songname.tfmx" (mit Sample-Datei .smpl bzw. .sam daneben).
    songChooser = std::make_unique<juce::FileChooser> (
        loc::t ("TFMX-Modul oeffnen (.mdat / .tfmx)", "Open TFMX module (.mdat / .tfmx)"),
        start, "*.mdat;mdat.*;*.tfmx;*.tfx;*.tfm");

    songChooser->launchAsync (juce::FileBrowserComponent::openMode
                                  | juce::FileBrowserComponent::canSelectFiles,
        [this] (const juce::FileChooser& fc)
        {
            const auto file = fc.getResult();
            if (file == juce::File())
                return; // abgebrochen

            juce::String message;
            if (! proc.loadTfmx (file, message))
            {
                hintLabel.setText (loc::t ("TFMX laden fehlgeschlagen: ", "TFMX load failed: ") + message,
                                   juce::dontSendNotification);
                return;
            }

            hintLabel.setText (loc::t ("TFMX gelesen - ", "TFMX read - ") + message,
                               juce::dontSendNotification);
        });
}

void RetroTraxEditor::grabTfmxClicked()
{
    const auto start = currentSongFile.existsAsFile()
                         ? currentSongFile.getParentDirectory() : songsFolder();

    songChooser = std::make_unique<juce::FileChooser> (
        loc::t ("TFMX-Modul fuer den Grabber waehlen (.mdat / .tfmx)",
                "Choose TFMX module to grab from (.mdat / .tfmx)"),
        start, "*.mdat;mdat.*;*.tfmx;*.tfx;*.tfm");

    songChooser->launchAsync (juce::FileBrowserComponent::openMode
                                  | juce::FileBrowserComponent::canSelectFiles,
        [this] (const juce::FileChooser& fc)
        {
            const auto file = fc.getResult();
            if (file == juce::File())
                return; // abgebrochen

            // Zielordner: Musik/RetroTrax/TFMX-Samples/<Titel> - wird als eigener
            // Ordner im Sample-Browser angezeigt (preview, IN SLOT LADEN, MERKEN).
            auto title = juce::File::createLegalFileName (file.getFileNameWithoutExtension());
            if (file.getFileName().startsWithIgnoreCase ("mdat."))
                title = juce::File::createLegalFileName (file.getFileName().substring (5));
            if (title.isEmpty())
                title = "TFMX";

            const auto outFolder = songsFolder().getChildFile ("TFMX-Samples").getChildFile (title);

            juce::String message;
            const int n = proc.grabTfmxSamples (file, outFolder, message);
            if (n <= 0)
            {
                hintLabel.setText (loc::t ("Grabber: ", "Grabber: ") + message,
                                   juce::dontSendNotification);
                return;
            }

            hintLabel.setText (loc::t ("TFMX-Grabber - ", "TFMX grabber - ") + message,
                               juce::dontSendNotification);
            diskBrowser.showFolder (outFolder); // gleich anzeigen
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
    loadMenuButton.setButtonText (loc::t ("LADEN", "LOAD"));
    loadMenuButton.setTooltip (loc::t ("Sample oder Song laden, Sample-Browser, oder ein Modul importieren (MOD/XM/TFMX)",
                                       "Load a sample or song, open the sample browser, or import a module (MOD/XM/TFMX)"));
    saveSongButton.setButtonText (loc::t ("SONG SPEICHERN", "SAVE SONG"));
    wavButton.setButtonText (loc::t ("WAV", "WAV"));
    wavButton.setTooltip (loc::t ("Song als WAV-Datei rausrendern - zum Teilen, Hochladen (Ko-fi/YouTube)",
                                  "Render the song to a WAV file - to share or upload (Ko-fi/YouTube)"));
    instLabel.setText (loc::t ("INSTR", "INSTR"), juce::dontSendNotification);
    octLabel.setText  (loc::t ("OKTAVE", "OCTAVE"), juce::dontSendNotification);
    liveHelpButton.setButtonText (loc::t ("TIPP", "TIP"));
    liveHelpButton.setTooltip (loc::t ("Hilfe-Zeile an/aus - erklaert die Stelle unterm Cursor",
                                       "Help line on/off - explains the spot under the cursor"));
    spectrumButton.setButtonText (loc::t ("SPEKTRUM", "SPECTRUM"));
    spectrumButton.setTooltip (loc::t ("Frequenz-Anzeige ein/aus - die tanzenden Balken des Klangs",
                                       "Frequency display on/off - the dancing bars of the sound"));

    songModeButton.setTooltip (loc::t ("LOOP = aktuelles Pattern wiederholen | SONG = die ganze Reihe abspielen",
                                       "LOOP = repeat current pattern | SONG = play the whole order"));
    quantButton.setButtonText (loc::t ("QUANT", "QUANT"));
    quantButton.setTooltip (loc::t ("Aufgenommene Noten im Pattern aufs gewaehlte Raster schnappen (Strg+Z macht es rueckgaengig)",
                                     "Snap the recorded notes in the pattern to the chosen grid (Ctrl+Z to undo)"));
    quantBox.setTooltip (loc::t ("Raster: 1/8 = jede 2. Zeile, 1/4 = jede 4. Zeile usw.",
                                 "Grid: 1/8 = every 2nd row, 1/4 = every 4th row, etc."));
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
    kitButton.setButtonText (loc::t ("KIT", "KIT"));
    kitButton.setTooltip (loc::t ("Drum-Kit: 16 Pads im MPC60/SP-1200-Stil zum Trommeln (eigene Samples)",
                                  "Drum kit: 16 pads in MPC60/SP-1200 style for finger drumming (own samples)"));
    kitPanel.applyLanguage();
    akaiButton.setButtonText (loc::t ("AKAI", "AKAI"));
    akaiButton.setTooltip (loc::t ("Akai-Sampler-Filter fuer das aktuelle Sample (resonanter Tiefpass + 12-Bit-Crunch)",
                                   "Akai sampler filter for the current sample (resonant low-pass + 12-bit crunch)"));

    refreshInstrumentBox();   // "(leer)"/"(empty)" nachziehen
    diskBrowser.applyLanguage();
    helpPanel.applyLanguage();
    sidPanel.applyLanguage();
    akaiPanel.applyLanguage();
    spectrumPanel.applyLanguage();
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
    g.drawText (loc::t ("v0.44 | SP-1200/Emu-Klang pro Pad",
                        "v0.44 | SP-1200/Emu sound per pad"),
                360, 0, juce::jmax (0, getWidth() - 360 - 300), header.getHeight(),
                juce::Justification::centred);
}

void RetroTraxEditor::parentHierarchyChanged()
{
    // Im Standalone steckt der Editor in einem DocumentWindow, das per Default nur
    // Minimieren + Schliessen zeigt - kein Maximieren-4eck. Wir schalten alle drei
    // Titelleisten-Knoepfe frei, sodass das gewohnte Viereck oben rechts (neben - und
    // X) erscheint und das Fenster gross macht. So braucht es keinen eigenen
    // VOLLBILD-Knopf in der Leiste. Im Plugin-Host (kein DocumentWindow) passiert
    // nichts - dann rahmt der Host das Fenster ohnehin selbst.
    if (auto* dw = findParentComponentOfClass<juce::DocumentWindow>())
        dw->setTitleBarButtonsRequired (juce::DocumentWindow::allButtons, false);
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
    songRow.removeFromRight (6);
    spectrumButton.setBounds (songRow.removeFromRight (96));
    songRow.removeFromRight (12);
    saveSongButton.setBounds (songRow.removeFromRight (140));
    songRow.removeFromRight (6);
    wavButton.setBounds (songRow.removeFromRight (60));

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
    loadMenuButton.setBounds (controls.removeFromLeft (110));
    controls.removeFromLeft (6);
    sidButton.setBounds (controls.removeFromLeft (64));
    controls.removeFromLeft (6);
    akaiButton.setBounds (controls.removeFromLeft (70));
    controls.removeFromLeft (6);
    kitButton.setBounds (controls.removeFromLeft (64));

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
    song.removeFromLeft (16);
    quantBox.setBounds (song.removeFromLeft (72));
    song.removeFromLeft (4);
    quantButton.setBounds (song.removeFromLeft (72));
    song.removeFromLeft (12);
    orderLabel.setBounds (song);

    hintLabel.setBounds (area.removeFromBottom (26));
    const auto gridArea = area.reduced (8, 4);
    grid.setBounds (gridArea);
    diskBrowser.setBounds (gridArea); // liegt als Overlay genau ueber dem Grid
    helpPanel.setBounds (gridArea);   // ebenfalls Overlay ueber dem Grid
    sidPanel.setBounds (gridArea);    // ebenfalls Overlay ueber dem Grid
    akaiPanel.setBounds (gridArea);   // ebenfalls Overlay ueber dem Grid
    kitPanel.setBounds (gridArea);    // ebenfalls Overlay ueber dem Grid
    spectrumPanel.setBounds (gridArea); // ebenfalls Overlay ueber dem Grid
}
