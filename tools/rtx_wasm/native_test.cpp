// native_test.cpp - prueft die WASM-C-API (rtx_wasm.cpp) NATIV mit g++.
//
// Beweist die Render-Logik ohne Emscripten: derselbe Code kompiliert spaeter
// unter emcc zu WASM. Laedt eine .retrotrax-Datei, rendert, prueft Signal.
//
// Bauen:
//   g++ -std=c++17 -O2 -DRETROTRAX_NO_JUCE -DHAVE_CXX17 -I src -I libs/residfp \
//       tools/rtx_wasm/rtx_wasm.cpp tools/rtx_wasm/native_test.cpp \
//       build/libresidfp.a -lpthread -lz -o build/rtx_wasm_test

#include <cstdio>
#include <cmath>
#include <fstream>
#include <sstream>
#include <string>

extern "C" {
    void*  rtx_create (double sampleRate);
    void   rtx_destroy (void* h);
    int    rtx_load_retrotrax (void* h, const char* xml);
    int    rtx_render (void* h, double maxSeconds);
    float* rtx_buffer (void* h);
    int    rtx_frames (void* h);
    double rtx_sample_rate (void* h);
}

int main (int argc, char** argv)
{
    if (argc < 2) { std::printf ("Nutzung: %s <song.retrotrax>\n", argv[0]); return 2; }

    std::ifstream in (argv[1], std::ios::binary);
    std::ostringstream ss; ss << in.rdbuf();
    const std::string xml = ss.str();
    if (xml.empty()) { std::printf ("FEHLER: '%s' leer/nicht lesbar.\n", argv[1]); return 1; }

    void* h = rtx_create (44100.0);
    const int inst = rtx_load_retrotrax (h, xml.c_str());
    std::printf ("rtx_load_retrotrax -> Instrumente=%d\n", inst);

    const int frames = rtx_render (h, 600.0);
    const float* buf = rtx_buffer (h);
    const int n = rtx_frames (h);
    const double sr = rtx_sample_rate (h);

    double peak = 0.0, sumSq = 0.0;
    long nz = 0;
    for (int i = 0; i < n * 2; ++i)
    {
        const double v = buf ? buf[i] : 0.0;
        peak = std::max (peak, std::fabs (v));
        sumSq += v * v;
        if (v != 0.0) ++nz;
    }
    const double rms = n > 0 ? std::sqrt (sumSq / (n * 2)) : 0.0;
    std::printf ("gerendert: frames=%d (%.2f s @ %.0f Hz)  peak=%.3f  rms=%.4f  nonzero=%.1f%%\n",
                 frames, n / sr, sr, peak, rms, n > 0 ? 100.0 * nz / (n * 2) : 0.0);

    rtx_destroy (h);
    return (n > 0 && peak > 0.001) ? 0 : 3; // Erfolg nur bei echtem Signal
}
