#pragma once
//
// rt_rtx.h - kompaktes Binaerformat ".rtx" (packen + lesen), JUCE-frei.
//
// Das lesbare `.retrotrax` (XML) BLEIBT das Arbeits-/Austauschformat. Daneben
// steht `.rtx` als gepackter Export fuer den Replayer: dieselbe Musik, nur
// klein genug zum Mitschicken in Spielen, Demos und im Web.
//
// Wo die Groesse wirklich herkommt (gemessen am Demo-Song, 44 KB XML):
//   - 54% der Datei sind Base64 der Sample-Daten. Base64 blaeht um 33% auf,
//     verschwindet aber fast komplett, sobald man die Datei einmal gzippt.
//   - Die Pattern-Zellen sind als XML riesig (~58 Zeichen pro Zelle), packen
//     sich aber ebenfalls sehr gut.
//   => Ein blosser Binaer-Container gewinnt gegen "XML + gzip" kaum etwas
//      (19.4 KB gegen 19.9 KB). Der echte Hebel sind die Sample-DATEN selbst.
//
// Darum speichert .rtx Samples mit waehlbarer Aufloesung (Codec pro Sample):
//   Codec 0 = 16-bit PCM  (immer verlustfrei)
//   Codec 1 =  8-bit PCM  (verlustfrei NUR wenn das Material ohnehin 8-bit ist)
// Beim Packen wird pro Sample geprueft, ob alle Werte auf dem 8-bit-Raster
// liegen (bei Amiga-/ST-01-Samples ist das zu 100% der Fall) - dann wird
// automatisch 8-bit gespeichert, ohne einen einzigen Bit Verlust. Sonst 16-bit.
// So wird nie etwas verschlechtert, was vorher besser war.
//
// Aufbau der Datei (alles little-endian):
//
//   "RTX1"            4 Bytes Kennung
//   u8   version      = 1
//   u8   flags        (reserviert, 0)
//   u16  sampleCount
//   u32  structRawLen / u32 structZLen / structZLen Bytes  (zlib)
//   je Sample: u8 codec, u8 channels, u32 frames, u32 rawLen, u32 zLen, Daten
//
// Der Struktur-Block (entpackt) enthaelt Tempo, Reihenfolge, alle Instrumente
// (Synth-Parameter bzw. Sampler-Charakter) und die belegten Pattern-Zellen -
// genau die Felder, die rt_load.h aus dem XML liest.

#include "TrackerEngine.h"

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

// zlib: der JUCE-freie Replayer nimmt die System-zlib direkt. Im Plugin-Build
// gibt es die nicht auf dem Include-Pfad (JUCE bringt zlib zwar mit, aber ohne
// oeffentliche Header) - dort laufen wir ueber JUCEs eigene Streams. Beide Wege
// erzeugen/lesen dasselbe zlib-Format (RFC1950, windowBits=0 -> erstes Byte 0x78),
// die Dateien sind also zwischen Plugin und Replayer austauschbar.
#ifdef RETROTRAX_NO_JUCE
  #include <zlib.h>
#endif

namespace rtrtx
{
    static constexpr uint8_t kVersion = 1;

    // --- kleine Schreib-/Lesehelfer -----------------------------------------
    struct Writer
    {
        std::vector<uint8_t> b;

        void u8v  (uint8_t v)  { b.push_back (v); }
        void u16v (uint16_t v) { b.push_back ((uint8_t) (v & 0xFF)); b.push_back ((uint8_t) (v >> 8)); }
        void i16v (int v)      { u16v ((uint16_t) (int16_t) v); }
        void u32v (uint32_t v) { for (int i = 0; i < 4; ++i) b.push_back ((uint8_t) ((v >> (8 * i)) & 0xFF)); }
        void f32v (float v)    { uint32_t t; std::memcpy (&t, &v, 4); u32v (t); }
        void f64v (double v)
        {
            uint64_t t; std::memcpy (&t, &v, 8);
            for (int i = 0; i < 8; ++i) b.push_back ((uint8_t) ((t >> (8 * i)) & 0xFF));
        }
        void str (const std::string& s)
        {
            const size_t n = s.size() > 255 ? 255 : s.size();
            u8v ((uint8_t) n);
            b.insert (b.end(), s.begin(), s.begin() + (long) n);
        }
        void raw (const uint8_t* p, size_t n) { b.insert (b.end(), p, p + n); }
    };

