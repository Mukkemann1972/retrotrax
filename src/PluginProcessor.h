#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "TrackerEngine.h"
#include "TfmxPlayer.h"

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

    // TFMX (Chris Huelsbeck, Amiga) laden. Anders als MOD/XM kein Grid-Import,
    // sondern ein eigener Replayer (siehe TfmxPlayer). Die passende .smpl-Datei
    // wird per Namens-Konvention neben der .mdat gesucht. STUFE 1: liest + meldet
    // nur den Datei-Inhalt (Diagnose), Wiedergabe folgt. 'message' = Zusammenfassung.
    bool loadTfmx (const juce::File& mdatFile, juce::String& message);

    TrackerEngine engine;
    std::atomic<int> currentInstrument { 0 };
    std::atomic<int> currentOctave { 5 };

private:
    std::unique_ptr<TrackerEngine::Instrument> createInstrument (const juce::File& file);

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
