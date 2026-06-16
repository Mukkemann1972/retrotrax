#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "IFF8SVXFormat.h"
#include "ModImport.h"

RetroTraxProcessor::RetroTraxProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
    formatManager.registerBasicFormats();             // WAV, AIFF, FLAC, OGG, MP3
    formatManager.registerFormat (new IFF8SVXAudioFormat(), false); // Amiga 8SVX/IFF
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

// Sprechender Name fuer einen SID-Slot, z.B. "SID Puls".
static juce::String sidName (const TrackerEngine::Instrument& inst)
{
    using W = TrackerEngine::Instrument::Wave;
    switch (inst.wave)
    {
        case W::Triangle: return "SID Dreieck";
        case W::Saw:      return "SID Saege";
        case W::Noise:    return "SID Rauschen";
        case W::Pulse:
        default:          return "SID Puls";
    }
}

void RetroTraxProcessor::makeSidInstrument (int slot)
{
    if (slot < 0 || slot >= TrackerEngine::kInstruments)
        return;
    auto inst = std::make_unique<TrackerEngine::Instrument>();
    inst->kind = TrackerEngine::Instrument::Kind::Synth;
    inst->name = sidName (*inst);
    engine.setInstrument (slot, std::move (inst));
}

bool RetroTraxProcessor::isSid (int slot) const
{
    if (slot < 0 || slot >= TrackerEngine::kInstruments)
        return false;
    const juce::ScopedLock sl (engine.lock);
    const auto& p = engine.instruments[slot];
    return p != nullptr && p->kind == TrackerEngine::Instrument::Kind::Synth;
}

bool RetroTraxProcessor::getSid (int slot, TrackerEngine::Instrument& out) const
{
    if (slot < 0 || slot >= TrackerEngine::kInstruments)
        return false;
    const juce::ScopedLock sl (engine.lock);
    const auto& p = engine.instruments[slot];
    if (p == nullptr || p->kind != TrackerEngine::Instrument::Kind::Synth)
        return false;
    out.engine     = p->engine;
    out.unison     = p->unison;
    out.detune     = p->detune;
    out.chord      = p->chord;
    out.wave       = p->wave;
    out.pulseWidth = p->pulseWidth;
    out.attack     = p->attack;
    out.decay      = p->decay;
    out.sustain    = p->sustain;
    out.release    = p->release;
    out.filter     = p->filter;
    out.cutoff     = p->cutoff;
    out.resonance  = p->resonance;
    out.ringMod    = p->ringMod;
    out.sync       = p->sync;
    out.modTune    = p->modTune;
    out.pwmRate    = p->pwmRate;
    out.pwmDepth   = p->pwmDepth;
    return true;
}

void RetroTraxProcessor::editSid (int slot, std::function<void (TrackerEngine::Instrument&)> fn)
{
    if (slot < 0 || slot >= TrackerEngine::kInstruments)
        return;
    const juce::ScopedLock sl (engine.lock); // Audio-Thread haelt dieselbe Sperre
    auto& p = engine.instruments[slot];
    if (p == nullptr || p->kind != TrackerEngine::Instrument::Kind::Synth)
        return;
    fn (*p);
    p->name = sidName (*p);
}

