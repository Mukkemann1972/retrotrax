#include "PluginProcessor.h"
#include "PluginEditor.h"

RetroTraxProcessor::RetroTraxProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
    formatManager.registerBasicFormats();
}

void RetroTraxProcessor::prepareToPlay (double sampleRate, int)
{
    engine.prepare (sampleRate);
}

bool RetroTraxProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void RetroTraxProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    for (const auto metadata : midi)
    {
        const auto msg = metadata.getMessage();
        if (msg.isNoteOn())
            engine.audition (msg.getNoteNumber(), currentInstrument.load());
    }
    midi.clear();

    engine.process (buffer);
}

std::unique_ptr<TrackerEngine::Instrument> RetroTraxProcessor::createInstrument (const juce::File& file)
{
    std::unique_ptr<juce::AudioFormatReader> reader (formatManager.createReaderFor (file));
    if (reader == nullptr || reader->lengthInSamples < 2)
        return nullptr;

    // Sicherheitsgrenze: max. 60 Sekunden pro Sample, damit der Speicher nicht explodiert
    const auto maxLen = (juce::int64) (reader->sampleRate * 60.0);
    const int numSamples = (int) juce::jmin (reader->lengthInSamples, maxLen);

    auto inst = std::make_unique<TrackerEngine::Instrument>();
    inst->data.setSize ((int) reader->numChannels, numSamples);
    reader->read (&inst->data, 0, numSamples, 0, true, true);
    inst->sourceRate = reader->sampleRate;
    inst->name = file.getFileNameWithoutExtension();
    inst->filePath = file.getFullPathName();
    return inst;
}

bool RetroTraxProcessor::loadInstrument (int slot, const juce::File& file)
{
    if (slot < 0 || slot >= TrackerEngine::kInstruments)
        return false;

    auto inst = createInstrument (file);
    if (inst == nullptr)
        return false;

    engine.setInstrument (slot, std::move (inst));
    return true;
}

bool RetroTraxProcessor::previewFile (const juce::File& file)
{
    auto inst = createInstrument (file);
    if (inst == nullptr)
        return false;

    engine.previewInstrument (std::move (inst));
    return true;
}

std::unique_ptr<juce::XmlElement> RetroTraxProcessor::stateToXml()
{
    auto xml = std::make_unique<juce::XmlElement> ("RETROTRAX");
    xml->setAttribute ("version", 1);
    xml->setAttribute ("bpm", (double) engine.bpm.load());
    xml->setAttribute ("instrument", currentInstrument.load());
    xml->setAttribute ("octave", currentOctave.load());

    for (int i = 0; i < TrackerEngine::kInstruments; ++i)
    {
        const juce::ScopedLock sl (engine.lock);
        if (engine.instruments[i] != nullptr && engine.instruments[i]->filePath.isNotEmpty())
        {
            auto* e = xml->createNewChildElement ("INSTRUMENT");
            e->setAttribute ("slot", i);
            e->setAttribute ("name", engine.instruments[i]->name);
            e->setAttribute ("path", engine.instruments[i]->filePath);
        }
    }

    for (int r = 0; r < TrackerEngine::kRows; ++r)
    {
        for (int t = 0; t < TrackerEngine::kTracks; ++t)
        {
            const auto& c = engine.cells[r][t];
            if (c.note >= 0 || c.instrument >= 0 || c.volume >= 0)
            {
                auto* e = xml->createNewChildElement ("C");
                e->setAttribute ("r", r);
                e->setAttribute ("t", t);
                e->setAttribute ("n", c.note);
                e->setAttribute ("i", c.instrument);
                e->setAttribute ("v", c.volume);
            }
        }
    }

    return xml;
}

void RetroTraxProcessor::applyStateXml (const juce::XmlElement& xml, juce::StringArray* missingSamples)
{
    engine.bpm = (float) xml.getDoubleAttribute ("bpm", 125.0);
    currentInstrument = juce::jlimit (0, TrackerEngine::kInstruments - 1,
                                      xml.getIntAttribute ("instrument", 0));
    currentOctave = juce::jlimit (1, 8, xml.getIntAttribute ("octave", 5));

    // Alte Instrumente leeren, damit Slots eines frueheren Songs nicht zurueckbleiben.
    for (int i = 0; i < TrackerEngine::kInstruments; ++i)
        engine.setInstrument (i, nullptr);

    engine.clearAllCells();

    for (auto* e : xml.getChildIterator())
    {
        if (e->hasTagName ("INSTRUMENT"))
        {
            const int slot = e->getIntAttribute ("slot", -1);
            const juce::File f (e->getStringAttribute ("path"));
            if (f.existsAsFile())
            {
                loadInstrument (slot, f);
            }
            else if (missingSamples != nullptr)
            {
                auto name = e->getStringAttribute ("name");
                if (name.isEmpty())
                    name = f.getFileNameWithoutExtension();
                missingSamples->add (name);
            }
        }
        else if (e->hasTagName ("C"))
        {
            const int r = e->getIntAttribute ("r", -1);
            const int t = e->getIntAttribute ("t", -1);
            if (r >= 0 && r < TrackerEngine::kRows && t >= 0 && t < TrackerEngine::kTracks)
            {
                auto& c = engine.cells[r][t];
                c.note       = e->getIntAttribute ("n", -1);
                c.instrument = e->getIntAttribute ("i", -1);
                c.volume     = e->getIntAttribute ("v", -1);
            }
        }
    }
}

bool RetroTraxProcessor::saveSong (const juce::File& file)
{
    auto xml = stateToXml();
    file.getParentDirectory().createDirectory();
    return xml->writeTo (file);
}

bool RetroTraxProcessor::loadSong (const juce::File& file, juce::StringArray& missingSamples)
{
    auto xml = juce::XmlDocument::parse (file);
    if (xml == nullptr || ! xml->hasTagName ("RETROTRAX"))
        return false;

    engine.stop(); // beim Oeffnen nie mitten im Abspielen bleiben
    applyStateXml (*xml, &missingSamples);
    return true;
}

void RetroTraxProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    copyXmlToBinary (*stateToXml(), destData);
}

void RetroTraxProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml != nullptr && xml->hasTagName ("RETROTRAX"))
        applyStateXml (*xml);
}

juce::AudioProcessorEditor* RetroTraxProcessor::createEditor()
{
    return new RetroTraxEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new RetroTraxProcessor();
}
