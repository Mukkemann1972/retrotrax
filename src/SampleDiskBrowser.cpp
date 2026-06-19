#include "SampleDiskBrowser.h"
#include "STDiskIndex.h"
#include "Sf2Catalog.h"
#include <algorithm>

// Archive.org liefert einzelne Dateien direkt aus dem ZIP aus; der Pfad
// innerhalb des ZIPs wird dabei %2F-kodiert an die ZIP-URL angehaengt.
static const char* const kArchiveBase =
    "https://archive.org/download/AmigaSTXX_originals_plus_conversions/ST-XX.zip/";

// Name der immer vorhandenen persoenlichen Sammlung (auch der Ordnername).
static const char* const kCollectionName = "Meine Sounds";

// Dateiendungen, die als Audio in eigenen Ordnern erkannt werden.
static const char* const kAudioWildcard = "*.wav;*.aif;*.aiff;*.flac;*.ogg;*.mp3;*.iff;*.8svx;*.svx";

SampleDiskBrowser::SampleDiskBrowser (RetroTraxProcessor& p) : proc (p)
{
    for (int i = 0; i < stdisk::kNumDisks; ++i)
    {
        juce::StringArray parts = juce::StringArray::fromTokens (stdisk::kIndex[i], "|", {});
        if (parts.size() < 2)
            continue;
        diskNames.add (parts[0]);
        parts.remove (0);
        diskSamples.add (parts);
    }

    for (auto* lb : { &diskList, &sampleList })
    {
        lb->setColour (juce::ListBox::backgroundColourId, rt::rowBeat);
        lb->setColour (juce::ListBox::outlineColourId, rt::steel.withAlpha (0.5f));
        lb->setOutlineThickness (1);
        lb->setRowHeight (20);
        addAndMakeVisible (*lb);
    }

    // In der Sample-Liste duerfen mehrere Eintraege markiert werden (Strg-/Shift-
    // Klick) - praktisch, um in "Meine Sounds" mehrere auf einmal zu VERGESSEN.
    sampleList.setMultipleSelectionEnabled (true);

    searchBox.setMultiLine (false);
    searchBox.setReturnKeyStartsNewLine (false);
    searchBox.setFont (rt::mono (13.0f));
    searchBox.setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xff262c38));
    searchBox.setColour (juce::TextEditor::textColourId, rt::text);
    searchBox.setColour (juce::TextEditor::outlineColourId, rt::steel.withAlpha (0.5f));
    searchBox.setColour (juce::TextEditor::focusedOutlineColourId, rt::cursor);
    searchBox.onTextChange = [this] { rebuildEntries(); };
    searchBox.onEscapeKey  = [this]
    {
        if (searchBox.getText().isNotEmpty())
        {
            searchBox.setText ({}, juce::dontSendNotification);
            rebuildEntries();
        }
        else if (onClose)
            onClose();
    };
    addAndMakeVisible (searchBox);

    addAndMakeVisible (addFolderButton);
    addAndMakeVisible (soundFontButton);
    addAndMakeVisible (removeFolderButton);
    addAndMakeVisible (saveButton);
    addAndMakeVisible (loadButton);
    addAndMakeVisible (closeButton);
    addAndMakeVisible (statusLabel);

    addFolderButton.onClick    = [this] { addFolderClicked(); };
    soundFontButton.onClick    = [this] { soundFontsClicked(); };
    removeFolderButton.onClick = [this] { removeFolderClicked(); };
    saveButton.onClick         = [this] { collectionButtonClicked(); };
    loadButton.onClick         = [this] { loadSelected(); };
    closeButton.onClick        = [this] { if (onClose) onClose(); };

    setWantsKeyboardFocus (true);

    statusLabel.setFont (rt::mono (13.0f));

    applyLanguage(); // Beschriftungen + Platzhalter setzen

    loadFolders();
    rebuildLocations();
    diskList.selectRow (0); // -> locationSelected(0) -> rebuildEntries()
}

void SampleDiskBrowser::applyLanguage()
{
    searchBox.setTextToShowWhenEmpty (
        loc::t ("Suche in allen Disks & Ordnern ...", "Search all disks & folders ..."),
        rt::textDim);
    addFolderButton.setButtonText    (loc::t ("+ ORDNER", "+ FOLDER"));
    soundFontButton.setButtonText    (loc::t ("SOUNDFONTS", "SOUNDFONTS"));
    soundFontButton.setTooltip       (loc::t ("Freie SoundFont-Banks aus dem Netz holen",
                                              "Fetch free SoundFont banks from the web"));
    loadButton.setButtonText         (loc::t ("IN SLOT LADEN", "LOAD INTO SLOT"));
    closeButton.setButtonText        (loc::t ("SCHLIESSEN", "CLOSE"));
    repaint();
    rebuildEntries(); // Statuszeile in neuer Sprache
}

SampleDiskBrowser::~SampleDiskBrowser()
{
    task.reset(); // laufenden Download abbrechen, bevor der Listener stirbt
}

