#include "HelpPanel.h"

HelpPanel::HelpPanel()
{
    topicList.setColour (juce::ListBox::backgroundColourId, rt::rowBeat);
    topicList.setColour (juce::ListBox::outlineColourId, rt::steel.withAlpha (0.5f));
    topicList.setOutlineThickness (1);
    topicList.setRowHeight (24);
    addAndMakeVisible (topicList);

    bodyView.setMultiLine (true);
    bodyView.setReadOnly (true);
    bodyView.setScrollbarsShown (true);
    bodyView.setCaretVisible (false);
    bodyView.setFont (rt::mono (14.0f));
    bodyView.setColour (juce::TextEditor::backgroundColourId, rt::rowBeat);
    bodyView.setColour (juce::TextEditor::textColourId, rt::text);
    bodyView.setColour (juce::TextEditor::outlineColourId, rt::steel.withAlpha (0.5f));
    addAndMakeVisible (bodyView);

    closeButton.onClick = [this] { if (onClose) onClose(); };
    addAndMakeVisible (closeButton);

    setWantsKeyboardFocus (true);
    rebuild();
}

void HelpPanel::applyLanguage()
{
    closeButton.setButtonText (loc::t ("SCHLIESSEN", "CLOSE"));
    rebuild();
    repaint();
}

// ---- Inhalte (waechst hier mit jedem neuen Feature mit) -------------------

