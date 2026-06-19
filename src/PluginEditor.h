#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "PatternGrid.h"
#include "RetroLookAndFeel.h"
#include "SampleDiskBrowser.h"
#include "HelpPanel.h"
#include "SidPanel.h"
#include "AkaiPanel.h"
#include "DrumKitPanel.h"
#include "SampleEditPanel.h"
#include "SpectrumPanel.h"
#include "Localisation.h"

class RetroTraxEditor : public juce::AudioProcessorEditor
{
public:
    explicit RetroTraxEditor (RetroTraxProcessor&);
    ~RetroTraxEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void parentHierarchyChanged() override; // schaltet im Standalone das Maximieren-4eck frei

private:
    void refreshInstrumentBox();
    void updateTransportButtons();
    void loadSampleClicked();
    void saveSongClicked();
    void loadSongClicked();
    void exportWavClicked();
    void loadModClicked();
    void loadXmClicked();
    void loadS3mClicked();
    void loadItClicked();
    void loadTfmxClicked();
    void grabTfmxClicked();   // TFMX-Grabber: Samples entnehmen -> eigener Ordner
    void syncUiFromState();
    void applyLanguage();
    void setDefaultHint(); // Standard-Tastenkuerzel-Zeile unten
    void updateSongUi();   // Pattern-Nummer, SONG/LOOP, Reihenfolge anzeigen
    juce::File songsFolder() const;

    RetroTraxProcessor& proc;
    RetroLookAndFeel lnf;

    juce::TextButton playButton { "PLAY" };
    juce::TextButton stopButton { "STOP" };
    juce::TextButton loadMenuButton { "LADEN" }; // Aufklapp-Menue: Sample/Song laden + Importieren
    juce::TextButton sidButton { "SID" }; // aktuellen Slot zu einem SID-Synth machen
    juce::TextButton akaiButton { "AKAI" }; // Akai-Filter fuer den aktuellen Sample-Slot
    juce::TextButton kitButton { "KIT" }; // Drum-Kit (16 Pads, MPC60/SP-1200-Stil)
    juce::TextButton editButton { "FAIRLIGHT" }; // Sample-Werkzeug (trimmen/zeichnen/choppen)
    juce::TextButton saveSongButton { "SONG SPEICHERN" };
    juce::TextButton wavButton { "WAV" }; // Song als WAV-Datei rausrendern
    juce::TextButton helpButton { "?" };
    juce::TextButton liveHelpButton { "TIPP" }; // Live-Hilfe-Zeile an/aus
    juce::TextButton spectrumButton { "SPEKTRUM" }; // Frequenz-Anzeige ein/aus
    juce::TextButton langButton { "DE" };

    // Song-Modus-Leiste
    juce::TextButton patPrevButton  { "< PAT" };
    juce::TextButton patNextButton  { "PAT >" };
    juce::TextButton songModeButton { "LOOP" };  // LOOP <-> SONG
    juce::TextButton orderAddButton { "+ PAT" }; // aktuelles Pattern hinten anhaengen
    juce::TextButton orderDelButton { "- PAT" }; // letzten Eintrag entfernen
    juce::ComboBox   quantBox;                    // Quantisierungs-Raster (Achtel/Viertel/...)
    juce::TextButton quantButton { "QUANT" };     // aktuelles Pattern aufs Raster schnappen
    juce::Label patLabel;
    juce::Label orderLabel;
    juce::Slider bpmSlider;
    juce::ComboBox instrumentBox;
    juce::ComboBox octaveBox;
    juce::Label instLabel { {}, "INSTR" };
    juce::Label octLabel { {}, "OKTAVE" };
    juce::Label hintLabel;

    // Kleines Farbquadrat: zeigt die Farbe des gerade gewaehlten Instruments
    struct ColourDot : juce::Component
    {
        juce::Colour colour;
        void paint (juce::Graphics& g) override
        {
            const auto r = getLocalBounds().toFloat().reduced (2.0f);
            g.setColour (colour);
            g.fillRoundedRectangle (r, 4.0f);
            g.setColour (juce::Colour (0xff0e1118));
            g.drawRoundedRectangle (r, 4.0f, 1.2f);
        }
    };
    ColourDot instDot;

    PatternGrid grid;
    SampleDiskBrowser diskBrowser;
    HelpPanel helpPanel;
    SidPanel sidPanel;
    AkaiPanel akaiPanel;
    DrumKitPanel kitPanel;
    SampleEditPanel editPanel;
    SpectrumPanel spectrumPanel;
    std::unique_ptr<juce::FileChooser> chooser;
    std::unique_ptr<juce::FileChooser> songChooser;
    juce::File currentSongFile; // zuletzt gespeicherter/geoeffneter Song

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RetroTraxEditor)
};