// ---- Listen-Modelle -------------------------------------------------------

int SampleDiskBrowser::Model::getNumRows()
{
    return isDiskList ? owner.locations.size()
                      : owner.currentEntries.size();
}

void SampleDiskBrowser::Model::paintListBoxItem (int row, juce::Graphics& g,
                                                 int w, int h, bool selected)
{
    if (selected)
        g.fillAll (rt::playBar);

    g.setFont (rt::mono (13.0f));

    if (isDiskList)
    {
        if (row < 0 || row >= owner.locations.size())
            return;

        const auto& loc = owner.locations.getReference (row);
        // Eigene Ordner (gruen) und SF2-Banks (Akzentfarbe) heben sich von den
        // ST-Disketten ab.
        const auto colour = selected ? juce::Colours::white
                                      : (loc.isSf2 ? rt::cursor
                                                   : (loc.isLocal ? rt::instCol : rt::text));
        g.setColour (colour);
        g.drawText (loc.name, 8, 0, w - 12, h, juce::Justification::centredLeft);
        return;
    }

    if (row < 0 || row >= owner.currentEntries.size())
        return;

    const auto& e   = owner.currentEntries.getReference (row);
    const auto& loc = owner.locations.getReference (e.location);

    juce::String text = e.name;
    // "*" = schon lokal vorhanden / herausgezogen -> laedt sofort.
    if (! loc.isLocal && ! loc.isSf2 && loc.diskIndex >= 0
        && owner.cacheFileFor (loc.diskIndex, e.name).existsAsFile())
        text << "  *";
    else if (loc.isSf2 && owner.sf2CacheFile (loc.folder, e.name).existsAsFile())
        text << "  *";

    g.setColour (selected ? juce::Colours::white : rt::text);

    // Bei einer Suche zeigt jede Zeile rechts ihre Quelle (Disk/Ordner).
    if (owner.searching())
    {
        g.drawText (text, 8, 0, w - 12, h, juce::Justification::centredLeft);
        g.setFont (rt::mono (11.0f));
        g.setColour (selected ? juce::Colours::white.withAlpha (0.8f) : rt::textDim);
        g.drawText (loc.name, 8, 0, w - 12, h, juce::Justification::centredRight);
    }
    else
    {
        g.drawText (text, 8, 0, w - 12, h, juce::Justification::centredLeft);
    }
}

void SampleDiskBrowser::Model::selectedRowsChanged (int row)
{
    if (isDiskList && row >= 0)
        owner.locationSelected (row);
    else if (! isDiskList)
        owner.previewSelected (row); // angewaehltes Sample sofort anspielen
}

void SampleDiskBrowser::Model::listBoxItemDoubleClicked (int row, const juce::MouseEvent&)
{
    if (! isDiskList)
    {
        owner.sampleList.selectRow (row);
        owner.loadSelected();
    }
}

// ---- Quellen (links) ------------------------------------------------------

bool SampleDiskBrowser::searching() const
{
    return searchBox.getText().trim().isNotEmpty();
}

void SampleDiskBrowser::rebuildLocations()
{
    locations.clear();

    // ST-Disketten zuerst ...
    for (int i = 0; i < diskNames.size(); ++i)
        locations.add ({ diskNames[i], false, false, i, {} });

    // ... dann die persoenliche Sammlung (immer vorhanden) ...
    const auto coll = collectionFolder();
    coll.createDirectory();
    locations.add ({ juce::String (juce::CharPointer_UTF8 ("\xe2\x98\x85 ")) + kCollectionName,
                     true, false, -1, coll });

    // ... und zuletzt die selbst hinzugefuegten Quellen: Ordner oder SF2-Banks.
    for (const auto& path : localFolders)
    {
        juce::File f (path);
        if (f.isDirectory())
            locations.add ({ f.getFileName(), true, false, -1, f });
        else if (f.existsAsFile() && f.hasFileExtension ("sf2"))
            locations.add ({ "SF2: " + f.getFileNameWithoutExtension(), false, true, -1, f });
    }

    diskList.updateContent();
    diskList.repaint();
}

void SampleDiskBrowser::locationSelected (int row)
{
    if (row < 0 || row >= locations.size())
        return;

    currentLocation = row;

    // Beim Anwaehlen einer SF2-Bank die Kopfdaten einlesen (schnell, ohne PCM).
    const auto& loc = locations.getReference (row);
    currentSf2 = loc.isSf2 ? sf2::readBank (loc.folder) : sf2::Bank();

    searchBox.setText ({}, juce::dontSendNotification); // Disk-Wahl beendet die Suche
    rebuildEntries(); // ruft am Ende updateButtons()
}

juce::Array<juce::File> SampleDiskBrowser::audioFilesIn (const juce::File& dir) const
{
    juce::Array<juce::File> out;
    if (! dir.isDirectory())
        return out;

    for (const auto& entry : juce::RangedDirectoryIterator (dir, false, kAudioWildcard,
                                                            juce::File::findFiles))
        out.add (entry.getFile());

    std::sort (out.begin(), out.end(), [] (const juce::File& a, const juce::File& b)
    {
        return a.getFileName().compareIgnoreCase (b.getFileName()) < 0;
    });
    return out;
}