std::unique_ptr<juce::XmlElement> RetroTraxProcessor::stateToXml()
{
    auto xml = std::make_unique<juce::XmlElement> ("RETROTRAX");
    xml->setAttribute ("version", 1);
    xml->setAttribute ("bpm", (double) engine.bpm.load());
    xml->setAttribute ("instrument", currentInstrument.load());
    xml->setAttribute ("octave", currentOctave.load());

    // Song-Reihenfolge + Modus (Reihenfolge als Liste von Pattern-Nummern).
    {
        juce::StringArray ord;
        for (int i = 0; i < engine.orderLen; ++i)
            ord.add (juce::String (engine.order[i]));
        xml->setAttribute ("order", ord.joinIntoString (" "));
        xml->setAttribute ("songMode", engine.songMode.load() ? 1 : 0);
        xml->setAttribute ("editPattern", engine.editPattern.load());
    }

    for (int i = 0; i < TrackerEngine::kInstruments; ++i)
    {
        const juce::ScopedLock sl (engine.lock);
        const auto& ip = engine.instruments[i];
        if (ip == nullptr)
            continue;

        if (ip->kind == TrackerEngine::Instrument::Kind::Synth)
        {
            auto* e = xml->createNewChildElement ("INSTRUMENT");
            e->setAttribute ("slot", i);
            e->setAttribute ("kind", "synth");
            e->setAttribute ("name", ip->name);
            e->setAttribute ("eng",  (int) ip->engine);
            e->setAttribute ("wave", (int) ip->wave);
            e->setAttribute ("pw",  ip->pulseWidth);
            e->setAttribute ("a",   ip->attack);
            e->setAttribute ("d",   ip->decay);
            e->setAttribute ("s",   ip->sustain);
            e->setAttribute ("rel", ip->release);
            e->setAttribute ("flt", (int) ip->filter);
            e->setAttribute ("cut", ip->cutoff);
            e->setAttribute ("res", ip->resonance);
            e->setAttribute ("ring", ip->ringMod ? 1 : 0);
            e->setAttribute ("sync", ip->sync ? 1 : 0);
            e->setAttribute ("mtune", ip->modTune);
            e->setAttribute ("pwmr", ip->pwmRate);
            e->setAttribute ("pwmd", ip->pwmDepth);
            e->setAttribute ("uni", ip->unison);
            e->setAttribute ("det", ip->detune);
            e->setAttribute ("chord", ip->chord);
        }
        else if (ip->filePath.isNotEmpty())
        {
            auto* e = xml->createNewChildElement ("INSTRUMENT");
            e->setAttribute ("slot", i);
            e->setAttribute ("name", ip->name);
            e->setAttribute ("path", ip->filePath);
        }
    }

    for (int p = 0; p < TrackerEngine::kMaxPatterns; ++p)
    {
        for (int r = 0; r < TrackerEngine::kRows; ++r)
        {
            for (int t = 0; t < TrackerEngine::kTracks; ++t)
            {
                const auto& c = engine.patterns[p][r][t];
                if (c.note >= 0 || c.instrument >= 0 || c.volume >= 0 || c.effect >= 0)
                {
                    auto* e = xml->createNewChildElement ("C");
                    if (p > 0) e->setAttribute ("p", p); // p=0 weglassen -> alte Songs laden weiter
                    e->setAttribute ("r", r);
                    e->setAttribute ("t", t);
                    e->setAttribute ("n", c.note);
                    e->setAttribute ("i", c.instrument);
                    e->setAttribute ("v", c.volume);
                    if (c.effect >= 0)
                    {
                        e->setAttribute ("fx", c.effect);
                        e->setAttribute ("fp", c.effectParam);
                    }
                }
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
            if (slot < 0 || slot >= TrackerEngine::kInstruments)
                continue;

            if (e->getStringAttribute ("kind") == "synth")
            {
                auto inst = std::make_unique<TrackerEngine::Instrument>();
                inst->kind       = TrackerEngine::Instrument::Kind::Synth;
                inst->engine     = (TrackerEngine::Instrument::Engine)
                                       juce::jlimit (0, 1, e->getIntAttribute ("eng", 0)); // alt -> Classic
                inst->wave       = (TrackerEngine::Instrument::Wave)
                                       juce::jlimit (0, 3, e->getIntAttribute ("wave", 2));
                inst->pulseWidth = (float) e->getDoubleAttribute ("pw",  0.5);
                inst->attack     = (float) e->getDoubleAttribute ("a",   0.004);
                inst->decay      = (float) e->getDoubleAttribute ("d",   0.18);
                inst->sustain    = (float) e->getDoubleAttribute ("s",   0.65);
                inst->release    = (float) e->getDoubleAttribute ("rel", 0.25);
                inst->filter     = (TrackerEngine::Instrument::Filter)
                                       juce::jlimit (0, 3, e->getIntAttribute ("flt", 0));
                inst->cutoff     = (float) e->getDoubleAttribute ("cut", 0.7);
                inst->resonance  = (float) e->getDoubleAttribute ("res", 0.12);
                inst->ringMod    = e->getIntAttribute ("ring", 0) != 0;
                inst->sync       = e->getIntAttribute ("sync", 0) != 0;
                inst->modTune    = (float) e->getDoubleAttribute ("mtune", 12.0);
                inst->pwmRate    = (float) e->getDoubleAttribute ("pwmr", 0.0);
                inst->pwmDepth   = (float) e->getDoubleAttribute ("pwmd", 0.0);
                inst->unison     = juce::jlimit (1, 3, e->getIntAttribute ("uni", 1)); // alt -> 1 (aus)
                inst->detune     = (float) e->getDoubleAttribute ("det", 0.25);
                inst->chord      = juce::jlimit (0, TrackerEngine::Instrument::kNumChords - 1,
                                                 e->getIntAttribute ("chord", 0)); // alt -> 0 (aus)
                inst->name       = e->getStringAttribute ("name", "SID");
                engine.setInstrument (slot, std::move (inst));
                continue;
            }

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
            const int p = e->getIntAttribute ("p", 0); // alte Songs: kein p -> Pattern 0
            const int r = e->getIntAttribute ("r", -1);
            const int t = e->getIntAttribute ("t", -1);
            if (p >= 0 && p < TrackerEngine::kMaxPatterns
                && r >= 0 && r < TrackerEngine::kRows
                && t >= 0 && t < TrackerEngine::kTracks)
            {
                auto& c = engine.patterns[p][r][t];
                c.note        = e->getIntAttribute ("n", -1);
                c.instrument  = e->getIntAttribute ("i", -1);
                c.volume      = e->getIntAttribute ("v", -1);
                c.effect      = e->getIntAttribute ("fx", -1);
                c.effectParam = e->getIntAttribute ("fp", 0);
            }
        }
    }

    // Song-Reihenfolge wiederherstellen (Standard: nur Pattern 0).
    juce::StringArray ord;
    ord.addTokens (xml.getStringAttribute ("order", "0"), " ", "");
    ord.removeEmptyStrings();
    int n = 0;
    for (const auto& s : ord)
    {
        if (n >= TrackerEngine::kMaxOrder) break;
        engine.order[n++] = juce::jlimit (0, TrackerEngine::kMaxPatterns - 1, s.getIntValue());
    }
    engine.orderLen = juce::jmax (1, n);
    engine.songMode = xml.getIntAttribute ("songMode", 0) != 0;
    engine.setEditPattern (xml.getIntAttribute ("editPattern", 0));
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

bool RetroTraxProcessor::loadMod (const juce::File& file, juce::String& message)
{
    auto song = ModImport::parse (file);
    if (! song.ok)
    {
        message = song.message;
        return false;
    }

    engine.stop(); // nie mitten im Abspielen umbauen

    // 1) Alle Patterns leeren, damit nichts vom alten Song stehen bleibt.
    {
        const juce::ScopedLock sl (engine.lock);
        for (auto& pat : engine.patterns)
            for (auto& row : pat)
                for (auto& cl : row)
                    cl = TrackerEngine::Cell();
    }

    // 2) Samples in die Instrument-Slots (setInstrument nimmt selbst den Lock).
    int loaded = 0;
    for (int i = 0; i < TrackerEngine::kInstruments; ++i)
    {
        if (i < 31 && song.samples[i].data.getNumSamples() > 1)
        {
            auto inst = std::make_unique<TrackerEngine::Instrument>();
            inst->kind       = TrackerEngine::Instrument::Kind::Sample;
            inst->data       = std::move (song.samples[i].data);
            inst->sourceRate = song.samples[i].sourceRate;
            inst->name       = song.samples[i].name.isNotEmpty()
                                 ? song.samples[i].name
                                 : juce::String ("Sample ") + juce::String (i + 1);
            engine.setInstrument (i, std::move (inst));
            ++loaded;
        }
        else
        {
            engine.setInstrument (i, nullptr); // leeren Slot freiraeumen
        }
    }

    // 3) Pattern-Zellen + Reihenfolge uebernehmen.
    {
        const juce::ScopedLock sl (engine.lock);
        const int nch  = juce::jmin (song.channels,    TrackerEngine::kTracks);
        const int npat = juce::jmin (song.numPatterns,  TrackerEngine::kMaxPatterns);
        for (int p = 0; p < npat; ++p)
            for (int r = 0; r < 64 && r < TrackerEngine::kRows; ++r)
                for (int c = 0; c < nch; ++c)
                {
                    const auto& mc = song.patterns[(size_t) p][(size_t) r][(size_t) c];
                    auto& cell = engine.patterns[p][r][c];
                    cell.note        = mc.note;
                    cell.instrument  = mc.instrument;
                    cell.volume      = mc.volume;
                    cell.effect      = mc.effect;
                    cell.effectParam = mc.effectParam;
                }

        int nn = 0;
        for (int i = 0; i < song.songLength && nn < TrackerEngine::kMaxOrder; ++i)
            engine.order[nn++] = juce::jlimit (0, TrackerEngine::kMaxPatterns - 1, song.order[(size_t) i]);
        engine.orderLen = juce::jmax (1, nn);
        engine.songMode = true; // ein MOD ist ein ganzer Song -> Song-Modus an
        engine.setEditPattern (engine.order[0]);
    }

    // 4) Kurze Zusammenfassung (mit Hinweis, falls etwas gekuerzt wurde).
    juce::String warn;
    if (song.channels > TrackerEngine::kTracks)
        warn << " (nur " << TrackerEngine::kTracks << " von " << song.channels << " Kanaelen)";
    if (song.numPatterns > TrackerEngine::kMaxPatterns)
        warn << " (auf " << TrackerEngine::kMaxPatterns << " Patterns gekuerzt)";

    message = "\"" + song.title + "\": " + juce::String (loaded) + " Samples, "
            + juce::String (juce::jmin (song.numPatterns, TrackerEngine::kMaxPatterns)) + " Patterns, "
            + juce::String (song.channels) + " Kanaele." + warn;
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