void HelpPanel::rebuild()
{
    const int keep = currentTopic;
    topics.clearQuick();

    topics.add ({ loc::t ("Willkommen", "Welcome"),
        loc::t (
            "Mukkemann RetroTrax - der Tracker mit Herz: Amiga-Gefuehl + C64-SID,\n"
            "fuer heute gebaut.\n\n"
            "Ein Tracker ist ein Musikprogramm, in dem du Noten in ein Raster (das\n"
            "\"Pattern\") tippst. Jede Spalte ist eine Spur, jede Zeile ein Schritt.\n"
            "Von oben nach unten wird abgespielt - wie Noten auf einer Walze.\n\n"
            "Du brauchst keine Vorkenntnisse. Lade einen Sound, tippe ein paar\n"
            "Noten, druecke die Leertaste - fertig ist dein erster Beat.",
            "Mukkemann RetroTrax - the tracker with heart: Amiga feel + C64 SID,\n"
            "built for today.\n\n"
            "A tracker is a music program where you type notes into a grid (the\n"
            "\"pattern\"). Each column is a track, each line a step. Playback runs\n"
            "top to bottom - like notes on a music box cylinder.\n\n"
            "No prior knowledge needed. Load a sound, type a few notes, hit the\n"
            "space bar - and there's your first beat.") });

    topics.add ({ loc::t ("Schnellstart", "Quick start"),
        loc::t (
            "1. Druecke SAMPLES und such dir einen Sound aus den ST-Disks aus\n"
            "   (Anklicken spielt ihn sofort vor). Dann IN SLOT LADEN.\n"
            "2. Klicke ins Raster und tippe Noten mit den Tasten Y X C V B N M.\n"
            "3. Druecke die Leertaste zum Abspielen. Nochmal = Stop.\n"
            "4. Stelle das Tempo (BPM) oben ein.\n"
            "5. Wenn es dir gefaellt: SONG SPEICHERN.\n\n"
            "Das war's im Kern. Alles Weitere findest du in den Themen links.",
            "1. Press SAMPLES and pick a sound from the ST disks (clicking plays\n"
            "   it instantly). Then LOAD INTO SLOT.\n"
            "2. Click into the grid and type notes with the keys Y X C V B N M.\n"
            "3. Press the space bar to play. Again = stop.\n"
            "4. Set the tempo (BPM) at the top.\n"
            "5. When you like it: SAVE SONG.\n\n"
            "That's the core. Everything else is in the topics on the left.") });

    topics.add ({ loc::t ("Noten tippen (Tastatur)", "Typing notes (keyboard)"),
        loc::t (
            "Die Tastatur ist dein Klavier (deutsches QWERTZ-Layout):\n\n"
            "  Y X C V B N M        Noten in der aktuellen Oktave\n"
            "  S D   G H J          die schwarzen Tasten (Halbtoene)\n"
            "  Q 2 W 3 E R 5 T 6 Z  eine Oktave hoeher\n\n"
            "  Pfeiltasten / Bild auf-ab / Pos1 / Ende   Cursor bewegen\n"
            "  Tab / Shift+Tab      naechste / vorherige Spur\n"
            "  Leertaste            Play / Stop\n"
            "  Entf / Ruecktaste    Zelle loeschen\n"
            "  + / -                Oktave wechseln\n"
            "  Ziffern              Wert in Instrument-/Lautstaerke-Spalte\n\n"
            "  Strg+Z / Strg+Y      Rueckgaengig / Wiederholen\n"
            "  Strg+C / V / X       Spur kopieren / einfuegen / ausschneiden\n\n"
            "Vertippt? Kein Problem - Strg+Z nimmt jeden Schritt zurueck (bis zu\n"
            "64). Mit Strg+C/V kopierst du eine ganze Spur z.B. von Spur 1 nach 5:\n"
            "Cursor in die Quellspur, Strg+C, dann in die Zielspur und Strg+V.",
            "Your keyboard is the piano (German QWERTZ layout):\n\n"
            "  Y X C V B N M        notes in the current octave\n"
            "  S D   G H J          the black keys (semitones)\n"
            "  Q 2 W 3 E R 5 T 6 Z  one octave higher\n\n"
            "  Arrow keys / Page up-down / Home / End   move the cursor\n"
            "  Tab / Shift+Tab      next / previous track\n"
            "  Space bar            play / stop\n"
            "  Delete / Backspace   clear cell\n"
            "  + / -                change octave\n"
            "  Number keys          value in instrument / volume column\n\n"
            "  Ctrl+Z / Ctrl+Y      undo / redo\n"
            "  Ctrl+C / V / X       copy / paste / cut track\n\n"
            "Typo? No problem - Ctrl+Z takes back every step (up to 64). With\n"
            "Ctrl+C/V you copy a whole track, e.g. from track 1 to 5: cursor into\n"
            "the source track, Ctrl+C, then into the target track and Ctrl+V.") });

    topics.add ({ loc::t ("Pattern & Spuren", "Pattern & tracks"),
        loc::t (
            "Das Raster hat 8 Spuren und 64 Zeilen. Die Cursor-Zeile bleibt in\n"
            "der Mitte, das Pattern scrollt daran vorbei - wie beim alten\n"
            "ProTracker.\n\n"
            "Jede Spur spielt zur gleichen Zeit eine Note ab. So baust du\n"
            "Schlagzeug, Bass und Melodie uebereinander.\n\n"
            "(Bald: mehrere Patterns hintereinander zu einem ganzen Song.)",
            "The grid has 8 tracks and 64 rows. The cursor row stays in the\n"
            "centre and the pattern scrolls past it - just like the old\n"
            "ProTracker.\n\n"
            "Each track plays one note at the same moment. That's how you stack\n"
            "drums, bass and melody.\n\n"
            "(Soon: chaining several patterns into a whole song.)") });

    topics.add ({ loc::t ("Instrumente & Farben", "Instruments & colours"),
        loc::t (
            "Es gibt 16 Instrument-Slots. In jeden laedst du einen Sound (WAV,\n"
            "AIFF, FLAC, OGG, MP3 und das native Amiga-Format 8SVX/IFF) - entweder\n"
            "ueber SAMPLE LADEN von der Platte oder bequem ueber den SAMPLES-Browser.\n\n"
            "Jedes Instrument hat eine feste Farbe. Die Noten im Raster leuchten\n"
            "in der Farbe ihres Instruments, und der Farbpunkt neben der\n"
            "Instrument-Auswahl zeigt deine aktuelle \"Malfarbe\".",
            "There are 16 instrument slots. Load a sound into each (WAV, AIFF,\n"
            "FLAC, OGG, MP3 and the native Amiga format 8SVX/IFF) - either via\n"
            "LOAD SAMPLE from disk, or comfortably through the SAMPLES browser.\n\n"
            "Each instrument has a fixed colour. Notes in the grid glow in their\n"
            "instrument's colour, and the colour dot next to the instrument\n"
            "selector shows your current \"paint colour\".") });

    topics.add ({ loc::t ("Klang & Stereo", "Sound & stereo"),
        loc::t (
            "RetroTrax verteilt die 8 Spuren automatisch im Stereobild - leicht\n"
            "nach links und rechts, abwechselnd (das LRRL-Muster der Amiga-Tracker).\n"
            "So klingt dein Beat von allein breit und lebendig, ohne dass du etwas\n"
            "einstellen musst. Das Vorhoeren beim Tippen bleibt mittig.\n\n"
            "Jede Note wird ausserdem winzig kurz ein- und am Ende wieder\n"
            "ausgeblendet. Dadurch knackt nichts, auch wenn du schnell tippst oder\n"
            "viele Noten dicht hintereinander setzt.\n\n"
            "Die Lautstaerke-Spalte (00-64) regelt einzelne Noten leiser: 64 ist\n"
            "voll, 32 etwa halb so laut, 00 still.",
            "RetroTrax spreads the 8 tracks across the stereo field automatically -\n"
            "slightly left and right, alternating (the LRRL pattern of the Amiga\n"
            "trackers). Your beat sounds wide and lively on its own, with nothing\n"
            "to set up. Previewing while typing stays centred.\n\n"
            "Every note also gets a tiny fade in and out. That keeps things from\n"
            "clicking, even when you type fast or place many notes close together.\n\n"
            "The volume column (00-64) makes individual notes quieter: 64 is full,\n"
            "32 about half, 00 silent.") });

    topics.add ({ loc::t ("Sample-Browser", "Sample browser"),
        loc::t (
            "Der SAMPLES-Knopf oeffnet den Browser mit den legendaeren ST-XX\n"
            "Amiga-Sample-Disketten (96 Disks, ~5.900 Sounds, Public Domain).\n\n"
            "- Links waehlst du eine Diskette oder einen eigenen Ordner.\n"
            "- Ein Klick auf ein Sample spielt es sofort vor.\n"
            "- IN SLOT LADEN (oder Doppelklick) legt es in den aktuellen Slot.\n"
            "- Das Suchfeld oben durchsucht ALLE Disks und Ordner auf einmal.\n"
            "- * hinter einem Namen heisst: schon heruntergeladen (laedt sofort).\n\n"
            "Mit + ORDNER bindest du eigene Sample-Ordner von der Festplatte ein\n"
            "(sie bleiben nach Neustart erhalten). ENTF nimmt einen wieder raus -\n"
            "deine Dateien bleiben dabei unangetastet.",
            "The SAMPLES button opens the browser with the legendary ST-XX Amiga\n"
            "sample disks (96 disks, ~5,900 sounds, public domain).\n\n"
            "- On the left you pick a disk or one of your own folders.\n"
            "- Clicking a sample plays it instantly.\n"
            "- LOAD INTO SLOT (or double-click) puts it in the current slot.\n"
            "- The search box at the top searches ALL disks and folders at once.\n"
            "- A * after a name means: already downloaded (loads instantly).\n\n"
            "With + FOLDER you add your own sample folders from disk (they stay\n"
            "after a restart). REMOVE takes one out again - your files are left\n"
            "untouched.") });

    topics.add ({ loc::t ("Meine Sounds (Sammlung)", "My Sounds (collection)"),
        loc::t (
            "Ganz oben in der Ordnerliste steht immer \"Meine Sounds\" - deine\n"
            "persoenliche Sammlung.\n\n"
            "Gefaellt dir ein Sound (egal ob von einer ST-Disk oder aus einem\n"
            "eigenen Ordner)? Waehl ihn an und druecke MERKEN. Schon liegt er\n"
            "dauerhaft in deiner Sammlung - so baust du dir nach und nach deine\n"
            "eigene Lieblings-Bibliothek auf.\n\n"
            "Brauchst du einen Sound nicht mehr? Oeffne links \"Meine Sounds\",\n"
            "waehl den Sound an - der MERKEN-Knopf heisst dort jetzt VERGESSEN.\n"
            "Druecken, und der Sound wandert in den Papierkorb (ein Fehlklick ist\n"
            "also nicht endgueltig). ENTF ist nur fuer eigene Ordner da.\n\n"
            "Aufraeumen im Schnelldurchlauf: Mit Strg- oder Shift-Klick markierst\n"
            "du mehrere Sounds und VERGESSEN raeumt sie alle auf einmal weg. Nach\n"
            "dem Loeschen springt die Auswahl von selbst zum naechsten Sound -\n"
            "so klickst du dich zuegig durch.",
            "At the very top of the folder list there's always \"My Sounds\" -\n"
            "your personal collection.\n\n"
            "Like a sound (whether from an ST disk or one of your own folders)?\n"
            "Select it and press REMEMBER. It's stored in your collection for\n"
            "good - that's how you gradually build your own favourites library.\n\n"
            "Don't need a sound anymore? Open \"My Sounds\" on the left, select the\n"
            "sound - the REMEMBER button is labelled FORGET there. Press it and the\n"
            "sound goes to the trash (so a misclick isn't final). REMOVE is only\n"
            "for your own folders.\n\n"
            "Tidying up fast: Ctrl- or Shift-click marks several sounds and FORGET\n"
            "clears them all at once. After deleting, the selection jumps to the\n"
            "next sound by itself - so you can click through quickly.") });

    topics.add ({ loc::t ("Song speichern & oeffnen", "Save & open song"),
        loc::t (
            "Oben rechts: SONG SPEICHERN und SONG OEFFNEN.\n\n"
            "Deine Songs werden als .retrotrax-Datei gesichert (im Ordner\n"
            "Musik/RetroTrax). Gespeichert werden Tempo, alle Noten und welche\n"
            "Samples in den Slots liegen.\n\n"
            "Hinweis: Es werden die WEGE zu den Samples gemerkt, nicht die Klaenge\n"
            "selbst. Solange die Sample-Dateien an ihrem Platz bleiben, klingt\n"
            "dein Song beim Oeffnen wieder genau gleich. Fehlt mal ein Sample,\n"
            "sagt dir RetroTrax beim Oeffnen Bescheid, welches.",
            "Top right: SAVE SONG and OPEN SONG.\n\n"
            "Your songs are stored as a .retrotrax file (in the Music/RetroTrax\n"
            "folder). Saved are the tempo, all notes and which samples sit in the\n"
            "slots.\n\n"
            "Note: the PATHS to the samples are remembered, not the sounds\n"
            "themselves. As long as the sample files stay in place, your song\n"
            "sounds exactly the same when reopened. If a sample is missing,\n"
            "RetroTrax tells you which one on opening.") });

    topics.add ({ loc::t ("Sprache", "Language"),
        loc::t (
            "Der Knopf DE/EN oben schaltet zwischen Deutsch und Englisch um. Die\n"
            "Wahl wird gespeichert. Beim allerersten Start richtet RetroTrax sich\n"
            "nach der Sprache deines Systems.",
            "The DE/EN button at the top switches between German and English. Your\n"
            "choice is saved. On the very first start RetroTrax follows your\n"
            "system language.") });

    topics.add ({ loc::t ("Tipps & Dank", "Tips & thanks"),
        loc::t (
            "- Speichere oft. SONG SPEICHERN kostet nichts und schuetzt deine Arbeit.\n"
            "- Vorhoeren per Klick spart Zeit: erst stoebern, dann laden.\n"
            "- ESC schliesst Browser und Hilfe.\n\n"
            "RetroTrax ist und bleibt kostenlos. Wenn es dir Freude macht, freut\n"
            "sich das Mukkemann-Universum ueber einen Kaffee auf Ko-fi. Danke,\n"
            "dass du dabei bist! <3",
            "- Save often. SAVE SONG costs nothing and protects your work.\n"
            "- Previewing on click saves time: browse first, load second.\n"
            "- ESC closes the browser and this help.\n\n"
            "RetroTrax is and stays free. If it brings you joy, the Mukkemann\n"
            "universe would love a coffee on Ko-fi. Thank you for being here! <3") });

    currentTopic = juce::jlimit (0, topics.size() - 1, keep);
    topicList.updateContent();
    topicList.selectRow (currentTopic);
    showTopic (currentTopic);
}

