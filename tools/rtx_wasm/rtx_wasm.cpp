// rtx_wasm.cpp - RetroTrax-Replayer als C-API fuer WebAssembly (Phase 3).
//
// Baut die JUCE-freie Klang-Engine (TrackerEngine + reSIDfp) zu WASM und
// exportiert eine winzige C-API, damit .retrotrax-Songs direkt im Browser
// klingen. Zwei Wege:
//   1. Komplett-Render (rtx_render/rtx_tfmx_render): ganzer Song in einen
//      Puffer — robust, dient aelteren Browsern als Rueckfall.
//   2. Streaming (rtx_stream_*): Haeppchenweise rendern fuer den
//      AudioWorklet-Player — Start sofort, wenig Speicher, Spulen moeglich.
//
// Bauen: siehe tools/rtx_wasm/build.sh (emcc, -sUSE_ZLIB=1).
// Nativ testbar mit g++ (RTX_API = extern "C") -> tools/rtx_wasm/native_test.cpp.

#include "TrackerEngine.h"
#include "rt_load.h"
#include "rt_tfmx.h"

#include <algorithm>
#include <string>
#include <vector>

#ifdef __EMSCRIPTEN__
  #include <emscripten/emscripten.h>
  #define RTX_API extern "C" EMSCRIPTEN_KEEPALIVE
#else
  #define RTX_API extern "C"
#endif

namespace
{
    struct RtxPlayer
    {
        TrackerEngine        engine;
        std::vector<float>   interleaved; // L,R,L,R ... (Ergebnis von rtx_render/stream)
        double               sampleRate = 44100.0;
        int                  frames     = 0;
        int                  instruments = 0;

        // --- Streaming-Zustand (rtx_stream_*) ---
        enum class Stream { None, Retrotrax, Tfmx };
        Stream               stream          = Stream::None;
        TfmxPlayer           tfmx;           // eigener Player fuers TFMX-Streaming
        std::string          tfmxMdatPath;   // fuer Seek (Neustart + Vorspulen)
        long                 streamBaseLoops = -1;  // Loop-Basislinie (Ende-Erkennung)
        bool                 streamEnded     = false;
        long                 streamPos       = 0;   // bereits gerenderte Frames
    };

    // Ein 512er-Block, wie ihn die Engines nativ liefern.
    constexpr int kBlock = 512;
}

// Neuen Player anlegen. Rueckgabe = opaker Zeiger (Handle).
RTX_API void* rtx_create (double sampleRate)
{
    auto* p = new RtxPlayer();
    p->sampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
    return p;
}

RTX_API void rtx_destroy (void* h)
{
    delete static_cast<RtxPlayer*> (h);
}

// .retrotrax-XML (nullterminiert) laden. Rueckgabe: #Instrumente (>=0) oder -1.
RTX_API int rtx_load_retrotrax (void* h, const char* xml)
{
    auto* p = static_cast<RtxPlayer*> (h);
    if (p == nullptr || xml == nullptr) return -1;
    p->instruments = rtload::loadRetrotrax (std::string (xml), p->engine);
    return p->instruments;
}

// Den geladenen Song komplett rendern (bis die Reihenfolge einmal umlaeuft,
// max. maxSeconds). Fuellt den internen Puffer. Rueckgabe = Frames.
RTX_API int rtx_render (void* h, double maxSeconds)
{
    auto* p = static_cast<RtxPlayer*> (h);
    if (p == nullptr) return 0;
    p->frames = (int) rtload::renderSong (p->engine, p->interleaved,
                                          p->sampleRate,
                                          maxSeconds > 0.0 ? maxSeconds : 600.0);
    return p->frames;
}

// Zeiger auf den interleaved-Stereo-Float-Puffer (2*frames Werte, L,R).
RTX_API float* rtx_buffer (void* h)
{
    auto* p = static_cast<RtxPlayer*> (h);
    return (p != nullptr && ! p->interleaved.empty()) ? p->interleaved.data() : nullptr;
}