void SampleDiskBrowser::rebuildEntries()
{
    currentEntries.clear();
    const auto q = searchBox.getText().trim();

    auto addLocal = [this] (int loc, const juce::File& dir)
    {
        for (const auto& f : audioFilesIn (dir))
            currentEntries.add ({ loc, f.getFileNameWithoutExtension(), f });
    };

    if (q.isNotEmpty())
    {
        // Suche ueber ALLE Quellen.
        for (int loc = 0; loc < locations.size(); ++loc)
        {
            const auto& L = locations.getReference (loc);
            if (L.isLocal)
            {
                for (const auto& f : audioFilesIn (L.folder))
                    if (f.getFileNameWithoutExtension().containsIgnoreCase (q))
                        currentEntries.add ({ loc, f.getFileNameWithoutExtension(), f });
            }
            else if (! L.isSf2) // ST-Disketten; SF2-Banks bleiben aus der Suche heraus
            {
                for (const auto& n : diskSamples[L.diskIndex])
                    if (n.containsIgnoreCase (q))
                        currentEntries.add ({ loc, n, {} });
            }
        }
    }
    else if (currentLocation >= 0 && currentLocation < locations.size())
    {
        const auto& L = locations.getReference (currentLocation);
        if (L.isLocal)
            addLocal (currentLocation, L.folder);
        else if (L.isSf2)
            for (int i = 0; i < currentSf2.samples.size(); ++i)
                currentEntries.add ({ currentLocation, currentSf2.samples[i].name, {}, i });
        else
            for (const auto& n : diskSamples[L.diskIndex])
                currentEntries.add ({ currentLocation, n, {} });
    }

    sampleList.deselectAllRows();
    sampleList.updateContent();
    sampleList.scrollToEnsureRowIsOnscreen (0);
    sampleList.repaint();
    updateButtons(); // nach Listenwechsel: MERKEN/VERGESSEN + ENTF passend setzen

    // Statuszeile passend zur Lage.
    if (q.isNotEmpty())
    {
        if (currentEntries.isEmpty())
            setStatus (loc::t ("Nichts gefunden fuer \"", "No matches for \"") + q + "\".", true);
        else
            setStatus (juce::String (currentEntries.size())
                       + loc::t (" Treffer fuer \"", " matches for \"") + q
                       + loc::t ("\" - anklicken hoert vor, dann IN SLOT LADEN.",
                                 "\" - click to preview, then LOAD INTO SLOT."));
    }
    else if (currentLocation >= 0 && currentLocation < locations.size())
    {
        const auto& L = locations.getReference (currentLocation);
        if (L.isSf2 && currentEntries.isEmpty())
            setStatus (loc::t ("Diese SF2-Bank enthaelt keine lesbaren Samples.",
                               "This SF2 bank has no readable samples."), true);
        else if (L.isLocal && currentEntries.isEmpty())
            setStatus (L.folder == collectionFolder()
                       ? loc::t ("Noch nichts gemerkt - Sample waehlen und MERKEN druecken.",
                                 "Nothing remembered yet - select a sample and press REMEMBER.")
                       : loc::t ("Ordner enthaelt keine Audiodateien (WAV/AIFF/FLAC/OGG/MP3/8SVX).",
                                 "Folder has no audio files (WAV/AIFF/FLAC/OGG/MP3/8SVX)."), true);
        else
            setDefaultStatus();
    }
}

// ---- Eigene Ordner & Sammlung ---------------------------------------------

juce::File SampleDiskBrowser::foldersFile() const
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
        .getChildFile ("MukkemannRetroTrax").getChildFile ("eigene-ordner.txt");
}

void SampleDiskBrowser::loadFolders()
{
    localFolders.clear();
    const auto f = foldersFile();
    if (! f.existsAsFile())
        return;

    for (auto line : juce::StringArray::fromLines (f.loadFileAsString()))
    {
        line = line.trim();
        const juce::File entry (line);
        const bool ok = line.isNotEmpty()
                        && (entry.isDirectory()
                            || (entry.existsAsFile() && entry.hasFileExtension ("sf2")));
        if (ok)
            localFolders.addIfNotAlreadyThere (line);
    }
}

void SampleDiskBrowser::saveFolders()
{
    const auto f = foldersFile();
    f.getParentDirectory().createDirectory();
    f.replaceWithText (localFolders.joinIntoString ("\n"));
}

juce::File SampleDiskBrowser::collectionFolder() const
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
        .getChildFile ("MukkemannRetroTrax").getChildFile (kCollectionName);
}

void SampleDiskBrowser::selectLocationForFolder (const juce::File& dir)
{
    for (int i = 0; i < locations.size(); ++i)
    {
        const auto& loc = locations.getReference (i);
        if ((loc.isLocal || loc.isSf2) && loc.folder == dir)
        {
            diskList.selectRow (i);
            return;
        }
    }
}

