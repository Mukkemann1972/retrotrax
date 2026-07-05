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
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

extern "C" {
    void*  rtx_create (double sampleRate);
    void   rtx_destroy (void* h);
    int    rtx_load_retrotrax (void* h, const char* xml);
    int    rtx_render (void* h, double maxSeconds);
    float* rtx_buffer (void* h);
    int    rtx_frames (void* h);
    double rtx_sample_rate (void* h);
    int    rtx_stream_start (void* h);
    int    rtx_stream_render (void* h, int maxFrames);
    int    rtx_stream_seek (void* h, double seconds);
    int    rtx_stream_ended (void* h);
    double rtx_estimate_seconds (void* h);
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

    // Komplett-Render fuer den Vergleich sichern (rtx_stream_* nutzt denselben Puffer).
    std::vector<float> full (buf, buf + (size_t) n * 2);
    rtx_destroy (h);

    // ── Streaming-API pruefen: Haeppchen muessen exakt dasselbe Audio ergeben ──
    void* hs = rtx_create (44100.0);
    rtx_load_retrotrax (hs, xml.c_str());
    if (rtx_stream_start (hs) != 0) { std::printf ("FEHLER: rtx_stream_start\n"); return 4; }
    std::printf ("Schaetzung rtx_estimate_seconds = %.2f s (echt: %.2f s)\n",
                 rtx_estimate_seconds (hs), n / sr);

    std::vector<float> streamed;
    streamed.reserve (full.size());
    const int chunk = 8192; // wie im Web-Player
    for (;;)
    {
        const int got = rtx_stream_render (hs, chunk);
        if (got <= 0) break;
        const float* b = rtx_buffer (hs);
        streamed.insert (streamed.end(), b, b + (size_t) got * 2);
        if ((long) streamed.size() > (long) full.size() * 2 + chunk * 4)
        { std::printf ("FEHLER: Stream endet nicht.\n"); return 5; }
    }
    if (rtx_stream_ended (hs) != 1)
    { std::printf ("FEHLER: rtx_stream_ended != 1 nach Songende.\n"); return 6; }

    const size_t common = std::min (full.size(), streamed.size());
    const bool sameAudio = common > 0
        && std::memcmp (full.data(), streamed.data(), common * sizeof (float)) == 0;
    // Stream rundet auf 512er-Bloecke — Laengen duerfen nur minimal abweichen.
    const long lenDiff = std::labs ((long) full.size() - (long) streamed.size());
    std::printf ("Streaming: frames=%zu vs. voll=%zu  identisch=%s  Laengen-Delta=%ld Werte\n",
                 streamed.size() / 2, full.size() / 2, sameAudio ? "JA" : "NEIN", lenDiff);

    // ── Spulen pruefen: seek(2s) muss ab Frame 2s*sr dasselbe liefern ──────
    bool seekOk = true;
    {
        const double seekSec = std::min (2.0, 0.5 * n / sr);
        if (rtx_stream_seek (hs, seekSec) != 0) { std::printf ("FEHLER: rtx_stream_seek\n"); return 7; }
        const int got = rtx_stream_render (hs, chunk);
        const float* b = rtx_buffer (hs);
        // Zielposition wird auf 512er-Bloecke gerundet (Engine rendert blockweise).
        size_t off = ((size_t) (seekSec * sr + 511) / 512) * 512 * 2;
        off = std::min (off, full.size());
        const size_t cmp = std::min ((size_t) got * 2, full.size() - off);
        seekOk = got > 0 && cmp > 0
              && std::memcmp (full.data() + off, b, cmp * sizeof (float)) == 0;
        std::printf ("Seek(%.2fs): frames=%d  identisch-ab-Ziel=%s\n",
                     seekSec, got, seekOk ? "JA" : "NEIN");
    }
    rtx_destroy (hs);

    const bool ok = n > 0 && peak > 0.001 && sameAudio && seekOk;
    std::printf (ok ? "ALLE TESTS OK\n" : "TEST FEHLGESCHLAGEN\n");
    return ok ? 0 : 3;
}
