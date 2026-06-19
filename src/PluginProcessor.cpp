#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "IFF8SVXFormat.h"
#include "ModImport.h"
#include "XmImport.h"
#include "S3mImport.h"
#include "ItImport.h"

RetroTraxProcessor::RetroTraxProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
    formatManager.registerBasicFormats();             // WAV, AIFF, FLAC, OGG, MP3
    formatManager.registerFormat (new IFF8SVXAudioFormat(), false); // Amiga 8SVX/IFF
}

void RetroTraxProcessor::prepareToPlay (double sampleRate, int)
{
    engine.prepare (sampleRate);
    tfmx.prepare (sampleRate);
}

bool RetroTraxProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void RetroTraxProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    // TFMX-Wiedergabe-Modus: der eigene Replayer ersetzt die Tracker-Engine.
    // PLAY/STOP nutzen dieselbe engine.playing-Flagge; die steigende Flanke
    // (Stop -> Play) startet den TFMX-Song von vorn.
    if (playbackMode.load() == PlaybackMode::Tfmx)
    {
        midi.clear();
        const bool playing = engine.playing.load();
        if (playing && ! tfmxWasPlaying)
            tfmx.restart();
        tfmxWasPlaying = playing;

        if (playing)
            tfmx.render (buffer, 0, buffer.getNumSamples());
        else
            buffer.clear();
        feedScope (buffer);
        return;
    }

    for (const auto metadata : midi)
    {
        const auto msg = metadata.getMessage();
        if (msg.isNoteOn())
            engine.audition (msg.getNoteNumber(), currentInstrument.load());
    }
    midi.clear();

    engine.process (buffer);
    feedScope (buffer);
}

// Den fertigen Stereo-Mix (als Mono-Mittel) in den Ringpuffer schreiben, damit
// die Spektrum-Anzeige im Timer den juengsten Ausschnitt lesen kann. Laeuft im
// Audio-Thread; nur ein paar Kopien + ein atomarer Schreibzeiger, kein Lock.
void RetroTraxProcessor::feedScope (const juce::AudioBuffer<float>& buffer)
{
    const int n  = buffer.getNumSamples();
    const int ch = buffer.getNumChannels();
    if (n <= 0 || ch <= 0)
        return;
    const float* L = buffer.getReadPointer (0);
    const float* R = ch > 1 ? buffer.getReadPointer (1) : L;
    int p = scopePos.load (std::memory_order_relaxed);
    for (int i = 0; i < n; ++i)
    {
        scope[p] = 0.5f * (L[i] + R[i]);
        p = (p + 1) & (kScopeSize - 1);
    }
    scopePos.store (p, std::memory_order_relaxed);
}