void SampleDiskBrowser::showFolder (const juce::File& dir)
{
    if (dir.isDirectory())
    {
        localFolders.addIfNotAlreadyThere (dir.getFullPathName());
        saveFolders();
        rebuildLocations();
        selectLocationForFolder (dir);
    }
    setVisible (true);
    toFront (false);
}

void SampleDiskBrowser::addFolderClicked()
{
    chooser = std::make_unique<juce::FileChooser> (
        loc::t ("Ordner mit Samples oder eine SF2-Bank waehlen",
                "Choose a folder of samples or an SF2 bank"),
        juce::File::getSpecialLocation (juce::File::userMusicDirectory),
        "*.sf2");

    chooser->launchAsync (juce::FileBrowserComponent::openMode
                              | juce::FileBrowserComponent::canSelectDirectories
                              | juce::FileBrowserComponent::canSelectFiles,
        [this] (const juce::FileChooser& fc)
        {
            const auto result = fc.getResult();
            // Ordner ODER eine .sf2-Datei sind erlaubt.
            const bool ok = result.isDirectory()
                            || (result.existsAsFile() && result.hasFileExtension ("sf2"));
            if (! ok)
                return;

            localFolders.addIfNotAlreadyThere (result.getFullPathName());
            saveFolders();
            rebuildLocations();
            selectLocationForFolder (result);
        });
}

// Wohin heruntergeladene SoundFont-Banks gecacht werden.
static juce::File sf2DownloadDir()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
        .getChildFile ("MukkemannRetroTrax").getChildFile ("SoundFonts");
}

void SampleDiskBrowser::soundFontsClicked()
{
    const auto banks = sf2cat::banks();

    juce::PopupMenu menu;
    menu.addSectionHeader (loc::t ("Freie SoundFont-Banks - werden bei Bedarf geladen",
                                   "Free SoundFont banks - downloaded on demand"));

    for (int i = 0; i < (int) banks.size(); ++i)
    {
        const auto& b = banks[(size_t) i];
        const juce::String mb (b.sizeKb / 1024.0, b.sizeKb >= 1024 ? 1 : 2);
        const auto label = juce::String (b.name) + "   (" + mb + " MB)";
        const bool cached = sf2DownloadDir().getChildFile (b.file).existsAsFile();
        menu.addItem (i + 1, label, true, cached); // Haken = schon im Cache
    }

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (soundFontButton),
        [this, banks] (int result)
        {
            if (result <= 0 || result > (int) banks.size())
                return;
            const auto& b = banks[(size_t) (result - 1)];
            startSf2Download (b.name, b.file);
        });
}

void SampleDiskBrowser::startSf2Download (const juce::String& displayName,
                                          const juce::String& fileName)
{
    if (sf2Task != nullptr)
    {
        setStatus (loc::t ("Es laeuft schon ein SoundFont-Download ...",
                           "A SoundFont download is already running ..."), true);
        return;
    }

    auto dir = sf2DownloadDir();
    dir.createDirectory();
    const auto target = dir.getChildFile (fileName);

    // Schon im Cache: einfach als Quelle eintragen und links anwaehlen.
    if (target.existsAsFile() && target.getSize() >= 64)
    {
        localFolders.addIfNotAlreadyThere (target.getFullPathName());
        saveFolders();
        rebuildLocations();
        selectLocationForFolder (target);
        setStatus (displayName + loc::t (" ist bereit.", " is ready."));
        return;
    }

    pendingSf2File = target;
    pendingSf2Name = displayName;

    setStatus (loc::t ("Lade SoundFont ", "Downloading SoundFont ") + displayName
               + loc::t (" von archive.org ... (kann dauern)",
                         " from archive.org ... (may take a while)"));
    soundFontButton.setEnabled (false);

    // Erst in eine .part-Datei - abgebrochene Downloads landen nie im Cache.
    juce::URL url (juce::String (sf2cat::baseUrl())
                   + juce::URL::addEscapeChars (fileName, false));

    sf2Task = url.downloadToFile (juce::File (target.getFullPathName() + ".part"),
                                  juce::URL::DownloadTaskOptions().withListener (this));

    if (sf2Task == nullptr)
    {
        soundFontButton.setEnabled (true);
        setStatus (loc::t ("Download konnte nicht gestartet werden (Internet?).",
                           "Could not start the download (internet?)."), true);
    }
}

bool SampleDiskBrowser::inCollectionView() const
{
    if (searching() || currentLocation < 0 || currentLocation >= locations.size())
        return false;
    const auto& loc = locations.getReference (currentLocation);
    return loc.isLocal && loc.folder == collectionFolder();
}