    struct Reader
    {
        const uint8_t* p = nullptr;
        size_t n = 0, i = 0;
        bool bad = false;

        Reader (const uint8_t* d, size_t len) : p (d), n (len) {}

        bool need (size_t k) { if (i + k > n) { bad = true; return false; } return true; }

        uint8_t  u8v()  { if (! need (1)) return 0; return p[i++]; }
        uint16_t u16v() { if (! need (2)) return 0; uint16_t v = (uint16_t) (p[i] | (p[i+1] << 8)); i += 2; return v; }
        int      i16v() { return (int) (int16_t) u16v(); }
        uint32_t u32v()
        {
            if (! need (4)) return 0;
            uint32_t v = 0;
            for (int k = 0; k < 4; ++k) v |= (uint32_t) p[i + (size_t) k] << (8 * k);
            i += 4; return v;
        }
        float f32v()  { uint32_t t = u32v(); float v; std::memcpy (&v, &t, 4); return v; }
        double f64v()
        {
            if (! need (8)) return 0.0;
            uint64_t t = 0;
            for (int k = 0; k < 8; ++k) t |= (uint64_t) p[i + (size_t) k] << (8 * k);
            i += 8; double v; std::memcpy (&v, &t, 8); return v;
        }
        std::string str()
        {
            const size_t k = u8v();
            if (! need (k)) return {};
            std::string s ((const char*) p + i, k);
            i += k; return s;
        }
    };

    // --- zlib ---------------------------------------------------------------
#ifdef RETROTRAX_NO_JUCE
    inline std::vector<uint8_t> deflateBytes (const std::vector<uint8_t>& in)
    {
        uLongf cap = compressBound ((uLong) in.size());
        std::vector<uint8_t> out (cap);
        if (compress2 (out.data(), &cap, in.data(), (uLong) in.size(), 9) != Z_OK)
            return {};
        out.resize (cap);
        return out;
    }

    inline bool inflateBytes (const uint8_t* src, size_t srcLen,
                              std::vector<uint8_t>& out, size_t expected)
    {
        out.resize (expected);
        uLongf destLen = (uLongf) expected;
        const int r = uncompress (out.data(), &destLen, src, (uLong) srcLen);
        return r == Z_OK && destLen == (uLongf) expected;
    }
#else
    inline std::vector<uint8_t> deflateBytes (const std::vector<uint8_t>& in)
    {
        juce::MemoryOutputStream mo;
        {
            // windowBits=0 -> zlib-Format (genau wie die <D>-Bloecke im XML)
            juce::GZIPCompressorOutputStream gz (mo, 9, 0);
            gz.write (in.data(), in.size());
        }   // erst der Destruktor schreibt den Rest raus
        const auto* p = (const uint8_t*) mo.getData();
        return std::vector<uint8_t> (p, p + mo.getDataSize());
    }

    inline bool inflateBytes (const uint8_t* src, size_t srcLen,
                              std::vector<uint8_t>& out, size_t expected)
    {
        if (expected == 0) { out.clear(); return true; }
        juce::MemoryInputStream mi (src, srcLen, false);
        juce::GZIPDecompressorInputStream gz (&mi, false,
                                              juce::GZIPDecompressorInputStream::zlibFormat,
                                              (juce::int64) expected);
        out.resize (expected);
        return gz.read (out.data(), (int) expected) == (int) expected;
    }
#endif

