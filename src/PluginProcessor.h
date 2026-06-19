#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "TrackerEngine.h"
#include "TfmxPlayer.h"
#include "ImportCommon.h"

class RetroTraxProcessor : public juce::AudioProcessor
{
public:
    RetroTraxProcessor();
    ~RetroTraxProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Mukkemann RetroTrax"; }
    bool acceptsMidi() const override  { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // Laedt eine Audiodatei in einen Instrument-Slot. Liefert false bei Fehler.
    bool loadInstrument (int slot, const juce::File& file);

    // Spielt eine Audiodatei sofort an, ohne einen Slot zu belegen (Vorschau).
    bool previewFile (const juce::File& file);

    // --- SID-Synth-Instrumente ------------------------------------------------
    // Macht aus dem Slot ein frisches SID-Synth-Instrument (ersetzt den Inhalt).
    void makeSidInstrument (int slot);
    // true, wenn der Slot ein SID-Synth ist.
    bool isSid (int slot) const;
    // Liest die SID-Parameter eines Slots; false, wenn der Slot kein Synth ist.
    bool getSid (int slot, TrackerEngine::Instrument& out) const;
    // Veraendert die SID-Parameter eines Slots in-place (legt nichts neu an).
    void editSid (int slot, std::function<void (TrackerEngine::Instrument&)> fn);

    // --- Akai-Sampler-Filter (Sample-Instrumente) -----------------------------
    // true, wenn der Slot ein Sample mit Klangdaten ist (also filterbar).
    bool isSampleSlot (int slot) const;
    // Liest die Parameter eines Sample-Slots (Akai-Filter + Name); false sonst.
    bool getSample (int slot, TrackerEngine::Instrument& out) const;
    // Veraendert die Parameter eines Sample-Slots in-place (legt nichts neu an).
    void editSample (int slot, std::function<void (TrackerEngine::Instrument&)> fn);

    // --- Drum-Kit (16 Pads, MPC60/SP-1200-Stil) -----------------------------
    // Eigenstaendige 16er-Sample-Bank, getrennt von den Spur-Slots, aber gebrueckt:
    // ein Pad ist ein normales Instrument (Sample), frei zwischen Pad/Slot/Grabber
    // verschiebbar. Geladen wird wie ein Sample (createInstrument).
    bool loadPad (int pad, const juce::File& file);   // Datei in ein Pad laden
    void clearPad (int pad);                          // Pad leeren
    bool getPadName (int pad, juce::String& name) const;        // Anzeigename + ob belegt
    bool getPad (int pad, TrackerEngine::Instrument& out) const; // Charakter lesen (wie getSample)
    void editPad (int pad, std::function<void (TrackerEngine::Instrument&)> fn); // Charakter aendern
    bool padToSlot (int pad, int slot);  // Pad-Sample in einen Spur-Slot kopieren
    bool slotToPad (int slot, int pad);  // Spur-Slot-Sample in ein Pad kopieren

    // --- Fairlight-Sample-Werkzeug -------------------------------------------
    // Arbeitskopie eines Slot-Samples holen (zum Anzeigen/Bearbeiten im Editor).
    bool getSampleCopy (int slot, juce::AudioBuffer<float>& out, double& rate) const;
    // Eine bearbeitete Arbeitskopie als WAV sichern und in den Slot laden (so
    // bleibt sie im Song erhalten - wie beim Grabber, ueber Dateipfad).
    bool applyEditedSample (int slot, const juce::AudioBuffer<float>& buf, double rate,
                            juce::String& message);
    // Ein Sample in gleich grosse Scheiben schneiden und auf die Kit-Pads legen
    // (als WAVs gesichert). Liefert die Anzahl gefuellter Pads.
    int chopToKit (const juce::AudioBuffer<float>& buf, double rate, int slices,
                   const juce::String& baseName, juce::String& message);
    // Ein Sample in Scheiben schneiden, in aufeinanderfolgende Instrument-Slots
    // legen (ab dem aktuellen) UND als Noten ins aktuelle Pattern schreiben
    // (gleichmaessig verteilt, Spur 1) - der Break wird wieder spielbar/umbaubar
    // (Recycle/Page-R-Idee). Liefert die Anzahl Scheiben.
    int sliceToPattern (const juce::AudioBuffer<float>& buf, double rate, int slices,
                        const juce::String& baseName, juce::String& message);
    // Eine Arbeitskopie sofort vorhoeren (ohne einen Slot zu belegen).
    void previewBuffer (const juce::AudioBuffer<float>& buf, double rate);

    // Den Song offline als Stereo-WAV (16 Bit) rausrendern: im Song-Modus die
    // ganze Reihenfolge einmal, sonst das aktuelle Pattern einmal. Laeuft schneller
    // als Echtzeit (Audio waehrenddessen ausgesetzt). 'message' = Zusammenfassung.
    bool renderSongToWav (const juce::File& file, juce::String& message);

    // Song als .retrotrax-Datei speichern bzw. oeffnen. loadSong sammelt in
    // 'missingSamples' die Namen der Samples, deren Datei nicht (mehr) da ist.
    bool saveSong (const juce::File& file);
    bool loadSong (const juce::File& file, juce::StringArray& missingSamples);

    // Klassisches Amiga-MOD (.mod) importieren: Samples -> Instrument-Slots,
    // Pattern-Daten -> Patterns, Reihenfolge -> Order. 'message' bekommt eine
    // kurze Zusammenfassung (oder den Fehlergrund).
    bool loadMod (const juce::File& file, juce::String& message);

    // FastTracker-2-Modul (.xm) importieren - wie loadMod, nur fuer das groessere
    // XM-Format (mehr Kanaele, 16-Bit-Samples, Finetune/relative Note).
    bool loadXm (const juce::File& file, juce::String& message);

    // Scream Tracker 3 (.s3m) bzw. Impulse Tracker (.it) importieren - wie MOD/XM,
    // ueber den gemeinsamen Importer (S3mImport/ItImport -> applyImportedSong).
    bool loadS3m (const juce::File& file, juce::String& message);
    bool loadIt  (const juce::File& file, juce::String& message);

    // TFMX (Chris Huelsbeck, Amiga) laden. Anders als MOD/XM kein Grid-Import,
    // sondern ein eigener Replayer (siehe TfmxPlayer). Die passende .smpl-Datei
    // wird per Namens-Konvention neben der .mdat gesucht. STUFE 1: liest + meldet
    // nur den Datei-Inhalt (Diagnose), Wiedergabe folgt. 'message' = Zusammenfassung.
    bool loadTfmx (const juce::File& mdatFile, juce::String& message);

    // TFMX-GRABBER: alle Samples (Instrumente) aus einem TFMX-Modul entnehmen und
    // als einzelne 16-Bit-WAVs in 'outFolder' schreiben (Renoise-Plugin-Grabber-
    // Idee). Stoert die laufende Wiedergabe NICHT (eigener Reader). Liefert die
    // Anzahl geschriebener Samples; 'message' = Zusammenfassung/Fehler.
    int grabTfmxSamples (const juce::File& mdatFile, const juce::File& outFolder,
                         juce::String& message);

    TrackerEngine engine;
    std::atomic<int> currentInstrument { 0 };
    std::atomic<int> currentOctave { 5 };

    // --- Spektrum-Anzeige: Ringpuffer des Ausgangs-Mixes (Mono) -----------------
    // Der Audio-Thread schreibt den fertigen Stereo-Mix (gemittelt) hier hinein;
    // die SpectrumPanel-Anzeige liest im Timer den juengsten Ausschnitt und rechnet
    // die Frequenzbalken. Reine Anzeige, beeinflusst den Klang nicht. Groesse ist
    // eine Zweierpotenz (Maske statt Modulo). feedScope wird im processBlock gerufen.
    static constexpr int kScopeSize = 4096;
    float scope[kScopeSize] = {};
    std::atomic<int> scopePos { 0 };
    void feedScope (const juce::AudioBuffer<float>& buffer);

private:
    std::unique_ptr<TrackerEngine::Instrument> createInstrument (const juce::File& file);

    // Eine fertig geparste ImportCommon::Song in die Engine einsetzen (Samples ->
    // Slots, Patterns, Reihenfolge, Song-Modus). Gemeinsam fuer S3M und IT.
    bool applyImportedSong (const ImportCommon::Song& song, juce::String& message);

    // Sucht die .smpl-Begleitdatei zu einer .mdat (mdat.xxx<->smpl.xxx bzw.
    // xxx.mdat<->xxx.smpl, sonst "mdat" im Namen durch "smpl" ersetzen).
    static juce::File findTfmxSmpl (const juce::File& mdatFile);

    TfmxPlayer tfmx;

    // Wiedergabe-Pfad: normaler Tracker ODER der TFMX-Replayer. Laden eines TFMX
    // schaltet auf Tfmx, Laden von Song/MOD/XM/Sample zurueck auf Tracker.
    enum class PlaybackMode { Tracker, Tfmx };
    std::atomic<PlaybackMode> playbackMode { PlaybackMode::Tracker };
    bool tfmxWasPlaying = false; // nur Audio-Thread: Flanke fuer Neustart bei PLAY

    // Gemeinsames Song-Format fuer Host-State und .retrotrax-Dateien.
    std::unique_ptr<juce::XmlElement> stateToXml();
    void applyStateXml (const juce::XmlElement&, juce::StringArray* missingSamples = nullptr);

    juce::AudioFormatManager formatManager;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RetroTraxProcessor)
};
