// rtx_to_mod - RetroTrax-Song (.retrotrax/.rtx) -> klassisches Amiga-ProTracker-MOD.
//
// Phase 2a des nativen Amiga-Players (s. tools/rtx_amiga/README.md): erst das
// PC-seitige Export-Tool, danach der echte 4-Kanal-68k-Pattern-Player.
//
//   rtx_to_mod <song.retrotrax|song.rtx> <song.mod>
//
// Ablauf: Song laden -> alle Synth-Instrumente automatisch einfrieren
// (rt_freeze.h, SID/Classic/FM -> Sample) -> als klassisches 4-Kanal-31-Sample-
// MOD exportieren (ModExport.h) -> SELBSTPRUEFUNG: exportierte Datei mit dem
// BESTEHENDEN Importer (ModImport.h) wieder einlesen, beide Fassungen rendern
// und vergleichen (Toleranzvergleich wie rtx_cli pack/freeze - 8-Bit-Sample und
// Perioden-Rundung sind keine Bit-exakte Operation, klanglich aber identisch).
//
// Bauen (JUCE-frei; -lz fuer zlib):
//   g++ -std=c++17 -O2 -DRETROTRAX_NO_JUCE -DHAVE_CXX17 -I src -I libs/residfp \
//       tools/rtx_amiga/export/rtx_to_mod.cpp build/libresidfp.a -lpthread -lz \
//       -o build/rtx_to_mod

#include "TrackerEngine.h"
#include "rt_freeze.h"
#include "rt_load.h"
#include "rt_rtx.h"
#include "rt_mod.h"
#include "ModImport.h"
#include "ModExport.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
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

static std::string baseName (const std::string& path)
{
    size_t slash = path.find_last_of ("/\\");
    std::string nm = (slash == std::string::npos) ? path : path.substr (slash + 1);
    size_t dot = nm.find_last_of ('.');
    if (dot != std::string::npos) nm = nm.substr (0, dot);
    return nm;
}

// Alle Synth-Instrumente eines geladenen Songs durch ihr gefrorenes
// Sample-Aequivalent ersetzen. Gibt die Zahl eingefrorener Instrumente zurueck.
static int freezeAllSynths (TrackerEngine& engine)
{
    using Inst = TrackerEngine::Instrument;
    int count = 0;
    for (int slot = 0; slot < TrackerEngine::kInstruments; ++slot)
    {
        const Inst* in = engine.instruments[slot].get();
        if (in == nullptr || in->kind != Inst::Kind::Synth) continue;

        auto result = rtfreeze::freezeInstrument (*in, 44100.0, 60, 8);
        if (! result.ok || result.instrument == nullptr)
        {
            std::printf ("WARNUNG: Instrument \"%s\" (Slot %d) liess sich nicht einfrieren - bleibt stumm.\n",
                         in->name.toRawUTF8(), slot);
            engine.setInstrument (slot, nullptr);
            continue;
        }
        engine.setInstrument (slot, std::move (result.instrument));
        ++count;
    }
    return count;
}