    // --- Sample-Daten <-> int16 ---------------------------------------------
    // Die Engine haelt Samples als float = int16 / 32768 (siehe rt_sample.h).
    // Der Rueckweg ist exakt: alle Werte k/32768 mit |k| <= 32768 sind in float
    // verlustfrei darstellbar, das Runden trifft also wieder genau k.
    inline int16_t toI16 (float f)
    {
        float v = f * 32768.0f;
        if (v >  32767.0f) v =  32767.0f;
        if (v < -32768.0f) v = -32768.0f;
        return (int16_t) (v < 0.0f ? v - 0.5f : v + 0.5f);
    }

    // --- Packen: Engine -> .rtx ---------------------------------------------
    inline std::vector<uint8_t> pack (const TrackerEngine& engine, bool force16bit = false)
    {
        using Inst = TrackerEngine::Instrument;

        // 1) Sample-Bloecke einsammeln (ein Block je Sample-Instrument mit Daten)
        struct Blob { uint8_t codec; uint8_t channels; uint32_t frames; std::vector<uint8_t> z; uint32_t rawLen; };
        std::vector<Blob> blobs;
        int sampleIndexOf[TrackerEngine::kInstruments];
        for (int i = 0; i < TrackerEngine::kInstruments; ++i) sampleIndexOf[i] = -1;

        for (int slot = 0; slot < TrackerEngine::kInstruments; ++slot)
        {
            const Inst* in = engine.instruments[slot].get();
            if (in == nullptr || in->kind != Inst::Kind::Sample) continue;
            const int ch = in->data.getNumChannels();
            const int nf = in->data.getNumSamples();
            if (ch < 1 || nf < 1) continue;

            // interleaved int16 wie im Plugin/XML
            std::vector<int16_t> pcm ((size_t) ch * (size_t) nf);
            bool eightBitClean = true;
            for (int f = 0; f < nf; ++f)
                for (int c = 0; c < ch; ++c)
                {
                    const int16_t v = toI16 (in->data.getReadPointer (c)[f]);
                    pcm[(size_t) f * (size_t) ch + (size_t) c] = v;
                    if ((v & 0xFF) != 0) eightBitClean = false;
                }

            Blob bl;
            bl.channels = (uint8_t) ch;
            bl.frames   = (uint32_t) nf;
            std::vector<uint8_t> rawBytes;
            if (eightBitClean && ! force16bit)
            {
                // Material liegt ohnehin auf dem 8-bit-Raster -> verlustfrei halbieren
                bl.codec = 1;
                rawBytes.resize (pcm.size());
                for (size_t k = 0; k < pcm.size(); ++k)
                    rawBytes[k] = (uint8_t) (((pcm[k] >> 8) & 0xFF));
            }
            else
            {
                bl.codec = 0;
                rawBytes.resize (pcm.size() * 2);
                std::memcpy (rawBytes.data(), pcm.data(), rawBytes.size());
            }
            bl.rawLen = (uint32_t) rawBytes.size();
            bl.z = deflateBytes (rawBytes);
            sampleIndexOf[slot] = (int) blobs.size();
            blobs.push_back (std::move (bl));
        }

        // 2) Struktur-Block
        Writer s;
        s.f32v (engine.bpm.load());
        s.f32v (engine.swing.load());

        const int ol = engine.orderLen > 0 ? engine.orderLen : 1;
        s.u16v ((uint16_t) ol);
        for (int i = 0; i < ol; ++i) s.u8v ((uint8_t) engine.order[i]);

        // Instrumente
        {
            Writer instBlock;
            uint16_t count = 0;
            for (int slot = 0; slot < TrackerEngine::kInstruments; ++slot)
            {
                const Inst* in = engine.instruments[slot].get();
                if (in == nullptr) continue;
                const bool isSynth = (in->kind == Inst::Kind::Synth);
                if (! isSynth && sampleIndexOf[slot] < 0) continue;  // Sample ohne Daten -> stumm, weglassen

                ++count;
                instBlock.u8v ((uint8_t) slot);
                instBlock.u8v (isSynth ? 1 : 0);
                instBlock.str (in->name.toStdString());

                if (isSynth)
                {
                    instBlock.u8v  ((uint8_t) (int) in->engine);
                    instBlock.u8v  ((uint8_t) (int) in->wave);
                    instBlock.f32v (in->pulseWidth);
                    instBlock.f32v (in->attack);
                    instBlock.f32v (in->decay);
                    instBlock.f32v (in->sustain);
                    instBlock.f32v (in->release);
                    instBlock.u8v  ((uint8_t) (int) in->filter);
                    instBlock.f32v (in->cutoff);
                    instBlock.f32v (in->resonance);
                    instBlock.u8v  (in->ringMod ? 1 : 0);
                    instBlock.u8v  (in->sync ? 1 : 0);
                    instBlock.f32v (in->modTune);
                    instBlock.f32v (in->pwmRate);
                    instBlock.f32v (in->pwmDepth);
                    instBlock.i16v (in->unison);
                    instBlock.f32v (in->detune);
                    instBlock.i16v (in->chord);
                }
                else
                {
                    instBlock.u16v ((uint16_t) sampleIndexOf[slot]);
                    instBlock.f64v (in->sourceRate);
                    instBlock.u8v  (in->akaiOn ? 1 : 0);
                    instBlock.f32v (in->akaiCutoff);
                    instBlock.f32v (in->akaiResonance);
                    instBlock.u8v  (in->akai12bit ? 1 : 0);
                    instBlock.u8v  (in->akai8bit ? 1 : 0);
                    instBlock.u8v  (in->companding ? 1 : 0);
                    instBlock.u8v  (in->reverse ? 1 : 0);
                    instBlock.f32v (in->srReduction);
                    instBlock.u8v  ((uint8_t) (int) in->loopMode);
                    instBlock.f32v (in->loopXfade);
                    instBlock.f32v (in->loopStart);
                    instBlock.f32v (in->drive);
                    instBlock.f32v (in->tapeWow);
                    instBlock.u8v  (in->vintagePitch ? 1 : 0);
                    instBlock.f32v (in->tuneSemis);
                    instBlock.u8v  (in->ampEnv ? 1 : 0);
                    instBlock.f32v (in->gain);
                    instBlock.f32v (in->attack);
                    instBlock.f32v (in->decay);
                    instBlock.f32v (in->sustain);
                    instBlock.f32v (in->release);
                }
            }
            s.u16v (count);
            s.raw (instBlock.b.data(), instBlock.b.size());
        }

        // Pattern-Zellen: nur die belegten (Tracker = meist leere Zellen)
        {
            Writer cellBlock;
            uint32_t count = 0;
            for (int p = 0; p < TrackerEngine::kMaxPatterns; ++p)
                for (int r = 0; r < TrackerEngine::kRows; ++r)
                    for (int t = 0; t < TrackerEngine::kTracks; ++t)
                    {
                        const auto& c = engine.patterns[p][r][t];
                        if (c.note == -1 && c.instrument == -1 && c.volume == -1
                            && c.effect == -1 && c.effectParam == 0)
                            continue;
                        ++count;
                        cellBlock.u8v  ((uint8_t) p);
                        cellBlock.u8v  ((uint8_t) r);
                        cellBlock.u8v  ((uint8_t) t);
                        cellBlock.i16v (c.note);
                        cellBlock.i16v (c.instrument);
                        cellBlock.i16v (c.volume);
                        cellBlock.i16v (c.effect);
                        cellBlock.i16v (c.effectParam);
                    }
            s.u32v (count);
            s.raw (cellBlock.b.data(), cellBlock.b.size());
        }

        const std::vector<uint8_t> structZ = deflateBytes (s.b);

        // 3) Datei zusammensetzen
        Writer out;
        out.raw ((const uint8_t*) "RTX1", 4);
        out.u8v (kVersion);
        out.u8v (0);
        out.u16v ((uint16_t) blobs.size());
        out.u32v ((uint32_t) s.b.size());
        out.u32v ((uint32_t) structZ.size());
        out.raw (structZ.data(), structZ.size());
        for (const auto& bl : blobs)
        {
            out.u8v  (bl.codec);
            out.u8v  (bl.channels);
            out.u32v (bl.frames);
            out.u32v (bl.rawLen);
            out.u32v ((uint32_t) bl.z.size());
            out.raw  (bl.z.data(), bl.z.size());
        }
        return out.b;
    }

