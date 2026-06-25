// rtx_cli - Phase-1-Beweis fuer den schlanken RetroTrax-Replayer.
//
// Ziel dieser Stufe: zeigen, dass die Klang-Engine (TrackerEngine + reSIDfp)
// OHNE JUCE baut und laeuft. Es wird noch KEIN .retrotrax geladen - nur die
// Engine hochgefahren und ein paar Bloecke gerendert (Smoke-Test). Das Laden
// echter Songs kommt in Phase 2.
//
// Bauen (JUCE-frei):
//   g++ -std=c++17 -O2 -DRETROTRAX_NO_JUCE -I src -I libs/residfp \
//       tools/rtx_cli/main.cpp build/libresidfp.a -lpthread -o build/rtx_cli

#include "TrackerEngine.h"

#include <cmath>
#include <cstdio>

int main()
{
    TrackerEngine engine;
    engine.prepare (44100.0);
    engine.play();

    juce::AudioBuffer<float> buf (2, 512);

    double peak = 0.0;
    long   nonzero = 0, total = 0;
    for (int block = 0; block < 100; ++block)
    {
        buf.clear();
        engine.process (buf);
        for (int ch = 0; ch < buf.getNumChannels(); ++ch)
        {
            const float* p = buf.getReadPointer (ch);
            for (int i = 0; i < buf.getNumSamples(); ++i)
            {
                const double v = std::fabs ((double) p[i]);
                peak = v > peak ? v : peak;
                if (v > 0.0) ++nonzero;
                ++total;
            }
        }
    }

    std::printf ("rtx_cli OK - Engine baute & lief JUCE-frei.\n");
    std::printf ("  Bloecke=100  Samples=%ld  Peak=%.4f  (leere Song-Daten -> Stille erwartet)\n",
                 total, peak);
    return 0;
}
