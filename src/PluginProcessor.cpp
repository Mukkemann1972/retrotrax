#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "IFF8SVXFormat.h"
#include "ModImport.h"
#include "XmImport.h"
#include "S3mImport.h"
#include "ItImport.h"

// Schreibt einen Float-Puffer als 16-Bit-WAV (Grabber/Chop/Edit). Definition weiter unten.
static bool writeWavBuffer (const juce::File& file, const juce::AudioBuffer<float>& buf, double rate);
// Sample-Daten ein-/auspacken (selbst-enthaltende Songs + Kits). Definition weiter unten.
static juce::String encodeSamples (const juce::AudioBuffer<float>& buf);
static bool decodeSamples (const juce::String& b64, int ch, int n, juce::AudioBuffer<float>& out);

RetroTraxProcessor::RetroTraxProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
    formatManager.registerBasicFormats();             // WAV, AIFF, FLAC, OGG, MP3
    formatManager.registerFormat (new IFF8SVXAudioFormat(), false); // Amiga 8SVX/IFF
}

// Kleiner Biquad (RBJ-Cookbook) fuer den Master-EQ - Transposed Direct Form II.
struct Biquad
{
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    float run (float x, float& z1, float& z2) const
    {
        const float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
};

static Biquad makeShelf (double fc, double dB, double fs, bool high)
{
    const double A  = std::pow (10.0, dB / 40.0);
    const double w0 = 2.0 * juce::MathConstants<double>::pi * fc / fs;
    const double c  = std::cos (w0), s = std::sin (w0);
    const double alpha = s / 2.0 * std::sqrt ((A + 1.0 / A) * (1.0 / 0.9 - 1.0) + 2.0);
    const double tsa = 2.0 * std::sqrt (A) * alpha;
    const double sign = high ? -1.0 : 1.0;
    const double b0 = A * ((A + 1) - sign * (A - 1) * c + tsa);
    const double b1 = 2 * A * ((A - 1) - sign * (A + 1) * c) * sign;
    const double b2 = A * ((A + 1) - sign * (A - 1) * c - tsa);
    const double a0 = (A + 1) + sign * (A - 1) * c + tsa;
    const double a1 = -2 * ((A - 1) + sign * (A + 1) * c) * sign;
    const double a2 = (A + 1) + sign * (A - 1) * c - tsa;
    return { (float)(b0/a0), (float)(b1/a0), (float)(b2/a0), (float)(a1/a0), (float)(a2/a0) };
}

static Biquad makePeak (double fc, double dB, double q, double fs)
{
    const double A  = std::pow (10.0, dB / 40.0);
    const double w0 = 2.0 * juce::MathConstants<double>::pi * fc / fs;
    const double c  = std::cos (w0), s = std::sin (w0);
    const double alpha = s / (2.0 * q);
    const double b0 = 1 + alpha * A, b1 = -2 * c, b2 = 1 - alpha * A;
    const double a0 = 1 + alpha / A, a1 = -2 * c, a2 = 1 - alpha / A;
    return { (float)(b0/a0), (float)(b1/a0), (float)(b2/a0), (float)(a1/a0), (float)(a2/a0) };
}

void RetroTraxProcessor::prepareToPlay (double sampleRate, int)
{
    engine.prepare (sampleRate);
    tfmx.prepare (sampleRate);
    for (auto& ch : eqZ) for (auto& f : ch) { f[0] = 0.0f; f[1] = 0.0f; }

    // Master-FX vorbereiten: Echo-Verzoegerungsspeicher (bis 2 s) + Hall.
    fxSampleRate = sampleRate;
    echoBuf.setSize (2, juce::jmax (1, (int) (sampleRate * 2.0)));
    echoBuf.clear();
    echoWrite = 0;
    reverb.setSampleRate (sampleRate);
    reverb.reset();
}

// Master-FX auf den fertigen Stereo-Mix: erst Echo (Delay mit Rueckkopplung),
// dann Hall. Beide standardmaessig AUS (Mix 0) -> der Klang bleibt unveraendert.
void RetroTraxProcessor::applyMasterFx (juce::AudioBuffer<float>& buffer)
{
    const int n  = buffer.getNumSamples();
    const int ch = buffer.getNumChannels();
    if (n <= 0 || ch <= 0)
        return;

    // --- Echo (Stereo-Delay mit Rueckkopplung) -------------------------------
    const float mix = echoMix.load();
    if (mix > 0.0001f && echoBuf.getNumSamples() > 1)
    {
        const int   size = echoBuf.getNumSamples();
        const float fb   = juce::jlimit (0.0f, 0.95f, echoFeedback.load());
        int   delay = (int) (echoTimeMs.load() * 0.001f * (float) fxSampleRate);
        delay = juce::jlimit (1, size - 1, delay);

        for (int c = 0; c < juce::jmin (ch, 2); ++c)
        {
            float* data = buffer.getWritePointer (c);
            float* line = echoBuf.getWritePointer (c);
            int    w    = echoWrite;
            for (int i = 0; i < n; ++i)
            {
                int r = w - delay; if (r < 0) r += size;
                const float echoed = line[r];
                const float in     = data[i];
                line[w] = in + echoed * fb;          // Rueckkopplung in die Leitung
                data[i] = in * (1.0f - mix) + echoed * mix;
                if (++w >= size) w = 0;
            }
            if (c == juce::jmin (ch, 2) - 1)
                echoWrite = w; // Schreibzeiger nach dem letzten Kanal uebernehmen
        }
    }

    // --- Hall (Reverb) -------------------------------------------------------
    if (reverbMix.load() > 0.0001f)
    {
        juce::Reverb::Parameters p;
        p.roomSize = juce::jlimit (0.0f, 1.0f, reverbSize.load());
        p.damping  = 0.5f;
        p.wetLevel = reverbMix.load();
        p.dryLevel = 1.0f - reverbMix.load() * 0.5f;
        p.width    = 1.0f;
        reverb.setParameters (p);
        if (ch >= 2)
            reverb.processStereo (buffer.getWritePointer (0), buffer.getWritePointer (1), n);
        else
            reverb.processMono (buffer.getWritePointer (0), n);
    }

    // --- 3-Band-EQ (Bass-Shelf / Mitten-Peak / Hoehen-Shelf) -----------------
    const float lo = eqLow.load(), md = eqMid.load(), hi = eqHigh.load();
    if (std::abs (lo) > 0.05f || std::abs (md) > 0.05f || std::abs (hi) > 0.05f)
    {
        const Biquad bLo = makeShelf (200.0,  lo, fxSampleRate, false);
        const Biquad bMd = makePeak  (1000.0, md, 0.9, fxSampleRate);
        const Biquad bHi = makeShelf (4000.0, hi, fxSampleRate, true);
        for (int c = 0; c < juce::jmin (ch, 2); ++c)
        {
            float* d = buffer.getWritePointer (c);
            for (int i = 0; i < n; ++i)
            {
                float x = d[i];
                x = bLo.run (x, eqZ[c][0][0], eqZ[c][0][1]);
                x = bMd.run (x, eqZ[c][1][0], eqZ[c][1][1]);
                x = bHi.run (x, eqZ[c][2][0], eqZ[c][2][1]);
                d[i] = x;
            }
        }
    }
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
        applyMasterFx (buffer);
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
    applyMasterFx (buffer);
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

void RetroTraxProcessor::makeWaveform (int slot, int type)
{
    if (slot < 0 || slot >= TrackerEngine::kInstruments)
        return;
    const int len = 600; // eine Schwingung
    auto inst = std::make_unique<TrackerEngine::Instrument>();
    inst->kind       = TrackerEngine::Instrument::Kind::Sample;
    inst->loopMode   = TrackerEngine::Instrument::Loop::Forward; // klingt dauerhaft
    inst->loopXfade  = 0.0f;
    inst->sourceRate = 261.63 * len; // Note 60 (C-5) ~ 261,6 Hz
    inst->data.setSize (1, len);
    auto* d = inst->data.getWritePointer (0);
    const double twoPi = 2.0 * juce::MathConstants<double>::pi;
    const char* names[5] = { "Sinus", "Saege", "Rechteck", "Dreieck", "Puls" };
    for (int i = 0; i < len; ++i)
    {
        const double ph = (double) i / (double) len; // 0..1
        float v = 0.0f;
        switch (type)
        {
            case 1:  v = (float) (2.0 * ph - 1.0); break;            // Saege
            case 2:  v = ph < 0.5 ? 1.0f : -1.0f; break;            // Rechteck
            case 3:  v = (float) (1.0 - 4.0 * std::abs (ph - 0.5)); break; // Dreieck
            case 4:  v = ph < 0.25 ? 1.0f : -1.0f; break;           // Puls 25%
            case 0:
            default: v = (float) std::sin (twoPi * ph); break;       // Sinus
        }
        d[i] = v * 0.9f;
    }
    inst->name = names[juce::jlimit (0, 4, type)];
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
    out.loopStart     = p->loopStart;
    out.drive         = p->drive;
    out.vintagePitch  = p->vintagePitch;
    out.tuneSemis     = p->tuneSemis;
    out.ampEnv        = p->ampEnv;
    out.gain          = p->gain;
    out.attack        = p->attack;
    out.decay         = p->decay;
    out.sustain       = p->sustain;
    out.release       = p->release;
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
    out.loopStart     = p->loopStart;
    out.drive         = p->drive;
    out.vintagePitch  = p->vintagePitch;
    out.sourceRate    = p->sourceRate;
    out.tuneSemis     = p->tuneSemis;
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

bool RetroTraxProcessor::saveKit (const juce::File& file, juce::String& message)
{
    auto xml = std::make_unique<juce::XmlElement> ("RETROKIT");
    int n = 0;
    {
        const juce::ScopedLock sl (engine.lock);
        for (int p = 0; p < TrackerEngine::kPads; ++p)
        {
            const auto* pad = engine.getPad (p);
            if (pad == nullptr || pad->data.getNumSamples() < 2)
                continue;
            auto* e = xml->createNewChildElement ("PAD");
            e->setAttribute ("idx", p);
            e->setAttribute ("name", pad->name);
            e->setAttribute ("dch", pad->data.getNumChannels());
            e->setAttribute ("dlen", pad->data.getNumSamples());
            e->setAttribute ("rate", pad->sourceRate);
            e->setAttribute ("tune", pad->tuneSemis);
            e->setAttribute ("ak12", pad->akai12bit ? 1 : 0);
            e->setAttribute ("srr", pad->srReduction);
            e->setAttribute ("vint", pad->vintagePitch ? 1 : 0);
            e->createNewChildElement ("D")->addTextElement (encodeSamples (pad->data));
            ++n;
        }
    }
    if (n == 0) { message = "Das Kit ist leer."; return false; }
    if (! xml->writeTo (file, {})) { message = "Kit speichern fehlgeschlagen."; return false; }
    message = juce::String (n) + " Pads als Kit gespeichert.";
    return true;
}

bool RetroTraxProcessor::loadKit (const juce::File& file, juce::String& message)
{
    auto xml = juce::XmlDocument::parse (file);
    if (xml == nullptr || ! xml->hasTagName ("RETROKIT"))
    {
        message = "Keine gueltige Kit-Datei.";
        return false;
    }
    for (int p = 0; p < TrackerEngine::kPads; ++p)
        engine.clearPad (p);

    int n = 0;
    for (auto* e : xml->getChildIterator())
    {
        if (! e->hasTagName ("PAD"))
            continue;
        const int idx = e->getIntAttribute ("idx", -1);
        auto* d = e->getChildByName ("D");
        if (idx < 0 || idx >= TrackerEngine::kPads || d == nullptr)
            continue;
        juce::AudioBuffer<float> buf;
        if (! decodeSamples (d->getAllSubText(), e->getIntAttribute ("dch", 1),
                             e->getIntAttribute ("dlen", 0), buf))
            continue;
        auto inst = std::make_unique<TrackerEngine::Instrument>();
        inst->kind       = TrackerEngine::Instrument::Kind::Sample;
        inst->data       = std::move (buf);
        inst->sourceRate = e->getDoubleAttribute ("rate", 8287.0);
        inst->name       = e->getStringAttribute ("name", "Pad");
        engine.setPad (idx, std::move (inst));
        {
            const juce::ScopedLock sl (engine.lock);
            if (auto* pad = const_cast<TrackerEngine::Instrument*> (engine.getPad (idx)))
            {
                pad->tuneSemis    = (float) e->getDoubleAttribute ("tune", 0.0);
                pad->akai12bit    = e->getIntAttribute ("ak12", 0) != 0;
                pad->srReduction  = (float) e->getDoubleAttribute ("srr", 0.0);
                pad->vintagePitch = e->getIntAttribute ("vint", 0) != 0;
            }
        }
        ++n;
    }
    message = juce::String (n) + " Pads aus dem Kit geladen.";
    return n > 0;
}

// --- Fairlight-Sample-Werkzeug ------------------------------------------------

bool RetroTraxProcessor::getSampleCopy (int slot, juce::AudioBuffer<float>& out, double& rate) const
{
    const juce::ScopedLock sl (engine.lock);
    if (slot < 0 || slot >= TrackerEngine::kInstruments)
        return false;
    const auto& p = engine.instruments[slot];
    if (p == nullptr || p->kind != TrackerEngine::Instrument::Kind::Sample
        || p->data.getNumSamples() < 2)
        return false;
    out  = p->data; // tiefe Kopie
    rate = p->sourceRate;
    return true;
}

bool RetroTraxProcessor::applyEditedSample (int slot, const juce::AudioBuffer<float>& buf,
                                            double rate, juce::String& message, bool loop, float loopStart)
{
    if (slot < 0 || slot >= TrackerEngine::kInstruments || buf.getNumSamples() < 2)
    {
        message = "Kein bearbeitbares Sample.";
        return false;
    }
    auto dir = juce::File::getSpecialLocation (juce::File::userMusicDirectory)
                   .getChildFile ("RetroTrax").getChildFile ("Bearbeitet");
    dir.createDirectory();
    const auto stamp = juce::Time::getCurrentTime().formatted ("%Y%m%d-%H%M%S");
    const auto file  = dir.getChildFile ("edit-" + stamp + ".wav");
    if (! writeWavBuffer (file, buf, rate) || ! loadInstrument (slot, file))
    {
        message = "Konnte das bearbeitete Sample nicht sichern.";
        return false;
    }
    // One-Shot oder Loop ans Instrument anlegen.
    {
        const juce::ScopedLock sl (engine.lock);
        if (auto& ip = engine.instruments[slot])
        {
            ip->loopMode  = loop ? TrackerEngine::Instrument::Loop::Forward
                                 : TrackerEngine::Instrument::Loop::Off;
            ip->loopStart = juce::jlimit (0.0f, 0.99f, loopStart);
        }
    }
    message = juce::String ("Bearbeitetes Sample in Slot ") + juce::String (slot + 1)
            + (loop ? " uebernommen (Loop)." : " uebernommen (One-Shot).");
    return true;
}

bool RetroTraxProcessor::exportSample (const juce::AudioBuffer<float>& buf, double rate,
                                       const juce::File& file, juce::String& message)
{
    if (buf.getNumSamples() < 2)
    {
        message = "Kein Sample zum Speichern.";
        return false;
    }
    if (! writeWavBuffer (file.withFileExtension ("wav"), buf, rate))
    {
        message = "Speichern fehlgeschlagen.";
        return false;
    }
    message = "Sample gespeichert: " + file.withFileExtension ("wav").getFileName();
    return true;
}

int RetroTraxProcessor::chopToKit (const juce::AudioBuffer<float>& buf, double rate, int slices,
                                   const juce::String& baseName, juce::String& message)
{
    slices = juce::jlimit (1, TrackerEngine::kPads, slices);
    const int total = buf.getNumSamples();
    if (total < slices * 2)
    {
        message = "Sample zu kurz zum Schneiden.";
        return 0;
    }
    auto safe = juce::File::createLegalFileName (baseName);
    if (safe.isEmpty()) safe = "Chop";
    auto dir = juce::File::getSpecialLocation (juce::File::userMusicDirectory)
                   .getChildFile ("RetroTrax").getChildFile ("Chops").getChildFile (safe);
    dir.createDirectory();

    const int ch  = buf.getNumChannels();
    const int len = total / slices;
    int filled = 0;
    for (int s = 0; s < slices; ++s)
    {
        const int start = s * len;
        const int n     = (s == slices - 1) ? (total - start) : len; // Rest in die letzte Scheibe
        juce::AudioBuffer<float> slice (ch, n);
        for (int c = 0; c < ch; ++c)
            slice.copyFrom (c, 0, buf, c, start, n);
        const auto file = dir.getChildFile (juce::String::formatted ("%02d ", s + 1) + safe + ".wav");
        if (writeWavBuffer (file, slice, rate) && loadPad (s, file))
            ++filled;
    }
    message = juce::String (filled) + " Scheiben auf die Kit-Pads gelegt (\"" + safe + "\").";
    return filled;
}

int RetroTraxProcessor::sliceToPattern (const juce::AudioBuffer<float>& buf, double rate, int slices,
                                        const juce::String& baseName, juce::String& message)
{
    slices = juce::jlimit (2, 16, slices);
    const int total = buf.getNumSamples();
    if (total < slices * 2)
    {
        message = "Sample zu kurz zum Schneiden.";
        return 0;
    }

    int base = currentInstrument.load();
    if (base < 0 || base + slices > TrackerEngine::kInstruments)
        base = 0; // genug aufeinanderfolgende Slots ab vorne
    slices = juce::jmin (slices, TrackerEngine::kInstruments - base);

    auto safe = juce::File::createLegalFileName (baseName);
    if (safe.isEmpty()) safe = "Slice";
    auto dir = juce::File::getSpecialLocation (juce::File::userMusicDirectory)
                   .getChildFile ("RetroTrax").getChildFile ("Chops").getChildFile (safe + " (Pattern)");
    dir.createDirectory();

    const int ch  = buf.getNumChannels();
    const int len = total / slices;
    int filled = 0;
    for (int s = 0; s < slices; ++s)
    {
        const int start = s * len;
        const int n     = (s == slices - 1) ? (total - start) : len;
        juce::AudioBuffer<float> slice (ch, n);
        for (int c = 0; c < ch; ++c)
            slice.copyFrom (c, 0, buf, c, start, n);
        const auto file = dir.getChildFile (juce::String::formatted ("%02d ", s + 1) + safe + ".wav");
        if (writeWavBuffer (file, slice, rate) && loadInstrument (base + s, file))
            ++filled;
    }

    // Noten ins aktuelle Pattern schreiben (Spur 1), gleichmaessig verteilt.
    {
        const juce::ScopedLock sl (engine.lock);
        const int pat   = engine.editPattern.load();
        const int track = 0;
        for (int r = 0; r < TrackerEngine::kRows; ++r)
            engine.patterns[pat][r][track] = TrackerEngine::Cell();
        const int interval = juce::jmax (1, TrackerEngine::kRows / slices);
        for (int s = 0; s < filled; ++s)
        {
            const int row = s * interval;
            if (row >= TrackerEngine::kRows) break;
            auto& c = engine.patterns[pat][row][track];
            c.note       = 60;          // C-5 = Originaltonhoehe der Scheibe
            c.instrument = base + s;
        }
    }

    message = juce::String (filled) + " Scheiben in Slots " + juce::String (base + 1)
            + ".." + juce::String (base + filled) + " + als Noten ins Pattern (Spur 1).";
    return filled;
}

void RetroTraxProcessor::previewBuffer (const juce::AudioBuffer<float>& buf, double rate, bool loop,
                                        float loopStart)
{
    if (buf.getNumSamples() < 2)
        return;
    auto inst = std::make_unique<TrackerEngine::Instrument>();
    inst->kind       = TrackerEngine::Instrument::Kind::Sample;
    inst->data       = buf;
    inst->sourceRate = rate;
    inst->name       = "Vorschau";
    inst->loopMode   = loop ? TrackerEngine::Instrument::Loop::Forward
                            : TrackerEngine::Instrument::Loop::Off;
    inst->loopStart  = juce::jlimit (0.0f, 0.99f, loopStart);
    engine.previewInstrument (std::move (inst));
}

// --- Selbst-enthaltenes .retrotrax: Sample-Daten einbetten -------------------
// Samples werden als 16-Bit-PCM, GZIP-komprimiert und Base64-kodiert direkt in
// die Song-Datei geschrieben -> eine portable Mini-Datei (Games/Demos/Web), und
// auch gezeichnete/gegrabbte Samples ohne Quelldatei bleiben erhalten.
static juce::String encodeSamples (const juce::AudioBuffer<float>& buf)
{
    const int ch = buf.getNumChannels();
    const int n  = buf.getNumSamples();
    juce::MemoryBlock pcm ((size_t) ch * (size_t) n * 2);
    auto* out = (int16_t*) pcm.getData();
    for (int i = 0; i < n; ++i)
        for (int c = 0; c < ch; ++c)
            *out++ = (int16_t) juce::jlimit (-32767, 32767,
                        (int) std::lround (juce::jlimit (-1.0f, 1.0f, buf.getReadPointer (c)[i]) * 32767.0f));

    juce::MemoryBlock gz;
    {
        juce::MemoryOutputStream gzo (gz, false);
        juce::GZIPCompressorOutputStream comp (gzo, 9);
        comp.write (pcm.getData(), pcm.getSize());
    } // comp + gzo hier zerstoert -> gz ist vollstaendig
    return juce::Base64::toBase64 (gz.getData(), gz.getSize());
}

static bool decodeSamples (const juce::String& b64, int ch, int n, juce::AudioBuffer<float>& out)
{
    if (ch < 1 || n < 1 || b64.isEmpty())
        return false;
    juce::MemoryOutputStream gz;
    if (! juce::Base64::convertFromBase64 (gz, b64))
        return false;
    juce::MemoryInputStream gzin (gz.getData(), gz.getDataSize(), false);
    juce::GZIPDecompressorInputStream dec (gzin);
    juce::MemoryBlock pcm;
    dec.readIntoMemoryBlock (pcm);
    if ((int) pcm.getSize() < ch * n * 2)
        return false;
    const auto* s = (const int16_t*) pcm.getData();
    out.setSize (ch, n);
    for (int i = 0; i < n; ++i)
        for (int c = 0; c < ch; ++c)
            out.getWritePointer (c)[i] = (float) s[i * ch + c] / 32768.0f;
    return true;
}

std::unique_ptr<juce::XmlElement> RetroTraxProcessor::stateToXml()
{
    auto xml = std::make_unique<juce::XmlElement> ("RETROTRAX");
    xml->setAttribute ("version", 1);
    xml->setAttribute ("bpm", (double) engine.bpm.load());
    xml->setAttribute ("swing", (double) engine.swing.load());
    xml->setAttribute ("instrument", currentInstrument.load());
    xml->setAttribute ("octave", currentOctave.load());
    xml->setAttribute ("echoTime", (double) echoTimeMs.load());
    xml->setAttribute ("echoFb",   (double) echoFeedback.load());
    xml->setAttribute ("echoMix",  (double) echoMix.load());
    xml->setAttribute ("revSize",  (double) reverbSize.load());
    xml->setAttribute ("revMix",   (double) reverbMix.load());
    xml->setAttribute ("eqLow",    (double) eqLow.load());
    xml->setAttribute ("eqMid",    (double) eqMid.load());
    xml->setAttribute ("eqHigh",   (double) eqHigh.load());

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
        else if (ip->kind == TrackerEngine::Instrument::Kind::Sample
                 && ip->data.getNumSamples() > 1)
        {
            auto* e = xml->createNewChildElement ("INSTRUMENT");
            e->setAttribute ("slot", i);
            e->setAttribute ("name", ip->name);
            if (ip->filePath.isNotEmpty())
                e->setAttribute ("path", ip->filePath); // Quellpfad als Hinweis
            // Selbst-enthaltend: Sample-Daten komprimiert einbetten.
            e->setAttribute ("dch", ip->data.getNumChannels());
            e->setAttribute ("dlen", ip->data.getNumSamples());
            e->setAttribute ("drate", ip->sourceRate);
            e->createNewChildElement ("D")->addTextElement (encodeSamples (ip->data));
            // Akai-Filter nur sichern, wenn er ueberhaupt benutzt wird -> normale
            // Sample-Slots bleiben in der Datei unveraendert (alte Songs laden weiter).
            if (ip->akaiOn || ip->akai12bit || ip->reverse || ip->srReduction > 0.0f
                || ip->loopMode != TrackerEngine::Instrument::Loop::Off
                || ip->loopXfade > 0.0f
                || ip->drive > 0.0f || ip->vintagePitch || ip->tuneSemis != 0.0f
                || ip->ampEnv || ip->gain != 1.0f)
            {
                e->setAttribute ("akon", ip->akaiOn ? 1 : 0);
                e->setAttribute ("akcut", ip->akaiCutoff);
                e->setAttribute ("akres", ip->akaiResonance);
                e->setAttribute ("ak12", ip->akai12bit ? 1 : 0);
                e->setAttribute ("rev", ip->reverse ? 1 : 0);
                e->setAttribute ("srr", ip->srReduction);
                e->setAttribute ("loop", (int) ip->loopMode);
                e->setAttribute ("lxf", ip->loopXfade);
                e->setAttribute ("lst", ip->loopStart);
                e->setAttribute ("drv", ip->drive);
                e->setAttribute ("vint", ip->vintagePitch ? 1 : 0);
                e->setAttribute ("tune", ip->tuneSemis);
                e->setAttribute ("aenv", ip->ampEnv ? 1 : 0);
                e->setAttribute ("gain", ip->gain);
                e->setAttribute ("a", ip->attack);
                e->setAttribute ("d", ip->decay);
                e->setAttribute ("s", ip->sustain);
                e->setAttribute ("rel", ip->release);
            }
        }
    }

    // Drum-Kit-Pads (16): Sample-Daten ebenfalls selbst-enthaltend einbetten,
    // plus der Sampler-Charakter (12-Bit/Loop/Drive...).
    for (int p = 0; p < TrackerEngine::kPads; ++p)
    {
        const juce::ScopedLock sl (engine.lock);
        const auto* pad = engine.getPad (p);
        if (pad == nullptr || pad->data.getNumSamples() < 2)
            continue;
        auto* e = xml->createNewChildElement ("PAD");
        e->setAttribute ("idx", p);
        e->setAttribute ("name", pad->name);
        if (pad->filePath.isNotEmpty())
            e->setAttribute ("path", pad->filePath);
        e->setAttribute ("dch", pad->data.getNumChannels());
        e->setAttribute ("dlen", pad->data.getNumSamples());
        e->createNewChildElement ("D")->addTextElement (encodeSamples (pad->data));
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
        e->setAttribute ("tune", pad->tuneSemis);
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
    engine.swing = (float) xml.getDoubleAttribute ("swing", 0.0);
    currentInstrument = juce::jlimit (0, TrackerEngine::kInstruments - 1,
                                      xml.getIntAttribute ("instrument", 0));
    currentOctave = juce::jlimit (1, 8, xml.getIntAttribute ("octave", 5));
    echoTimeMs   = (float) xml.getDoubleAttribute ("echoTime", 300.0);
    echoFeedback = (float) xml.getDoubleAttribute ("echoFb",   0.35);
    echoMix      = (float) xml.getDoubleAttribute ("echoMix",  0.0);
    reverbSize   = (float) xml.getDoubleAttribute ("revSize",  0.5);
    reverbMix    = (float) xml.getDoubleAttribute ("revMix",   0.0);
    eqLow        = (float) xml.getDoubleAttribute ("eqLow",    0.0);
    eqMid        = (float) xml.getDoubleAttribute ("eqMid",    0.0);
    eqHigh       = (float) xml.getDoubleAttribute ("eqHigh",   0.0);

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

            // Bevorzugt: eingebettete Sample-Daten (selbst-enthaltend). Sonst
            // Rueckfall auf den Dateipfad (alte Songs / externe Referenz).
            bool loaded = false;
            if (auto* dEl = e->getChildByName ("D"))
            {
                juce::AudioBuffer<float> buf;
                if (decodeSamples (dEl->getAllSubText(),
                                   e->getIntAttribute ("dch", 1),
                                   e->getIntAttribute ("dlen", 0), buf))
                {
                    auto inst = std::make_unique<TrackerEngine::Instrument>();
                    inst->kind       = TrackerEngine::Instrument::Kind::Sample;
                    inst->data       = std::move (buf);
                    inst->sourceRate = e->getDoubleAttribute ("drate", 44100.0);
                    inst->name       = e->getStringAttribute ("name", "Sample");
                    inst->filePath   = e->getStringAttribute ("path");
                    engine.setInstrument (slot, std::move (inst));
                    loaded = true;
                }
            }
            if (! loaded)
            {
                const juce::File f (e->getStringAttribute ("path"));
                if (f.existsAsFile())
                    loaded = loadInstrument (slot, f);
                else if (missingSamples != nullptr)
                {
                    auto name = e->getStringAttribute ("name");
                    if (name.isEmpty())
                        name = f.getFileNameWithoutExtension();
                    missingSamples->add (name);
                }
            }
            if (loaded)
            {
                // Akai-Filter/Charakter auf den geladenen Slot legen (egal ob
                // eingebettet oder aus Datei; fehlen sie -> Standard AUS).
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
                    ip->loopStart     = (float) e->getDoubleAttribute ("lst", 0.0);
                    ip->drive         = (float) e->getDoubleAttribute ("drv", 0.0);
                    ip->vintagePitch  = e->getIntAttribute ("vint", 0) != 0;
                    ip->tuneSemis     = (float) e->getDoubleAttribute ("tune", 0.0);
                    ip->ampEnv        = e->getIntAttribute ("aenv", 0) != 0;
                    ip->gain          = (float) e->getDoubleAttribute ("gain", 1.0);
                    ip->attack        = (float) e->getDoubleAttribute ("a", 0.004);
                    ip->decay         = (float) e->getDoubleAttribute ("d", 0.18);
                    ip->sustain       = (float) e->getDoubleAttribute ("s", 0.65);
                    ip->release       = (float) e->getDoubleAttribute ("rel", 0.25);
                }
            }
        }
        else if (e->hasTagName ("PAD"))
        {
            const int idx = e->getIntAttribute ("idx", -1);
            bool padOk = false;
            if (idx >= 0 && idx < TrackerEngine::kPads)
            {
                if (auto* dEl = e->getChildByName ("D"))
                {
                    juce::AudioBuffer<float> buf;
                    if (decodeSamples (dEl->getAllSubText(),
                                       e->getIntAttribute ("dch", 1),
                                       e->getIntAttribute ("dlen", 0), buf))
                    {
                        auto inst = std::make_unique<TrackerEngine::Instrument>();
                        inst->kind       = TrackerEngine::Instrument::Kind::Sample;
                        inst->data       = std::move (buf);
                        inst->sourceRate = e->getDoubleAttribute ("rate", 8287.0);
                        inst->name       = e->getStringAttribute ("name", "Pad");
                        inst->filePath   = e->getStringAttribute ("path");
                        engine.setPad (idx, std::move (inst));
                        padOk = true;
                    }
                }
                if (! padOk)
                {
                    const juce::File f (e->getStringAttribute ("path"));
                    padOk = f.existsAsFile() && loadPad (idx, f);
                }
            }
            if (padOk)
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
                    pad->tuneSemis     = (float) e->getDoubleAttribute ("tune", 0.0);
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

            // Kuerzer als 64 Zeilen? Impliziter Pattern-Break am letzten Takt auf
            // einer freien Effekt-Zelle -> die Wiedergabe laeuft nicht durch leere
            // Zeilen, sondern springt originalgetreu weiter (XM/S3M/IT mit <64).
            if (prows > 0 && prows < TrackerEngine::kRows)
                for (int c = 0; c < nch; ++c)
                    if (engine.patterns[p][prows - 1][c].effect < 0)
                    {
                        engine.patterns[p][prows - 1][c].effect      = 0xD;
                        engine.patterns[p][prows - 1][c].effectParam = 0;
                        break;
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

            // Kuerzer als 64 Zeilen? Impliziter Pattern-Break am letzten Takt auf
            // einer freien Effekt-Zelle -> die Wiedergabe laeuft nicht durch leere
            // Zeilen, sondern springt originalgetreu weiter (XM/S3M/IT mit <64).
            if (prows > 0 && prows < TrackerEngine::kRows)
                for (int c = 0; c < nch; ++c)
                    if (engine.patterns[p][prows - 1][c].effect < 0)
                    {
                        engine.patterns[p][prows - 1][c].effect      = 0xD;
                        engine.patterns[p][prows - 1][c].effectParam = 0;
                        break;
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