    // --- Lesen: .rtx -> Engine ----------------------------------------------
    // Gibt die Zahl geladener Instrumente zurueck (>=0) oder -1 bei Fehler.
    inline int load (const uint8_t* data, size_t len, TrackerEngine& engine)
    {
        using Inst = TrackerEngine::Instrument;
        if (data == nullptr || len < 16 || std::memcmp (data, "RTX1", 4) != 0)
            return -1;

        Reader h (data, len);
        h.i = 4;
        const uint8_t ver = h.u8v();
        h.u8v();                                  // flags
        if (ver > kVersion) return -1;            // neuer als wir -> lieber ehrlich scheitern
        const uint16_t sampleCount = h.u16v();
        const uint32_t structRawLen = h.u32v();
        const uint32_t structZLen   = h.u32v();
        if (h.bad || ! h.need (structZLen)) return -1;

        std::vector<uint8_t> structRaw;
        if (! inflateBytes (data + h.i, structZLen, structRaw, structRawLen))
            return -1;
        h.i += structZLen;

        // Sample-Bloecke einlesen (Reihenfolge = Index aus dem Struktur-Block)
        struct Blob { uint8_t codec; uint8_t channels; uint32_t frames; std::vector<uint8_t> raw; };
        std::vector<Blob> blobs;
        blobs.reserve (sampleCount);
        for (uint16_t k = 0; k < sampleCount; ++k)
        {
            Blob bl;
            bl.codec    = h.u8v();
            bl.channels = h.u8v();
            bl.frames   = h.u32v();
            const uint32_t rawLen = h.u32v();
            const uint32_t zLen   = h.u32v();
            if (h.bad || ! h.need (zLen)) return -1;
            if (! inflateBytes (data + h.i, zLen, bl.raw, rawLen)) return -1;
            h.i += zLen;
            blobs.push_back (std::move (bl));
        }

        Reader s (structRaw.data(), structRaw.size());
        engine.bpm.store   (s.f32v());
        engine.swing.store (s.f32v());

        const int ol = (int) s.u16v();
        for (int i = 0; i < ol; ++i)
        {
            const int v = (int) s.u8v();
            if (i < TrackerEngine::kMaxOrder) engine.order[i] = v;
        }
        engine.orderLen = ol > 0 ? (ol < TrackerEngine::kMaxOrder ? ol : TrackerEngine::kMaxOrder) : 1;

        int loaded = 0;
        const int instCount = (int) s.u16v();
        for (int k = 0; k < instCount && ! s.bad; ++k)
        {
            const int slot     = (int) s.u8v();
            const bool isSynth = s.u8v() != 0;
            const std::string nm = s.str();

            auto inst = std::make_unique<Inst>();
            inst->name = nm.c_str();

            if (isSynth)
            {
                inst->kind       = Inst::Kind::Synth;
                inst->engine     = (Inst::Engine) (int) s.u8v();
                inst->wave       = (Inst::Wave) (int) s.u8v();
                inst->pulseWidth = s.f32v();
                inst->attack     = s.f32v();
                inst->decay      = s.f32v();
                inst->sustain    = s.f32v();
                inst->release    = s.f32v();
                inst->filter     = (Inst::Filter) (int) s.u8v();
                inst->cutoff     = s.f32v();
                inst->resonance  = s.f32v();
                inst->ringMod    = s.u8v() != 0;
                inst->sync       = s.u8v() != 0;
                inst->modTune    = s.f32v();
                inst->pwmRate    = s.f32v();
                inst->pwmDepth   = s.f32v();
                inst->unison     = s.i16v();
                inst->detune     = s.f32v();
                inst->chord      = s.i16v();
            }
            else
            {
                inst->kind = Inst::Kind::Sample;
                const int si = (int) s.u16v();
                inst->sourceRate    = s.f64v();
                inst->akaiOn        = s.u8v() != 0;
                inst->akaiCutoff    = s.f32v();
                inst->akaiResonance = s.f32v();
                inst->akai12bit     = s.u8v() != 0;
                inst->akai8bit      = s.u8v() != 0;
                inst->companding    = s.u8v() != 0;
                inst->reverse       = s.u8v() != 0;
                inst->srReduction   = s.f32v();
                {
                    const int lm = (int) s.u8v();
                    inst->loopMode = (Inst::Loop) (lm > 2 ? 2 : lm);
                }
                inst->loopXfade     = s.f32v();
                inst->loopStart     = s.f32v();
                inst->drive         = s.f32v();
                inst->tapeWow       = s.f32v();
                inst->vintagePitch  = s.u8v() != 0;
                inst->tuneSemis     = s.f32v();
                inst->ampEnv        = s.u8v() != 0;
                inst->gain          = s.f32v();
                inst->attack        = s.f32v();
                inst->decay         = s.f32v();
                inst->sustain       = s.f32v();
                inst->release       = s.f32v();

                if (si < 0 || si >= (int) blobs.size()) continue;
                const Blob& bl = blobs[(size_t) si];
                const int ch = (int) bl.channels;
                const int nf = (int) bl.frames;
                if (ch < 1 || nf < 1) continue;
                const size_t wanted = (size_t) ch * (size_t) nf * (bl.codec == 1 ? 1u : 2u);
                if (bl.raw.size() != wanted) continue;

                inst->data.setSize (ch, nf);
                if (bl.codec == 1)
                {
                    for (int f = 0; f < nf; ++f)
                        for (int c = 0; c < ch; ++c)
                        {
                            const int16_t v = (int16_t) ((int16_t) (int8_t) bl.raw[(size_t) f * (size_t) ch + (size_t) c] << 8);
                            inst->data.getWritePointer (c)[f] = (float) v / 32768.0f;
                        }
                }
                else
                {
                    const auto* pcm = (const int16_t*) bl.raw.data();
                    for (int f = 0; f < nf; ++f)
                        for (int c = 0; c < ch; ++c)
                            inst->data.getWritePointer (c)[f]
                                = (float) pcm[(size_t) f * (size_t) ch + (size_t) c] / 32768.0f;
                }
            }

            if (s.bad || slot < 0 || slot >= TrackerEngine::kInstruments) continue;
            engine.setInstrument (slot, std::move (inst));
            ++loaded;
        }

        const uint32_t cellCount = s.u32v();
        for (uint32_t k = 0; k < cellCount && ! s.bad; ++k)
        {
            const int p = (int) s.u8v();
            const int r = (int) s.u8v();
            const int t = (int) s.u8v();
            const int note = s.i16v(), ins = s.i16v(), vol = s.i16v(), fx = s.i16v(), fp = s.i16v();
            if (p < 0 || p >= TrackerEngine::kMaxPatterns) continue;
            if (r < 0 || r >= TrackerEngine::kRows)        continue;
            if (t < 0 || t >= TrackerEngine::kTracks)      continue;
            auto& c = engine.patterns[p][r][t];
            c.note = note; c.instrument = ins; c.volume = vol; c.effect = fx; c.effectParam = fp;
        }

        return s.bad ? -1 : loaded;
    }

    inline int load (const std::vector<uint8_t>& bytes, TrackerEngine& engine)
    {
        return load (bytes.data(), bytes.size(), engine);
    }
}