// Zahl gerenderter Frames (Sample-Paare). Werte im Puffer = 2*frames.
RTX_API int rtx_frames (void* h)
{
    auto* p = static_cast<RtxPlayer*> (h);
    return p != nullptr ? p->frames : 0;
}

// Abtastrate, mit der gerendert wurde (fuer den WebAudio-Buffer).
RTX_API double rtx_sample_rate (void* h)
{
    auto* p = static_cast<RtxPlayer*> (h);
    return p != nullptr ? p->sampleRate : 44100.0;
}

// ============================================================================
//  TFMX (Chris Huelsbeck) im Browser. Die .mdat/.smpl-Bytes schreibt der
//  JavaScript-Player vorher ins virtuelle Dateisystem (MEMFS); hier wird nur
//  der Pfad reingereicht, der Decoder findet die .smpl daneben (findSmpl).
//  TFMX laeuft endlos -> feste Spieldauer 'seconds' in einen Puffer rendern.
// ============================================================================

// mdatPath: Pfad im virtuellen Dateisystem (z.B. "/work/mdat.song").
// Rueckgabe: gerenderte Frames, oder <0 bei Fehler (kein/ungueltiges TFMX).
RTX_API int rtx_tfmx_render (void* h, const char* mdatPath, double seconds)
{
    auto* p = static_cast<RtxPlayer*> (h);
    if (p == nullptr || mdatPath == nullptr) return -1;
    TfmxPlayer::Info info;
    const long fr = rttfmx::renderTfmxToBuffer (std::string (mdatPath), p->interleaved,
                                                p->sampleRate,
                                                seconds > 0.0 ? seconds : 30.0, &info);
    p->frames = fr > 0 ? (int) fr : 0;
    return (int) fr; // <0 = Fehler, sonst Frames (Puffer via rtx_buffer/rtx_frames)
}

// ============================================================================
//  Streaming-API (Phase 4): haeppchenweise rendern fuer den AudioWorklet-
//  Player. Ablauf im Browser: rtx_load_retrotrax + rtx_stream_start (oder
//  rtx_stream_start_tfmx), dann wiederholt rtx_stream_render -> rtx_buffer.
//  Der Song startet sofort, statt erst komplett gerendert zu werden.
// ============================================================================

// .retrotrax-Stream starten (nach rtx_load_retrotrax). 0 = ok.
RTX_API int rtx_stream_start (void* h)
{
    auto* p = static_cast<RtxPlayer*> (h);
    if (p == nullptr) return -1;
    p->engine.prepare (p->sampleRate);
    p->engine.songMode.store (true);
    p->engine.play();
    p->stream          = RtxPlayer::Stream::Retrotrax;
    p->streamBaseLoops = -1;
    p->streamEnded     = false;
    p->streamPos       = 0;
    return 0;
}

// TFMX-Stream starten (mdat/smpl liegen bereits im MEMFS, wie beim
// Komplett-Render). 0 = ok, -1 = keine gueltige Datei, -2 = nicht abspielbar.
RTX_API int rtx_stream_start_tfmx (void* h, const char* mdatPath)
{
    auto* p = static_cast<RtxPlayer*> (h);
    if (p == nullptr || mdatPath == nullptr) return -1;
    const std::string smpl = rttfmx::findSmpl (mdatPath);
    if (! p->tfmx.load (juce::File (std::string (mdatPath)), juce::File (smpl)))
        return -1;
    if (! p->tfmx.isPlayable())
        return -2;
    p->tfmx.prepare (p->sampleRate);
    p->tfmx.restart();
    p->tfmxMdatPath = mdatPath;
    p->stream       = RtxPlayer::Stream::Tfmx;
    p->streamEnded  = false;
    p->streamPos    = 0;
    return 0;
}