void SampleDiskBrowser::updateButtons()
{
    const bool collection = inCollectionView();
    const bool hasSelection = sampleList.getSelectedRow() >= 0;

    // Der grosse Sammel-Knopf: in "Meine Sounds" wird MERKEN zu VERGESSEN
    // (loescht den gewaehlten Sound) - dort, wo man ohnehin hinschaut.
    saveButton.setButtonText (collection ? loc::t ("VERGESSEN", "FORGET")
                                         : loc::t ("MERKEN", "REMEMBER"));
    saveButton.setEnabled (collection ? hasSelection : true);

    // ENTF nimmt eine selbst hinzugefuegte Quelle (eigener Ordner ODER SF2-Bank)
    // aus der Liste - nicht die Sammlung, nicht die ST-Disks.
    bool removable = false;
    if (currentLocation >= 0 && currentLocation < locations.size())
    {
        const auto& loc = locations.getReference (currentLocation);
        removable = loc.isSf2 || (loc.isLocal && loc.folder != collectionFolder());
    }
    removeFolderButton.setButtonText (loc::t ("ENTF", "REMOVE"));
    removeFolderButton.setEnabled (removable);
}

void SampleDiskBrowser::collectionButtonClicked()
{
    if (inCollectionView())
        deleteFromCollection(); // VERGESSEN
    else
        saveToCollection();     // MERKEN
}

void SampleDiskBrowser::removeFolderClicked()
{
    if (currentLocation < 0 || currentLocation >= locations.size())
        return;

    const auto& loc = locations.getReference (currentLocation);
    const bool removable = loc.isSf2 || (loc.isLocal && loc.folder != collectionFolder());
    if (! removable)
        return; // Sammlung und ST-Disks bleiben

    // Nur aus der Liste nehmen - die Datei(en) auf der Platte bleiben unberuehrt.
    localFolders.removeString (loc.folder.getFullPathName());
    saveFolders();
    rebuildLocations();
    diskList.selectRow (0);
}

void SampleDiskBrowser::deleteFromCollection()
{
    if (! inCollectionView())
        return; // nur in "Meine Sounds" loeschen (nicht in der Suchansicht/auf Disks)

    // Alle markierten Zeilen einsammeln, BEVOR die Liste neu gebaut wird
    // (danach sind die Zeilennummern hinfaellig).
    const auto rows = sampleList.getSelectedRows();
    juce::Array<juce::File> files;
    juce::StringArray names;
    for (int i = 0; i < rows.size(); ++i)
    {
        const int row = rows[i];
        if (row < 0 || row >= currentEntries.size())
            continue;
        const auto& e = currentEntries.getReference (row);
        const auto& L = locations.getReference (e.location);
        if (L.isLocal && L.folder == collectionFolder() && e.localFile.existsAsFile())
        {
            files.add (e.localFile);
            names.add (e.name);
        }
    }

    if (files.isEmpty())
    {
        setStatus (loc::t ("Bitte erst einen Sound in der Sammlung auswaehlen.",
                           "Please select a sound in the collection first."), true);
        return;
    }

    const int firstRow = rows[0]; // hier wird nach dem Loeschen weitergemacht

    // In den Papierkorb statt hart loeschen - ein Fehlklick ist so nicht endgueltig.
    int removed = 0;
    for (auto& f : files)
        if (f.moveToTrash() || f.deleteFile())
            ++removed;

    rebuildEntries(); // Sammlung neu anzeigen

    // Auto-Weiterspringen: an gleicher Stelle den naechsten Sound markieren,
    // damit man mehrere flott nacheinander wegklicken kann.
    const int n = currentEntries.size();
    if (n > 0)
        sampleList.selectRow (juce::jlimit (0, n - 1, firstRow)); // -> spielt ihn vor, Knopf wieder aktiv

    if (removed == 1)
        setStatus ("\"" + names[0] + loc::t ("\" aus deiner Sammlung entfernt (liegt im Papierkorb).",
                                             "\" removed from your collection (now in the trash)."));
    else
        setStatus (juce::String (removed)
                   + loc::t (" Sounds aus deiner Sammlung entfernt (liegen im Papierkorb).",
                             " sounds removed from your collection (now in the trash)."));
}

