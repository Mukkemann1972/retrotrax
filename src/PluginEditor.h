#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "PatternGrid.h"
#include "RetroLookAndFeel.h"
#include "SampleDiskBrowser.h"
#include "HelpPanel.h"
#include "SidPanel.h"
#include "DrumKitPanel.h"
#include "SampleEditPanel.h"
#include "FxPanel.h"
#include "SpectrumPanel.h"
#include "KeyboardPanel.h"
#include "SplashOverlay.h"
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
    void loadModClicked();
    void loadXmClicked();
    void loadS3mClicked();
    void loadItClicked();
    void loadTfmxClicked();
    void loadSidClicked();   // C64-SID (.sid) laden + Diagnose
    void grabTfmxClicked();   // TFMX-Grabber: Samples entnehmen -> eigener Ordner
    void syncUiFromState();
    void hideAllOverlays(); // alle Overlay-Panels schliessen (immer nur eins offen)
    void applyLanguage();
    void setDefaultHint(); // Standard-Tastenkuerzel-Zeile unten
    void updateSongUi();   // Pattern-Nummer, SONG/LOOP, Reihenfolge anzeigen
    juce::File songsFolder() const;

    RetroTraxProcessor& proc;
    RetroLookAndFeel lnf;

    juce::TextButton playButton { "PLAY" }; // Umschalter: PLAY <-> STOP
    juce::TextButton recButton  { "REC" };  // Aufnahme scharf schalten (rot, wenn aktiv)
    juce::TextButton loadMenuButton { "LADEN" }; // Aufklapp-Menue: Sample/Song laden + Importieren
    juce::TextButton sidButton { "SID" }; // aktuellen Slot zu einem SID-Synth machen
    juce::TextButton kitButton { "KIT" }; // Drum-Kit (16 Pads, MPC60/SP-1200-Stil)
    juce::TextButton editButton { "FAIRLIGHT" }; // Sample-Werkzeug (trimmen/zeichnen/choppen)
    juce::TextButton fxButton { "FX" }; // FX: Akai-Sampler-Effekte + Master-Effekte
    juce::TextButton saveSongButton { "SONG SPEICHERN" };
    juce::TextButton helpButton { "?" };
    juce::TextButton liveHelpButton { "TIPP" }; // Live-Hilfe-Zeile an/aus
    juce::TextButton spectrumButton { "SPEKTRUM" }; // Frequenz-Anzeige ein/aus
    juce::TextButton kbButton { "TASTEN" }; // Bildschirm-Tastatur (welche Taste = welche Note)
    juce::TextButton modeButton { "PROFI" }; // EINFACH <-> PROFI (Anfaenger/Fortgeschrittene)
    juce::TextButton langButton { "DE" };

    // Song-Modus-Leiste
    juce::TextButton patPrevButton  { "< PAT" };
    juce::TextButton patNextButton  { "PAT >" };
    juce::TextButton songModeButton { "LOOP" };  // LOOP <-> SONG
    juce::TextButton orderAddButton { "+ PAT" }; // aktuelles Pattern hinten anhaengen
    juce::TextButton orderDelButton { "- PAT" }; // letzten Eintrag entfernen
    juce::ComboBox   quantBox;                    // Quantisierungs-Raster (Achtel/Viertel/...)
    juce::TextButton quantButton { "QUANT" };     // aktuelles Pattern aufs Raster schnappen
    juce::TextButton randomButton { "WUERFEL" };  // Cursor-Spur mit Zufallsmelodie fuellen
    juce::Label patLabel;
    juce::Label orderLabel;
    juce::Slider bpmSlider;
    juce::Slider swingSlider; // Swing/Groove (0..80 %)
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
    DrumKitPanel kitPanel;
    SampleEditPanel editPanel;
    FxPanel fxPanel;
    SpectrumPanel spectrumPanel;
    KeyboardPanel kbPanel;
    SplashOverlay splash;
    std::unique_ptr<juce::FileChooser> chooser;
    std::unique_ptr<juce::FileChooser> songChooser;
    juce::File currentSongFile; // zuletzt gespeicherter/geoeffneter Song

    std::unique_ptr<juce::AlertWindow> startDialog; // Startabfrage "Weitermachen/leere Seite"
    void maybeAskFreshStart();                      // beim Standalone-Start ggf. fragen

    // Anfaenger-Modus (EINFACH) vs. Profi-Modus. Schaltet weniger Spalten im Grid,
    // blendet fortgeschrittene Knoepfe/Song-Leiste aus und oeffnet die Tastatur.
    bool beginnerMode = false;
    void applyBeginnerMode (bool justSwitchedOn);   // Oberflaeche an den Modus anpassen
    std::unique_ptr<juce::AlertWindow> firstStepsBox; // Erste-Schritte-Box (Anfaenger)

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RetroTraxEditor)
};
