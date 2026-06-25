// rtx_cli - JUCE-freier RetroTrax-Replayer (Phase 2a).
//
//   Ohne Argument : Smoke-Test (Engine hochfahren, Stille rendern).
//   <song.retrotrax> [out.wav] : Song laden und zu WAV rendern.
//
// Phase 2a kann: bpm/swing/order, Synth-Instrumente, Pattern-Zellen.
// Sample-Instrumente (eingebettete <D>-Daten) folgen in Phase 2b.
//
// Bauen (JUCE-frei):
//   g++ -std=c++17 -O2 -DRETROTRAX_NO_JUCE -DHAVE_CXX17 -I src -I libs/residfp \
//       tools/rtx_cli/main.cpp build/libresidfp.a -lpthread -o build/rtx_cli

#include "TrackerEngine.h"
#include "rt_load.h"

#include <cmath>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>

static std::string readFile (const std::string& path)
{
    std::ifstream in (path, std::ios::binary);
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

int main (int argc, char** argv)
{
    const double sr = 44100.0;

    if (argc < 2)
    {
        // Smoke-Test: nur zeigen, dass die Engine JUCE-frei laeuft.
        TrackerEngine engine;
        engine.prepare (sr);
        engine.play();
        juce::AudioBuffer<float> buf (2, 512);
        for (int b = 0; b < 100; ++b) { buf.clear(); engine.process (buf); }
        std::printf ("rtx_cli OK - Engine lief JUCE-frei (Smoke-Test, kein Song geladen).\n");
        std::printf ("Nutzung: %s <song.retrotrax> [out.wav]\n", argv[0]);
        return 0;
    }

    const std::string inPath  = argv[1];
    const std::string outPath = (argc >= 3) ? argv[2] : "out.wav";

    const std::string xml = readFile (inPath);
    if (xml.empty())
    {
        std::printf ("FEHLER: konnte '%s' nicht lesen (leer?).\n", inPath.c_str());
        return 1;
    }

    TrackerEngine engine;
    const int n = rtload::loadRetrotrax (xml, engine);
    if (n < 0)
    {
        std::printf ("FEHLER: '%s' ist kein RETROTRAX-Dokument.\n", inPath.c_str());
        return 1;
    }

    std::printf ("Geladen: %s\n", inPath.c_str());
    std::printf ("  Instrumente=%d  bpm=%.1f  orderLen=%d\n",
                 n, engine.bpm.load(), engine.orderLen);

    const long frames = rtload::renderSongToWav (engine, outPath, sr);
    std::printf ("Gerendert: %ld Frames (%.2f s) -> %s\n",
                 frames, frames / sr, outPath.c_str());
    return 0;
}