void SampleDiskBrowser::saveToCollection()
{
    const int row = sampleList.getSelectedRow();
    if (row < 0 || row >= currentEntries.size())
    {
        setStatus (loc::t ("Bitte erst ein Sample auswaehlen.", "Please select a sample first."), true);
        return;
    }

    const auto e   = currentEntries.getReference (row);
    const auto& L  = locations.getReference (e.location);

    // Schon in der Sammlung? Dann nichts zu tun.
    if (L.isLocal && L.folder == collectionFolder())
    {
        setStatus ("\"" + e.name + loc::t ("\" ist schon in deiner Sammlung.",
                                            "\" is already in your collection."));
        return;
    }

    // Quelldatei finden (eigener Ordner: direkt; ST: aus dem Cache;
    // SF2: das Sample wird als WAV herausgezogen).
    juce::File source;
    if (L.isLocal)
        source = e.localFile;
    else if (L.isSf2)
        source = (e.sf2Index >= 0 && e.sf2Index < currentSf2.samples.size())
                     ? ensureSf2Cache (L, currentSf2.samples[e.sf2Index]) : juce::File();
    else
        source = cacheFileFor (L.diskIndex, e.name);

    if (! source.existsAsFile())
    {
        // ST-Sample noch nicht geladen: erst anklicken (hoert vor & laedt herunter).
        setStatus (loc::t ("Sample erst anklicken (laedt es herunter), dann MERKEN.",
                           "Click the sample first (downloads it), then REMEMBER."), true);
        return;
    }

    const auto coll = collectionFolder();
    coll.createDirectory();

    // Dateinamen mit Quelle versehen, damit gleich benannte Sounds nicht kollidieren.
    juce::String base = L.isLocal ? e.name : (L.name + " - " + e.name);
    base = juce::File::createLegalFileName (base);
    auto target = coll.getChildFile (base + source.getFileExtension());
    for (int n = 2; target.existsAsFile(); ++n)
        target = coll.getChildFile (base + " (" + juce::String (n) + ")" + source.getFileExtension());

    if (source.copyFileTo (target))
    {
        setStatus ("\"" + e.name + loc::t ("\" in deine Sammlung gemerkt.",
                                           "\" remembered into your collection."));
        // Falls die Sammlung gerade offen ist, frisch anzeigen.
        if (! searching() && locations.getReference (currentLocation).folder == coll)
            rebuildEntries();
    }
    else
    {
        setStatus ("\"" + e.name + loc::t ("\" konnte nicht in die Sammlung kopiert werden.",
                                           "\" could not be copied into the collection."), true);
    }
}

// ---- ST-Download & Laden --------------------------------------------------

juce::File SampleDiskBrowser::cacheFileFor (int diskIdx, const juce::String& sampleName) const
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
        .getChildFile ("MukkemannRetroTrax").getChildFile ("ST-Disks")
        .getChildFile (diskNames[diskIdx]).getChildFile (sampleName + ".aiff");
}

// ---- SoundFont 2: Samples als WAV cachen ----------------------------------

// Schreibt einen Puffer als 16-Bit-WAV (Cache fuer SF2-Samples).
static bool writeWavFile (const juce::File& file, const juce::AudioBuffer<float>& buf, double rate)
{
    file.getParentDirectory().createDirectory();
    file.deleteFile();

    juce::WavAudioFormat fmt;
    std::unique_ptr<juce::FileOutputStream> os (file.createOutputStream());
    if (os == nullptr)
        return false;

    std::unique_ptr<juce::AudioFormatWriter> writer (
        fmt.createWriterFor (os.get(), rate, (unsigned int) buf.getNumChannels(), 16, {}, 0));
    if (writer == nullptr)
        return false;

    os.release(); // ab jetzt gehoert der Stream dem Writer
    return writer->writeFromAudioSampleBuffer (buf, 0, buf.getNumSamples());
}

juce::File SampleDiskBrowser::sf2CacheFile (const juce::File& sf2File,
                                            const juce::String& sampleName) const
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
        .getChildFile ("MukkemannRetroTrax").getChildFile ("SF2-Cache")
        .getChildFile (juce::File::createLegalFileName (sf2File.getFileNameWithoutExtension()))
        .getChildFile (juce::File::createLegalFileName (sampleName) + ".wav");
}

juce::File SampleDiskBrowser::ensureSf2Cache (const Location& loc, const sf2::Sample& s)
{
    auto cache = sf2CacheFile (loc.folder, s.name);
    if (cache.existsAsFile())
        return cache;

    juce::AudioBuffer<float> buf;
    double rate = 44100.0;
    if (! sf2::readSamplePCM (loc.folder, currentSf2, s, buf, rate))
        return {};
    if (! writeWavFile (cache, buf, rate))
        return {};
    return cache;
}

juce::URL SampleDiskBrowser::urlFor (int diskIdx, const juce::String& sampleName) const
{
    const auto esc = [] (const juce::String& s)
    {
        return juce::URL::addEscapeChars (s, false);
    };
    return juce::URL (juce::String (kArchiveBase)
                      + "ST-XX%2F" + esc (diskNames[diskIdx])
                      + "%2F" + esc (sampleName) + ".aiff");
}