void HelpPanel::showTopic (int index)
{
    if (index < 0 || index >= topics.size())
        return;
    currentTopic = index;
    bodyView.setText (topics.getReference (index).body, juce::dontSendNotification);
    bodyView.moveCaretToTop (false);
}

// ---- ListBox --------------------------------------------------------------

int HelpPanel::Model::getNumRows()
{
    return owner.topics.size();
}

void HelpPanel::Model::paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool selected)
{
    if (row < 0 || row >= owner.topics.size())
        return;
    if (selected)
        g.fillAll (rt::playBar);
    g.setFont (rt::mono (13.0f));
    g.setColour (selected ? juce::Colours::white : rt::text);
    g.drawText (owner.topics.getReference (row).title, 8, 0, w - 12, h,
                juce::Justification::centredLeft);
}

void HelpPanel::Model::selectedRowsChanged (int row)
{
    if (row >= 0)
        owner.showTopic (row);
}

// ---- Optik & Layout -------------------------------------------------------

bool HelpPanel::keyPressed (const juce::KeyPress& key)
{
    if (key.getKeyCode() == juce::KeyPress::escapeKey && onClose != nullptr)
    {
        onClose();
        return true;
    }
    return false;
}

void HelpPanel::paint (juce::Graphics& g)
{
    g.fillAll (rt::bg);
    g.setColour (rt::steel.withAlpha (0.7f));
    g.drawRect (getLocalBounds(), 1);

    g.setFont (rt::mono (15.0f, true));
    g.setColour (rt::cursor);
    g.drawText (loc::t ("HILFE", "HELP"), 12, 6, getWidth() - 24, 20,
                juce::Justification::centredLeft);

    g.setFont (rt::mono (12.0f));
    g.setColour (rt::textDim);
    g.drawText (loc::t ("Thema links waehlen - ESC schliesst.",
                        "Pick a topic on the left - ESC closes."),
                12, 26, getWidth() - 24, 16, juce::Justification::centredLeft);
}

void HelpPanel::resized()
{
    auto area = getLocalBounds().reduced (10);
    area.removeFromTop (38); // Titel (nur paint)

    auto bottom = area.removeFromBottom (30);
    closeButton.setBounds (bottom.removeFromRight (110).reduced (0, 2));
    area.removeFromBottom (6);

    topicList.setBounds (area.removeFromLeft (210));
    area.removeFromLeft (8);
    bodyView.setBounds (area);
}