int main (int argc, char** argv)
{
    if (argc < 3)
    {
        std::printf ("Nutzung: %s <song.retrotrax|song.rtx> <song.mod>\n", argv[0]);
        return 1;
    }
    const std::string inPath  = argv[1];
    const std::string outPath = argv[2];
    const double sr = 44100.0;

    const std::string raw = readFile (inPath);
    if (raw.empty())
    {
        std::printf ("FEHLER: konnte '%s' nicht lesen (leer?).\n", inPath.c_str());
        return 1;
    }

    TrackerEngine engine;
    int nInst = -1;
    if (raw.rfind ("RTX1", 0) == 0)
        nInst = rtrtx::load ((const uint8_t*) raw.data(), raw.size(), engine);
    else
        nInst = rtload::loadRetrotrax (raw, engine);

    if (nInst < 0)
    {
        std::printf ("FEHLER: '%s' ist weder ein gueltiges .retrotrax noch ein gepacktes .rtx.\n",
                     inPath.c_str());
        return 1;
    }
    std::printf ("Geladen: %s (%d Instrument(e))\n", inPath.c_str(), nInst);

    const int frozen = freezeAllSynths (engine);
    std::printf ("Eingefroren: %d Synth-Instrument(e) -> Sample.\n", frozen);

    rtmodexport::Report report;
    const std::vector<uint8_t> modBytes = rtmodexport::exportMod (engine, baseName (inPath), report);
    for (const auto& w : report.warnings)
        std::printf ("WARNUNG: %s\n", w.c_str());
    if (! report.ok)
    {
        std::printf ("FEHLER: %s\n", report.error.c_str());
        return 1;
    }
    std::printf ("Export OK: %d Kanal/Kanaele, %d Instrument(e), %d Pattern -> %zu Bytes\n",
                 report.usedChannels, report.numInstruments, report.numPatterns, modBytes.size());

    {
        std::ofstream out (outPath, std::ios::binary);
        out.write ((const char*) modBytes.data(), (long) modBytes.size());
    }

    // --- Selbstpruefung: exportierte Datei mit dem BESTEHENDEN Importer wieder
    //     einlesen und gegen das Original rendern -------------------------------
    juce::File modFile (outPath);
    ModImport::Song song = ModImport::parse (modFile);
    if (! song.ok)
    {
        std::printf ("FEHLER: exportierte Datei laesst sich nicht wieder einlesen (%s).\n",
                     song.message.toRawUTF8());
        return 1;
    }

    TrackerEngine reimported;
    rtmod::applySamplesCopy (song.samples, 31, reimported);
    rtmod::applyPatternsAndOrder (song, reimported, /*variableRows*/ false);

    // Referenz muss FRISCH aus dem eingefrorenen Zustand gerendert werden -
    // 'engine' hat durch renderSong bereits Transport-Zustand veraendert.
    std::vector<float> a, b;
    const long fa = rtload::renderSong (engine, a, sr);
    const long fb = rtload::renderSong (reimported, b, sr);

    if (fa <= 0 || fb <= 0)
    {
        std::printf ("FEHLER: Rundreise-Rendern ergab kein Audio (Original=%ld, Reimport=%ld Frames).\n",
                     fa, fb);
        return 1;
    }

    // Toleranz noetig: 8-Bit-Quantisierung der Samples + Perioden-Rundung auf
    // ganze Amiga-Perioden sind keine verlustfreien Schritte (anders als beim
    // .rtx-Pack-Selbsttest, der dieselbe Engine nur umpackt). Original und
    // Reimport laufen zudem ueber VERSCHIEDENE Wiedergabewege (Original: teils
    // Live-Synthese; Reimport: immer ein gefrorenes, geloopptes 8-Bit-Sample) -
    // ein reiner Sample-fuer-Sample-Vergleich waere blind fuer Phasenversatz
    // (identischer Ton, andere Schwingungs-Startphase = trotzdem riesige
    // Differenz). Darum wie beim rt_freeze-Selbsttest ueber PEGEL (RMS)
    // vergleichen statt ueber Rohsamples - das ist die Groesse, die ein echter
    // Exportfehler (falsches Instrument, verlorene Lautstaerke, stummer Kanal)
    // tatsaechlich verschiebt.
    const long minFrames = std::min (a.size(), b.size()) / 2;
    double sumA = 0.0, sumB = 0.0;
    for (long i = 0; i < minFrames * 2; ++i)
    {
        sumA += std::abs (a[(size_t) i]);
        sumB += std::abs (b[(size_t) i]);
    }
    const double ratio = sumA > 1.0 ? sumB / sumA : 0.0;

    std::printf ("Rundreise: Original %ld Frames (Pegel-Summe %.1f), Reimport %ld Frames (Pegel-Summe %.1f), Verhaeltnis %.2f\n",
                 fa, sumA, fb, sumB, ratio);

    const bool lengthOk = std::abs (fa - fb) <= 64;         // eine Zeile Toleranz reicht
    const bool levelOk  = sumA > 1.0 && sumB > 1.0;         // beide muessen wirklich klingen
    const bool ratioOk  = ratio > 0.5 && ratio < 2.0;       // hoechstens ~6 dB daneben

    if (! lengthOk || ! levelOk || ! ratioOk)
    {
        std::printf ("FEHLGESCHLAGEN: Rundreise weicht zu stark ab (Laenge ok=%d, Pegel ok=%d, Verhaeltnis ok=%d).\n",
                     lengthOk, levelOk, ratioOk);
        return 2;
    }

    std::printf ("Rundreise OK - Export klingt wie das Original.\n");
    return 0;
}
