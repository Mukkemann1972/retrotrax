#pragma once

#include "PluginProcessor.h"
#include "RetroLookAndFeel.h"
#include "Localisation.h"
#include "Soundfont2.h"

// Browser fuer Samples aus zwei Quellen:
//  1. die klassischen Amiga-Sample-Disketten ST-01..ST-XX. Der Katalog
//     (Disk- und Sample-Namen) ist fest eingebaut (STDiskIndex.h); die Audio-
//     daten werden erst beim Laden einzeln von archive.org geholt (Public
//     Domain) und lokal gecacht - beim zweiten Mal geht es offline.
//  2. eigene Ordner auf der Festplatte (WAV/AIFF/FLAC/OGG/MP3). Diese werden
//     direkt geladen (kein Download) und die Ordnerliste ueberlebt Neustarts.
// Ein Suchfeld oben filtert ueber ALLE Quellen gleichzeitig.
class SampleDiskBrowser : public juce::Component,
                          private juce::URL::DownloadTaskListener
{
public:
    explicit SampleDiskBrowser (RetroTraxProcessor&);
    ~SampleDiskBrowser() override;

    // Wird nach erfolgreichem Laden gerufen (Sample-Name, Slot-Index).
    std::function<void (const juce::String&, int)> onSampleLoaded;
    std::function<void()> onClose;

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;

    // Nach einem Sprachwechsel aufrufen: Beschriftungen neu setzen.
    void applyLanguage();

    // Browser oeffnen und (falls ein eigener Ordner uebergeben wird) diesen als
    // Quelle aufnehmen + auswaehlen. Genutzt vom TFMX-Grabber, der seine
    // entnommenen Samples in einen Ordner schreibt und ihn gleich anzeigt.
    void showFolder (const juce::File& dir);

private:
    // Eine Quelle in der linken Liste: entweder eine ST-Diskette oder ein
    // eigener Ordner.
    struct Location
    {
        juce::String name;     // Anzeigename
        bool isLocal = false;  // true = eigener Ordner / Sammlung
        bool isSf2   = false;  // true = SoundFont-2-Bank (folder zeigt auf die .sf2-Datei)
        int diskIndex = -1;    // ST: Index in den Katalog (diskNames/diskSamples)
        juce::File folder;     // eigener Ordner ODER die .sf2-Datei
    };

    // Ein einzelnes Sample in der rechten Liste (Inhalt einer Quelle oder
    // ein Suchtreffer).
    struct Entry
    {
        int location = -1;     // Index in 'locations'
        juce::String name;     // Anzeigename des Samples
        juce::File localFile;  // nur bei eigenem Ordner gesetzt
        int sf2Index = -1;     // nur bei SF2: Index in currentSf2.samples
    };

    // Ein ListBoxModel pro Liste; beide malen im ProTracker-Stil.
    struct Model : juce::ListBoxModel
    {
        SampleDiskBrowser& owner;
        bool isDiskList;
        Model (SampleDiskBrowser& o, bool disks) : owner (o), isDiskList (disks) {}

        int getNumRows() override;
        void paintListBoxItem (int row, juce::Graphics&, int w, int h, bool selected) override;
        void selectedRowsChanged (int row) override;
        void listBoxItemDoubleClicked (int row, const juce::MouseEvent&) override;
    };

    void rebuildLocations();
    void locationSelected (int row);
    void rebuildEntries();
    void previewSelected (int row);
    void loadSelected();
    void addFolderClicked();
    void soundFontsClicked();        // SOUNDFONTS: Online-Katalog freier SF2-Banks oeffnen
    void startSf2Download (const juce::String& displayName, const juce::String& fileName);
    void removeFolderClicked();      // ENTF: nur eigene Ordner aus der Liste nehmen
    void updateButtons();            // MERKEN/VERGESSEN und ENTF je nach Lage setzen
    bool inCollectionView() const;   // true, wenn gerade "Meine Sounds" offen ist
    void collectionButtonClicked();  // MERKEN bzw. (in der Sammlung) VERGESSEN
    void deleteFromCollection();     // ausgewaehlten Sound aus "Meine Sounds" entfernen
    void saveToCollection();
    void selectLocationForFolder (const juce::File& dir);

    // Die immer vorhandene persoenliche Sammlung "Meine Sounds".
    juce::File collectionFolder() const;

    // SoundFont-2: Cache-WAV eines Bank-Samples; bei Bedarf erst herausziehen.
    juce::File sf2CacheFile (const juce::File& sf2File, const juce::String& sampleName) const;
    juce::File ensureSf2Cache (const Location& loc, const sf2::Sample& s);

    void finished (juce::URL::DownloadTask*, bool success) override;
    void finishLoad (const juce::File& file, bool success);
    void setStatus (const juce::String& text, bool warn = false);
    void setDefaultStatus();

    bool searching() const;
    juce::Array<juce::File> audioFilesIn (const juce::File& dir) const;

    juce::File cacheFileFor (int diskIdx, const juce::String& sampleName) const;
    juce::URL  urlFor (int diskIdx, const juce::String& sampleName) const;

    // Persistenz der eigenen Ordner (eine Pfadzeile pro Ordner).
    juce::File foldersFile() const;
    void loadFolders();
    void saveFolders();

    RetroTraxProcessor& proc;

    juce::StringArray diskNames;             // ST-Katalog
    juce::Array<juce::StringArray> diskSamples;
    juce::StringArray localFolders;          // gespeicherte eigene Ordner (Pfade)

    juce::Array<Location> locations;         // links: ST-Disks + eigene Ordner + SF2-Banks
    juce::Array<Entry>    currentEntries;    // rechts: Inhalt oder Suchtreffer
    int currentLocation = 0;
    sf2::Bank currentSf2;                    // Kopfdaten der gerade gewaehlten SF2-Bank

    Model diskModel   { *this, true };
    Model sampleModel { *this, false };
    juce::ListBox diskList   { "disks",   &diskModel };
    juce::ListBox sampleList { "samples", &sampleModel };

    juce::TextEditor searchBox;
    juce::TextButton addFolderButton    { "+ ORDNER" };
    juce::TextButton soundFontButton    { "SOUNDFONTS" };
    juce::TextButton removeFolderButton { "ENTF" };
    juce::TextButton saveButton         { "MERKEN" };
    juce::TextButton loadButton         { "IN SLOT LADEN" };
    juce::TextButton closeButton        { "SCHLIESSEN" };
    juce::Label statusLabel;

    std::unique_ptr<juce::FileChooser> chooser;

    std::unique_ptr<juce::URL::DownloadTask> task;
    juce::String pendingSample;       // Sample-Name (fuer Status/Callback)
    juce::String pendingLocationName; // Quelle des geladenen Samples
    int pendingDisk = 0;              // nur bei ST-Download relevant

    std::unique_ptr<juce::URL::DownloadTask> sf2Task; // Download einer ganzen SF2-Bank
    juce::File   pendingSf2File;      // Zielpfad der gerade geladenen SF2-Bank
    juce::String pendingSf2Name;      // Anzeigename fuer die Statuszeile

    std::unique_ptr<juce::URL::DownloadTask> previewTask; // Download nur fuers Vorhoeren

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SampleDiskBrowser)
};
