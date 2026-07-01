// rtx_wasm.cpp - RetroTrax-Replayer als C-API fuer WebAssembly (Phase 3).
//
// Baut die JUCE-freie Klang-Engine (TrackerEngine + reSIDfp) zu WASM und
// exportiert eine winzige C-API, damit .retrotrax-Songs direkt im Browser
// klingen. MVP: einen Song komplett in einen Float-Puffer rendern; der
// JavaScript-Player im Browser packt den Puffer in einen WebAudio-Buffer und
// spielt ihn. (Streaming/AudioWorklet und TFMX-im-Browser via MEMFS = spaeter.)
//
// Bauen: siehe tools/rtx_wasm/build.sh (emcc, -sUSE_ZLIB=1).
// Nativ testbar mit g++ (RTX_API = extern "C") -> tools/rtx_wasm/native_test.cpp.

#include "TrackerEngine.h"
#include "rt_load.h"

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
        std::vector<float>   interleaved; // L,R,L,R ... (Ergebnis von rtx_render)
        double               sampleRate = 44100.0;
        int                  frames     = 0;
        int                  instruments = 0;
    };
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
