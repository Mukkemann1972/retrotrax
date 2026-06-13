#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "TrackerEngine.h"

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

    // Song als .retrotrax-Datei speichern bzw. oeffnen. loadSong sammelt in
    // 'missingSamples' die Namen der Samples, deren Datei nicht (mehr) da ist.
    bool saveSong (const juce::File& file);
    bool loadSong (const juce::File& file, juce::StringArray& missingSamples);

    TrackerEngine engine;
    std::atomic<int> currentInstrument { 0 };
    std::atomic<int> currentOctave { 5 };

private:
    std::unique_ptr<TrackerEngine::Instrument> createInstrument (const juce::File& file);

    // Gemeinsames Song-Format fuer Host-State und .retrotrax-Dateien.
    std::unique_ptr<juce::XmlElement> stateToXml();
    void applyStateXml (const juce::XmlElement&, juce::StringArray* missingSamples = nullptr);

    juce::AudioFormatManager formatManager;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RetroTraxProcessor)
};