void SampleDiskBrowser::previewSelected (int row)
{
    updateButtons(); // VERGESSEN nur aktiv, wenn in der Sammlung wirklich etwas gewaehlt ist
    if (row < 0 || row >= currentEntries.size())
        return;

    previewTask.reset(); // alten Vorschau-Download abbrechen (schnelles Durchblaettern)

    const auto e  = currentEntries.getReference (row);
    const auto& L = locations.getReference (e.location);

    if (L.isLocal)
    {
        if (e.localFile.existsAsFile())
            proc.previewFile (e.localFile);
        return;
    }

    if (L.isSf2)
    {
        // Sample aus der Bank ziehen (als WAV cachen) und vorspielen.
        if (e.sf2Index >= 0 && e.sf2Index < currentSf2.samples.size())
        {
            const auto wav = ensureSf2Cache (L, currentSf2.samples[e.sf2Index]);
            if (wav.existsAsFile())
            {
                proc.previewFile (wav);
                sampleList.repaint(); // Sternchen aktualisieren
            }
            else
                setStatus (loc::t ("Dieses SF2-Sample konnte nicht gelesen werden.",
                                   "This SF2 sample could not be read."), true);
        }
        return;
    }

    const auto file = cacheFileFor (L.diskIndex, e.name);
    if (file.existsAsFile())
    {
        proc.previewFile (file);
        return;
    }

    // Noch nicht im Cache: im Hintergrund holen, dann anspielen.
    // Erst in eine .part-Datei - abgebrochene Downloads landen nie im Cache.
    file.getParentDirectory().createDirectory();
    setStatus (loc::t ("Hole ", "Fetching ") + e.name + loc::t (" zum Vorhoeren ...", " to preview ..."));
    previewTask = urlFor (L.diskIndex, e.name)
                      .downloadToFile (juce::File (file.getFullPathName() + ".part"),
                                       juce::URL::DownloadTaskOptions().withListener (this));
}

void SampleDiskBrowser::loadSelected()
{
    if (task != nullptr)
        return; // es laeuft schon ein Download

    previewTask.reset(); // Vorschau-Download nicht mit dem Slot-Download kreuzen

    const int row = sampleList.getSelectedRow();
    if (row < 0 || row >= currentEntries.size())
    {
        setStatus (loc::t ("Bitte erst ein Sample auswaehlen.", "Please select a sample first."), true);
        return;
    }

    const auto e  = currentEntries.getReference (row);
    const auto& L = locations.getReference (e.location);

    pendingSample       = e.name;
    pendingLocationName = L.name;

    // Eigener Ordner: direkt aus der Datei laden, kein Download.
    if (L.isLocal)
    {
        finishLoad (e.localFile, e.localFile.existsAsFile());
        return;
    }

    // SF2: Sample als WAV herausziehen und wie eine Datei laden.
    if (L.isSf2)
    {
        juce::File wav;
        if (e.sf2Index >= 0 && e.sf2Index < currentSf2.samples.size())
            wav = ensureSf2Cache (L, currentSf2.samples[e.sf2Index]);
        finishLoad (wav, wav.existsAsFile());
        return;
    }

    pendingDisk = L.diskIndex;

    const auto target = cacheFileFor (pendingDisk, pendingSample);
    if (target.existsAsFile())
    {
        finishLoad (target, true);
        return;
    }

    target.getParentDirectory().createDirectory();
    setStatus (loc::t ("Lade ", "Loading ") + pendingSample + loc::t (" von archive.org ...", " from archive.org ..."));
    loadButton.setEnabled (false);

    task = urlFor (pendingDisk, pendingSample)
               .downloadToFile (juce::File (target.getFullPathName() + ".part"),
                                juce::URL::DownloadTaskOptions().withListener (this));

    if (task == nullptr)
    {
        loadButton.setEnabled (true);
        setStatus (loc::t ("Download konnte nicht gestartet werden (Internet?).",
                           "Could not start the download (internet?)."), true);
    }
}

void SampleDiskBrowser::finished (juce::URL::DownloadTask* t, bool success)
{
    // Kommt vom Download-Thread -> auf den Message-Thread wechseln.
    // (Der Pointer-Vergleich ist hier sicher: solange dieser Callback laeuft,
    // blockiert ein reset() des Tasks auf dem Message-Thread.)
    const auto part = t->getTargetLocation(); // die .part-Datei
    const bool ok   = success && ! t->hadError();
    const bool isPreview = (t == previewTask.get());
    const bool isSf2     = (t == sf2Task.get());

    juce::MessageManager::callAsync (
        [sp = juce::Component::SafePointer<SampleDiskBrowser> (this), part, ok, isPreview, isSf2]
        {
            if (sp == nullptr)
                return;

            const juce::File file (part.getFullPathName().upToLastOccurrenceOf (".part", false, false));
            const bool complete = ok && part.getSize() >= 64 && part.moveFileTo (file);
            if (! complete)
                part.deleteFile();

            if (isSf2)
            {
                sp->sf2Task.reset();
                sp->soundFontButton.setEnabled (true);
                if (complete)
                {
                    // Wie eine selbst hinzugefuegte SF2-Bank behandeln.
                    sp->localFolders.addIfNotAlreadyThere (file.getFullPathName());
                    sp->saveFolders();
                    sp->rebuildLocations();
                    sp->selectLocationForFolder (file);
                    sp->setStatus (sp->pendingSf2Name
                                   + loc::t (" geladen - links angewaehlt.",
                                             " downloaded - selected on the left."));
                }
                else
                {
                    sp->setStatus (loc::t ("SoundFont-Download fehlgeschlagen - Internet pruefen.",
                                           "SoundFont download failed - check your internet."), true);
                }
                return;
            }

            if (isPreview)
            {
                sp->previewTask.reset();
                if (complete)
                {
                    sp->proc.previewFile (file);
                    sp->sampleList.repaint(); // Cache-Sternchen aktualisieren
                    sp->setDefaultStatus();
                }
                else
                {
                    sp->setStatus (loc::t ("Vorhoeren fehlgeschlagen - Internetverbindung pruefen.",
                                           "Preview failed - check your internet connection."), true);
                }
                return;
            }

            sp->task.reset();
            sp->loadButton.setEnabled (true);
            sp->finishLoad (file, complete);
        });
}