// Naechstes Haeppchen rendern (mind. maxFrames, aufgerundet auf 512er-Bloecke).
// Ergebnis liegt in rtx_buffer(); Rueckgabe = Frames, 0 = Songende erreicht.
// (TFMX laeuft endlos — dort begrenzt der JavaScript-Player die Spieldauer.)
RTX_API int rtx_stream_render (void* h, int maxFrames)
{
    auto* p = static_cast<RtxPlayer*> (h);
    if (p == nullptr || p->stream == RtxPlayer::Stream::None || maxFrames <= 0)
        return 0;
    if (p->streamEnded)
    {
        p->frames = 0;
        return 0;
    }

    juce::AudioBuffer<float> buf (2, kBlock);
    p->interleaved.clear();
    long done = 0;
    while (done < maxFrames)
    {
        buf.clear();
        if (p->stream == RtxPlayer::Stream::Retrotrax)
            p->engine.process (buf);
        else
            p->tfmx.render (buf, 0, kBlock);

        const float* L = buf.getReadPointer (0);
        const float* R = buf.getReadPointer (1);
        for (int i = 0; i < kBlock; ++i)
        {
            p->interleaved.push_back (L[i]);
            p->interleaved.push_back (R[i]);
        }
        done += kBlock;

        // Ende-Erkennung wie in rtload::renderSong: Basislinie nach dem
        // ersten Block, Stopp sobald die Reihenfolge einmal umgelaufen ist.
        if (p->stream == RtxPlayer::Stream::Retrotrax)
        {
            const long loops = p->engine.songLoopCount.load();
            if (p->streamBaseLoops < 0)            p->streamBaseLoops = loops;
            else if (loops > p->streamBaseLoops) { p->streamEnded = true; break; }
        }
    }
    p->streamPos += done;
    p->frames = (int) done;
    return (int) done;
}

// Spulen: Stream neu starten und stumm bis 'seconds' vorrendern (die Engine
// muss den Zustand ja durchlaufen — Rendern ist deutlich schneller als Echtzeit).
RTX_API int rtx_stream_seek (void* h, double seconds)
{
    auto* p = static_cast<RtxPlayer*> (h);
    if (p == nullptr || p->stream == RtxPlayer::Stream::None) return -1;

    if (p->stream == RtxPlayer::Stream::Retrotrax)
    {
        p->engine.prepare (p->sampleRate);
        p->engine.songMode.store (true);
        p->engine.play();
        p->streamBaseLoops = -1;
    }
    else
    {
        p->tfmx.prepare (p->sampleRate);
        p->tfmx.restart();
    }
    p->streamEnded = false;
    p->streamPos   = 0;

    const long target = (long) (std::max (0.0, seconds) * p->sampleRate);
    juce::AudioBuffer<float> buf (2, kBlock);
    while (p->streamPos < target && ! p->streamEnded)
    {
        buf.clear();
        if (p->stream == RtxPlayer::Stream::Retrotrax)
        {
            p->engine.process (buf);
            const long loops = p->engine.songLoopCount.load();
            if (p->streamBaseLoops < 0)          p->streamBaseLoops = loops;
            else if (loops > p->streamBaseLoops) p->streamEnded = true;
        }
        else
            p->tfmx.render (buf, 0, kBlock);
        p->streamPos += kBlock;
    }
    return 0;
}

// 1 = der Stream hat das Songende erreicht (nur .retrotrax).
RTX_API int rtx_stream_ended (void* h)
{
    auto* p = static_cast<RtxPlayer*> (h);
    return (p != nullptr && p->streamEnded) ? 1 : 0;
}

// Geschaetzte Songlaenge in Sekunden: Reihenfolge x 64 Zeilen x Zeilendauer
// (Zeile = Sechzehntel). Tempo-Effekte im Song verschieben die echte Laenge —
// der Player korrigiert die Anzeige, sobald das Ende wirklich erreicht ist.
RTX_API double rtx_estimate_seconds (void* h)
{
    auto* p = static_cast<RtxPlayer*> (h);
    if (p == nullptr) return 0.0;
    const double bpm = std::max (20.0, (double) p->engine.bpm.load());
    return p->engine.orderLen * (double) TrackerEngine::kRows * 60.0 / (bpm * 4.0);
}