bool RetroTraxProcessor::renderSongToWav (const juce::File& file, juce::String& message)
{
    const double sr = getSampleRate() > 0.0 ? getSampleRate() : 44100.0;

    file.deleteFile();
    std::unique_ptr<juce::FileOutputStream> stream (file.createOutputStream());
    if (stream == nullptr)
    {
        message = "Datei konnte nicht angelegt werden.";
        return false;
    }
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatWriter> writer (
        wav.createWriterFor (stream.get(), sr, 2, 16, {}, 0));
    if (writer == nullptr)
    {
        message = "WAV-Writer konnte nicht erstellt werden.";
        return false;
    }
    stream.release(); // der Writer uebernimmt den Stream

    // Waehrend des Offline-Renderns das Live-Audio aussetzen, damit der Audio-
    // Thread die Engine nicht parallel anfasst (sonst Datenrennen/Doppel-Trigger).
    suspendProcessing (true);

    // Transport sichern und so einstellen, dass wir GENAU einen Durchlauf rendern:
    // im Song-Modus die echte Reihenfolge, sonst nur das gerade bearbeitete Pattern.
    const bool savedSongMode = engine.songMode.load();
    const int  savedOrder0   = engine.order[0];
    const int  savedOrderLen = engine.orderLen;

    engine.stop();
    if (! savedSongMode)
    {
        engine.order[0] = engine.editPattern.load();
        engine.orderLen = 1;
    }
    engine.songMode = true;
    engine.play(); // startet bei Order-Position 0, setzt songLoopCount = 0

    const int block = 1024;
    juce::AudioBuffer<float> buf (2, block);
    const double maxSamples = sr * 60.0 * 12.0; // Sicherheits-Kappe: 12 Minuten
    double rendered = 0.0;
    bool ok = true;
    // Rendern, bis die Reihenfolge einmal komplett umgelaufen ist.
    while (engine.songLoopCount.load() == 0 && rendered < maxSamples)
    {
        buf.clear();
        engine.process (buf);
        if (! writer->writeFromAudioSampleBuffer (buf, 0, block))
        {
            ok = false;
            break;
        }
        rendered += block;
    }

    engine.stop();
    engine.songMode = savedSongMode;
    engine.order[0] = savedOrder0;
    engine.orderLen = savedOrderLen;

    writer.reset(); // schliesst und finalisiert die WAV-Datei
    suspendProcessing (false);

    if (! ok)
    {
        message = "Schreiben der WAV-Datei fehlgeschlagen.";
        return false;
    }
    message = "Song als WAV gespeichert (" + juce::String (rendered / sr, 1) + " s).";
    return true;
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
    playbackMode = PlaybackMode::Tracker; // zurueck zum Tracker, falls TFMX lief
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

bool RetroTraxProcessor::isSampleSlot (int slot) const
{
    if (slot < 0 || slot >= TrackerEngine::kInstruments)
        return false;
    const juce::ScopedLock sl (engine.lock);
    const auto& p = engine.instruments[slot];
    return p != nullptr && p->kind == TrackerEngine::Instrument::Kind::Sample
        && p->data.getNumSamples() > 1;
}

bool RetroTraxProcessor::getSample (int slot, TrackerEngine::Instrument& out) const
{
    if (slot < 0 || slot >= TrackerEngine::kInstruments)
        return false;
    const juce::ScopedLock sl (engine.lock);
    const auto& p = engine.instruments[slot];
    if (p == nullptr || p->kind != TrackerEngine::Instrument::Kind::Sample)
        return false;
    out.name          = p->name;
    out.akaiOn        = p->akaiOn;
    out.akaiCutoff    = p->akaiCutoff;
    out.akaiResonance = p->akaiResonance;
    out.akai12bit     = p->akai12bit;
    out.reverse       = p->reverse;
    out.srReduction   = p->srReduction;
    out.loopMode      = p->loopMode;
    out.loopXfade     = p->loopXfade;
    out.drive         = p->drive;
    out.vintagePitch  = p->vintagePitch;
    return true;
}

void RetroTraxProcessor::editSample (int slot, std::function<void (TrackerEngine::Instrument&)> fn)
{
    if (slot < 0 || slot >= TrackerEngine::kInstruments)
        return;
    const juce::ScopedLock sl (engine.lock); // Audio-Thread haelt dieselbe Sperre
    auto& p = engine.instruments[slot];
    if (p == nullptr || p->kind != TrackerEngine::Instrument::Kind::Sample)
        return;
    fn (*p);
}

// --- Drum-Kit -----------------------------------------------------------------

bool RetroTraxProcessor::loadPad (int pad, const juce::File& file)
{
    if (pad < 0 || pad >= TrackerEngine::kPads)
        return false;
    auto inst = createInstrument (file);
    if (inst == nullptr)
        return false;
    engine.setPad (pad, std::move (inst));
    return true;
}

void RetroTraxProcessor::clearPad (int pad)
{
    engine.clearPad (pad);
}

bool RetroTraxProcessor::getPadName (int pad, juce::String& name) const
{
    const juce::ScopedLock sl (engine.lock);
    const auto* p = engine.getPad (pad);
    if (p == nullptr)
        return false;
    name = p->name;
    return true;
}

bool RetroTraxProcessor::getPad (int pad, TrackerEngine::Instrument& out) const
{
    const juce::ScopedLock sl (engine.lock);
    const auto* p = engine.getPad (pad);
    if (p == nullptr)
        return false;
    out.name          = p->name;
    out.akaiOn        = p->akaiOn;
    out.akaiCutoff    = p->akaiCutoff;
    out.akaiResonance = p->akaiResonance;
    out.akai12bit     = p->akai12bit;
    out.reverse       = p->reverse;
    out.srReduction   = p->srReduction;
    out.loopMode      = p->loopMode;
    out.loopXfade     = p->loopXfade;
    out.drive         = p->drive;
    out.vintagePitch  = p->vintagePitch;
    out.sourceRate    = p->sourceRate;
    return true;
}

void RetroTraxProcessor::editPad (int pad, std::function<void (TrackerEngine::Instrument&)> fn)
{
    const juce::ScopedLock sl (engine.lock);
    if (auto* p = const_cast<TrackerEngine::Instrument*> (engine.getPad (pad)))
        fn (*p);
}

bool RetroTraxProcessor::padToSlot (int pad, int slot)
{
    const juce::ScopedLock sl (engine.lock);
    const auto* src = engine.getPad (pad);
    if (src == nullptr || slot < 0 || slot >= TrackerEngine::kInstruments)
        return false;
    engine.setInstrument (slot, std::make_unique<TrackerEngine::Instrument> (*src)); // tiefe Kopie
    return true;
}

bool RetroTraxProcessor::slotToPad (int slot, int pad)
{
    const juce::ScopedLock sl (engine.lock);
    if (slot < 0 || slot >= TrackerEngine::kInstruments || pad < 0 || pad >= TrackerEngine::kPads)
        return false;
    const auto& p = engine.instruments[slot];
    if (p == nullptr || p->kind != TrackerEngine::Instrument::Kind::Sample
        || p->data.getNumSamples() < 2)
        return false;
    engine.setPad (pad, std::make_unique<TrackerEngine::Instrument> (*p)); // tiefe Kopie
    return true;
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
            // Akai-Filter nur sichern, wenn er ueberhaupt benutzt wird -> normale
            // Sample-Slots bleiben in der Datei unveraendert (alte Songs laden weiter).
            if (ip->akaiOn || ip->akai12bit || ip->reverse || ip->srReduction > 0.0f
                || ip->loopMode != TrackerEngine::Instrument::Loop::Off
                || ip->loopXfade > 0.0f
                || ip->drive > 0.0f || ip->vintagePitch)
            {
                e->setAttribute ("akon", ip->akaiOn ? 1 : 0);
                e->setAttribute ("akcut", ip->akaiCutoff);
                e->setAttribute ("akres", ip->akaiResonance);
                e->setAttribute ("ak12", ip->akai12bit ? 1 : 0);
                e->setAttribute ("rev", ip->reverse ? 1 : 0);
                e->setAttribute ("srr", ip->srReduction);
                e->setAttribute ("loop", (int) ip->loopMode);
                e->setAttribute ("lxf", ip->loopXfade);
                e->setAttribute ("drv", ip->drive);
                e->setAttribute ("vint", ip->vintagePitch ? 1 : 0);
            }
        }
    }

    // Drum-Kit-Pads (16). Wie bei den Sample-Slots per Dateipfad referenziert,
    // plus der Sampler-Charakter (12-Bit/Loop/Drive...). Pads ohne Dateipfad
    // (z.B. spaeter gezeichnete) bleiben hier vorerst aussen vor.
    for (int p = 0; p < TrackerEngine::kPads; ++p)
    {
        const juce::ScopedLock sl (engine.lock);
        const auto* pad = engine.getPad (p);
        if (pad == nullptr || pad->filePath.isEmpty())
            continue;
        auto* e = xml->createNewChildElement ("PAD");
        e->setAttribute ("idx", p);
        e->setAttribute ("name", pad->name);
        e->setAttribute ("path", pad->filePath);
        e->setAttribute ("akon", pad->akaiOn ? 1 : 0);
        e->setAttribute ("akcut", pad->akaiCutoff);
        e->setAttribute ("akres", pad->akaiResonance);
        e->setAttribute ("ak12", pad->akai12bit ? 1 : 0);
        e->setAttribute ("rev", pad->reverse ? 1 : 0);
        e->setAttribute ("srr", pad->srReduction);
        e->setAttribute ("loop", (int) pad->loopMode);
        e->setAttribute ("lxf", pad->loopXfade);
        e->setAttribute ("drv", pad->drive);
        e->setAttribute ("vint", pad->vintagePitch ? 1 : 0);
        e->setAttribute ("rate", pad->sourceRate);
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
    for (int p = 0; p < TrackerEngine::kPads; ++p) // Drum-Kit auch leeren
        engine.clearPad (p);

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
                // Akai-Filter-Einstellungen auf das frisch geladene Sample legen
                // (fehlen sie in der Datei -> Standard AUS, Klang unveraendert).
                const juce::ScopedLock sl (engine.lock);
                if (auto& ip = engine.instruments[slot])
                {
                    ip->akaiOn        = e->getIntAttribute ("akon", 0) != 0;
                    ip->akaiCutoff    = (float) e->getDoubleAttribute ("akcut", 1.0);
                    ip->akaiResonance = (float) e->getDoubleAttribute ("akres", 0.12);
                    ip->akai12bit     = e->getIntAttribute ("ak12", 0) != 0;
                    ip->reverse       = e->getIntAttribute ("rev", 0) != 0;
                    ip->srReduction   = (float) e->getDoubleAttribute ("srr", 0.0);
                    ip->loopMode      = (TrackerEngine::Instrument::Loop)
                                            juce::jlimit (0, 2, e->getIntAttribute ("loop", 0));
                    ip->loopXfade     = (float) e->getDoubleAttribute ("lxf", 0.0);
                    ip->drive         = (float) e->getDoubleAttribute ("drv", 0.0);
                    ip->vintagePitch  = e->getIntAttribute ("vint", 0) != 0;
                }
            }
            else if (missingSamples != nullptr)
            {
                auto name = e->getStringAttribute ("name");
                if (name.isEmpty())
                    name = f.getFileNameWithoutExtension();
                missingSamples->add (name);
            }
        }
        else if (e->hasTagName ("PAD"))
        {
            const int idx = e->getIntAttribute ("idx", -1);
            const juce::File f (e->getStringAttribute ("path"));
            if (idx >= 0 && idx < TrackerEngine::kPads && f.existsAsFile() && loadPad (idx, f))
            {
                const juce::ScopedLock sl (engine.lock);
                if (auto* pad = const_cast<TrackerEngine::Instrument*> (engine.getPad (idx)))
                {
                    pad->akaiOn        = e->getIntAttribute ("akon", 0) != 0;
                    pad->akaiCutoff    = (float) e->getDoubleAttribute ("akcut", 1.0);
                    pad->akaiResonance = (float) e->getDoubleAttribute ("akres", 0.12);
                    pad->akai12bit     = e->getIntAttribute ("ak12", 0) != 0;
                    pad->reverse       = e->getIntAttribute ("rev", 0) != 0;
                    pad->srReduction   = (float) e->getDoubleAttribute ("srr", 0.0);
                    pad->loopMode      = (TrackerEngine::Instrument::Loop)
                                             juce::jlimit (0, 2, e->getIntAttribute ("loop", 0));
                    pad->loopXfade     = (float) e->getDoubleAttribute ("lxf", 0.0);
                    pad->drive         = (float) e->getDoubleAttribute ("drv", 0.0);
                    pad->vintagePitch  = e->getIntAttribute ("vint", 0) != 0;
                    pad->sourceRate    = e->getDoubleAttribute ("rate", pad->sourceRate);
                    if (e->getStringAttribute ("name").isNotEmpty())
                        pad->name = e->getStringAttribute ("name");
                }
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
    playbackMode = PlaybackMode::Tracker; // ein .retrotrax-Song laeuft im Tracker
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
    playbackMode = PlaybackMode::Tracker; // MOD/XM laeuft ueber die Tracker-Engine

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

bool RetroTraxProcessor::loadXm (const juce::File& file, juce::String& message)
{
    auto song = XmImport::parse (file);
    if (! song.ok)
    {
        message = song.message;
        return false;
    }

    engine.stop(); // nie mitten im Abspielen umbauen
    playbackMode = PlaybackMode::Tracker; // MOD/XM laeuft ueber die Tracker-Engine

    // 1) Alle Patterns leeren, damit nichts vom alten Song stehen bleibt.
    {
        const juce::ScopedLock sl (engine.lock);
        for (auto& pat : engine.patterns)
            for (auto& row : pat)
                for (auto& cl : row)
                    cl = TrackerEngine::Cell();
    }

    // 2) Samples in die Instrument-Slots (je XM-Instrument das erste Sample).
    int loaded = 0;
    const int nInst = (int) song.samples.size();
    for (int i = 0; i < TrackerEngine::kInstruments; ++i)
    {
        if (i < nInst && song.samples[(size_t) i].data.getNumSamples() > 1)
        {
            auto inst = std::make_unique<TrackerEngine::Instrument>();
            inst->kind       = TrackerEngine::Instrument::Kind::Sample;
            inst->data       = std::move (song.samples[(size_t) i].data);
            inst->sourceRate = song.samples[(size_t) i].sourceRate;
            inst->name       = song.samples[(size_t) i].name.isNotEmpty()
                                 ? song.samples[(size_t) i].name
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
        {
            const int prows = (int) song.patterns[(size_t) p].size();
            for (int r = 0; r < prows && r < TrackerEngine::kRows; ++r)
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
        }

        int nn = 0;
        for (int i = 0; i < song.songLength && nn < TrackerEngine::kMaxOrder; ++i)
            engine.order[nn++] = juce::jlimit (0, TrackerEngine::kMaxPatterns - 1, song.order[i]);
        engine.orderLen = juce::jmax (1, nn);
        engine.songMode = true; // ein XM ist ein ganzer Song -> Song-Modus an
        engine.setEditPattern (engine.order[0]);
    }

    // 4) Kurze Zusammenfassung (mit Hinweis, falls etwas gekuerzt wurde).
    juce::String warn;
    if (song.channels > TrackerEngine::kTracks)
        warn << " (nur " << TrackerEngine::kTracks << " von " << song.channels << " Kanaelen)";
    if (song.numPatterns > TrackerEngine::kMaxPatterns)
        warn << " (auf " << TrackerEngine::kMaxPatterns << " Patterns gekuerzt)";
    {
        bool longPat = false;
        for (const auto& pat : song.patterns)
            if ((int) pat.size() > TrackerEngine::kRows) { longPat = true; break; }
        if (longPat)
            warn << " (Patterns auf " << TrackerEngine::kRows << " Zeilen gekuerzt)";
    }

    message = "\"" + song.title + "\": " + juce::String (loaded) + " Samples, "
            + juce::String (juce::jmin (song.numPatterns, TrackerEngine::kMaxPatterns)) + " Patterns, "
            + juce::String (song.channels) + " Kanaele." + warn;
    return true;
}

bool RetroTraxProcessor::applyImportedSong (const ImportCommon::Song& song, juce::String& message)
{
    if (! song.ok)
    {
        message = song.message;
        return false;
    }

    engine.stop();
    playbackMode = PlaybackMode::Tracker; // laeuft ueber die Tracker-Engine

    // 1) Alle Patterns leeren.
    {
        const juce::ScopedLock sl (engine.lock);
        for (auto& pat : engine.patterns)
            for (auto& row : pat)
                for (auto& cl : row)
                    cl = TrackerEngine::Cell();
    }

    // 2) Samples in die Instrument-Slots.
    int loaded = 0;
    const int nInst = (int) song.samples.size();
    for (int i = 0; i < TrackerEngine::kInstruments; ++i)
    {
        if (i < nInst && song.samples[(size_t) i].data.getNumSamples() > 1)
        {
            auto inst = std::make_unique<TrackerEngine::Instrument>();
            inst->kind       = TrackerEngine::Instrument::Kind::Sample;
            inst->data       = song.samples[(size_t) i].data; // Kopie (song ist const)
            inst->sourceRate = song.samples[(size_t) i].sourceRate;
            inst->name       = song.samples[(size_t) i].name.isNotEmpty()
                                 ? song.samples[(size_t) i].name
                                 : juce::String ("Sample ") + juce::String (i + 1);
            engine.setInstrument (i, std::move (inst));
            ++loaded;
        }
        else
        {
            engine.setInstrument (i, nullptr);
        }
    }

    // 3) Pattern-Zellen + Reihenfolge uebernehmen.
    {
        const juce::ScopedLock sl (engine.lock);
        const int nch  = juce::jmin (song.channels,    TrackerEngine::kTracks);
        const int npat = juce::jmin (song.numPatterns,  TrackerEngine::kMaxPatterns);
        for (int p = 0; p < npat; ++p)
        {
            const int prows = (int) song.patterns[(size_t) p].size();
            for (int r = 0; r < prows && r < TrackerEngine::kRows; ++r)
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
        }

        int nn = 0;
        for (int i = 0; i < (int) song.order.size() && nn < TrackerEngine::kMaxOrder; ++i)
            engine.order[nn++] = juce::jlimit (0, TrackerEngine::kMaxPatterns - 1, song.order[(size_t) i]);
        engine.orderLen = juce::jmax (1, nn);
        engine.songMode = true; // ein ganzes Modul -> Song-Modus an
        engine.setEditPattern (engine.order[0]);
    }

    // 4) Kurze Zusammenfassung (mit Hinweisen, falls etwas gekuerzt/ausgelassen).
    juce::String warn;
    if (song.channels > TrackerEngine::kTracks)
        warn << " (nur " << TrackerEngine::kTracks << " von " << song.channels << " Kanaelen)";
    if (song.numPatterns > TrackerEngine::kMaxPatterns)
        warn << " (auf " << TrackerEngine::kMaxPatterns << " Patterns gekuerzt)";
    if (song.message != "OK" && song.message.isNotEmpty())
        warn << " (" << song.message << ")";

    message = "\"" + song.title + "\": " + juce::String (loaded) + " Samples, "
            + juce::String (juce::jmin (song.numPatterns, TrackerEngine::kMaxPatterns)) + " Patterns, "
            + juce::String (song.channels) + " Kanaele." + warn;
    return true;
}

bool RetroTraxProcessor::loadS3m (const juce::File& file, juce::String& message)
{
    return applyImportedSong (S3mImport::parse (file), message);
}

bool RetroTraxProcessor::loadIt (const juce::File& file, juce::String& message)
{
    return applyImportedSong (ItImport::parse (file), message);
}

juce::File RetroTraxProcessor::findTfmxSmpl (const juce::File& mdatFile)
{
    const auto dir  = mdatFile.getParentDirectory();
    const auto name = mdatFile.getFileName();

    // Modland-Konvention: "mdat.xxx" <-> "smpl.xxx".
    if (name.startsWithIgnoreCase ("mdat."))
    {
        auto cand = dir.getChildFile ("smpl." + name.substring (5));
        if (cand.existsAsFile()) return cand;
    }
    // Endungs-Konventionen: "xxx.mdat" <-> "xxx.smpl", und
    // "xxx.tfmx"/".tfx"/".tfm" <-> "xxx.sam" (haeufige Zip-Variante).
    const auto ext = mdatFile.getFileExtension();
    if (ext.equalsIgnoreCase (".mdat"))
    {
        auto cand = mdatFile.withFileExtension ("smpl");
        if (cand.existsAsFile()) return cand;
    }
    if (ext.equalsIgnoreCase (".tfmx") || ext.equalsIgnoreCase (".tfx") || ext.equalsIgnoreCase (".tfm"))
    {
        auto cand = mdatFile.withFileExtension ("sam");
        if (cand.existsAsFile()) return cand;
        auto cand2 = mdatFile.withFileExtension ("smpl");
        if (cand2.existsAsFile()) return cand2;
    }
    // Fallback: "mdat" im Namen durch "smpl" ersetzen.
    if (name.containsIgnoreCase ("mdat"))
    {
        auto cand = dir.getChildFile (name.replace ("mdat", "smpl", true));
        if (cand.existsAsFile()) return cand;
    }
    return {};
}

bool RetroTraxProcessor::loadTfmx (const juce::File& mdatFile, juce::String& message)
{
    const auto smplFile = findTfmxSmpl (mdatFile);
    if (! smplFile.existsAsFile())
    {
        message = "Keine passende .smpl-Datei neben \"" + mdatFile.getFileName() + "\" gefunden.";
        return false;
    }

    if (! tfmx.load (mdatFile, smplFile))
    {
        message = tfmx.info().message;
        return false;
    }

    engine.stop();                          // Tracker anhalten
    playbackMode = PlaybackMode::Tfmx;      // ab jetzt spielt der TFMX-Replayer

    const auto& in = tfmx.info();
    message = "\"" + in.title + "\": " + juce::String (in.subsongs) + " Subsongs, "
            + juce::String (in.patterns) + " Patterns, " + juce::String (in.macros) + " Makros, "
            + juce::String (in.tracksteps) + " Tracksteps, " + juce::String (in.sampleBytes / 1024) + " KB Samples.";
    message += tfmx.isPlayable() ? "  PLAY startet die Wiedergabe."
                                 : "  (Wiedergabe nicht moeglich - .smpl gefunden?)";
    return true;
}

// Schreibt einen Float-Puffer als 16-Bit-WAV (fuer den TFMX-Grabber).
static bool writeWavBuffer (const juce::File& file, const juce::AudioBuffer<float>& buf, double rate)
{
    file.getParentDirectory().createDirectory();
    file.deleteFile();

    std::unique_ptr<juce::FileOutputStream> os (file.createOutputStream());
    if (os == nullptr)
        return false;

    juce::WavAudioFormat fmt;
    std::unique_ptr<juce::AudioFormatWriter> writer (
        fmt.createWriterFor (os.get(), rate, (unsigned int) buf.getNumChannels(), 16, {}, 0));
    if (writer == nullptr)
        return false;

    os.release(); // gehoert ab jetzt dem Writer
    return writer->writeFromAudioSampleBuffer (buf, 0, buf.getNumSamples());
}

int RetroTraxProcessor::grabTfmxSamples (const juce::File& mdatFile, const juce::File& outFolder,
                                         juce::String& message)
{
    const auto smplFile = findTfmxSmpl (mdatFile);
    if (! smplFile.existsAsFile())
    {
        message = "Keine passende .smpl-Datei neben \"" + mdatFile.getFileName() + "\" gefunden.";
        return 0;
    }

    // Eigener Reader, damit der laufende TFMX-Replayer ungestoert bleibt.
    TfmxPlayer grabber;
    if (! grabber.load (mdatFile, smplFile))
    {
        message = grabber.info().message;
        return 0;
    }

    const auto grabs = grabber.grabSamples();
    if (grabs.empty())
    {
        message = "Keine Samples in diesem TFMX gefunden.";
        return 0;
    }

    outFolder.createDirectory();
    const double rate = 8287.0; // Amiga-typische Sample-Rate (wie beim MOD-Import)
    int written = 0;
    for (size_t i = 0; i < grabs.size(); ++i)
    {
        const auto f = outFolder.getChildFile (juce::String::formatted ("%02d", (int) i + 1)
                         + " " + outFolder.getFileName() + ".wav");
        if (writeWavBuffer (f, grabs[i].audio, rate))
            ++written;
    }

    message = juce::String (written) + " Samples entnommen nach \"" + outFolder.getFileName() + "\".";
    return written;
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