void SampleDiskBrowser::finishLoad (const juce::File& file, bool success)
{
    if (! success || file.getSize() < 64)
    {
        if (! file.existsAsFile() || file.getSize() < 64)
            file.deleteFile(); // halbe Downloads nicht im Cache lassen
        setStatus (loc::t ("Laden fehlgeschlagen - Datei / Internetverbindung pruefen.",
                           "Loading failed - check the file / your internet connection."), true);
        return;
    }

    const int slot = proc.currentInstrument.load();
    if (! proc.loadInstrument (slot, file))
    {
        setStatus ("\"" + pendingSample + loc::t ("\" konnte nicht geladen werden.",
                                                  "\" could not be loaded."), true);
        return;
    }

    proc.engine.audition (60, slot); // gleich anspielen (C-5 = Originaltonhoehe)
    sampleList.repaint();            // Cache-Sternchen aktualisieren
    setStatus (pendingLocationName + "/" + pendingSample
               + loc::t (" in Slot ", " loaded into slot ")
               + juce::String::formatted ("%02d", slot + 1)
               + loc::t (" geladen.", "."));

    if (onSampleLoaded)
        onSampleLoaded (pendingSample, slot);
}

void SampleDiskBrowser::setStatus (const juce::String& text, bool warn)
{
    statusLabel.setColour (juce::Label::textColourId, warn ? rt::cursor : rt::textDim);
    statusLabel.setText (text, juce::dontSendNotification);
}

void SampleDiskBrowser::setDefaultStatus()
{
    setStatus (loc::t (
        "Anklicken hoert vor - IN SLOT LADEN (oder Doppelklick) | "
        "MERKEN sichert in deine Sammlung | ESC schliesst.",
        "Click to preview - LOAD INTO SLOT (or double-click) | "
        "REMEMBER saves to your collection | ESC closes."));
}

bool SampleDiskBrowser::keyPressed (const juce::KeyPress& key)
{
    if (key.getKeyCode() == juce::KeyPress::escapeKey && onClose != nullptr)
    {
        onClose();
        return true;
    }
    return false;
}

// ---- Optik ----------------------------------------------------------------

void SampleDiskBrowser::paint (juce::Graphics& g)
{
    g.fillAll (rt::bg);
    g.setColour (rt::steel.withAlpha (0.7f));
    g.drawRect (getLocalBounds(), 1);

    g.setFont (rt::mono (15.0f, true));
    g.setColour (rt::cursor);
    g.drawText (loc::t ("SAMPLE-BROWSER", "SAMPLE BROWSER"), 12, 6, getWidth() - 24, 20,
                juce::Justification::centredLeft);

    g.setFont (rt::mono (12.0f));
    g.setColour (rt::textDim);
    g.drawText (loc::t ("ST-XX Amiga-Sounds (Public Domain) + eigene Ordner - * = schon geladen",
                        "ST-XX Amiga sounds (public domain) + your own folders - * = already downloaded"),
                12, 26, getWidth() - 24, 16, juce::Justification::centredLeft);
}

void SampleDiskBrowser::resized()
{
    auto area = getLocalBounds().reduced (10);
    area.removeFromTop (38); // Titel (nur paint)

    // Suchfeld ganz oben, ueber die volle Breite.
    searchBox.setBounds (area.removeFromTop (26));
    area.removeFromTop (6);

    // Unterste Zeile: Status + Aktionsknoepfe.
    auto bottom = area.removeFromBottom (30);
    closeButton.setBounds (bottom.removeFromRight (110).reduced (0, 2));
    bottom.removeFromRight (8);
    loadButton.setBounds (bottom.removeFromRight (140).reduced (0, 2));
    bottom.removeFromRight (8);
    saveButton.setBounds (bottom.removeFromRight (96).reduced (0, 2));
    statusLabel.setBounds (bottom);

    area.removeFromBottom (6);

    // Linke Spalte: Quellenliste + Quellen-Knoepfe darunter (zwei Zeilen).
    auto left = area.removeFromLeft (168);
    auto folderRow = left.removeFromBottom (24);
    addFolderButton.setBounds (folderRow.removeFromLeft (100).reduced (0, 2));
    folderRow.removeFromLeft (4);
    removeFolderButton.setBounds (folderRow.reduced (0, 2));
    left.removeFromBottom (2);
    soundFontButton.setBounds (left.removeFromBottom (24).reduced (0, 2));
    left.removeFromBottom (4);
    diskList.setBounds (left);

    area.removeFromLeft (8);
    sampleList.setBounds (area);
}
