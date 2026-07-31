#pragma once
//
// rt_mod.h - importierten MOD/XM-Song in die TrackerEngine legen, JUCE-frei.
//
// Die Importer (ModImport/XmImport) lesen die Datei nur ein; das Einraeumen in
// die Engine stand bisher zweimal fast gleich im PluginProcessor. Hier liegt es
// EINMAL - so kann auch der Replayer (CLI/Web) MODs und XMs abspielen, ohne dass
// im Plugin etwas anders laeuft als vorher.
//
// Der Aufrufer nimmt vorher den Engine-Lock bzw. stoppt das Abspielen; hier
// passiert nur das Umraeumen selbst.

#include "TrackerEngine.h"

#include <algorithm>
#include <memory>

namespace rtmod
{
    // Alle Patterns leeren, damit nichts vom vorigen Song stehen bleibt.
    inline void clearPatterns (TrackerEngine& engine)
    {
        for (auto& pat : engine.patterns)
            for (auto& row : pat)
                for (auto& cl : row)
                    cl = TrackerEngine::Cell();
    }

    // Samples eines importierten Songs in die Instrument-Slots legen.
    // `Sample` braucht nur .data (AudioBuffer), .sourceRate und .name -
    // das erfuellen ModImport::Song::samples und XmImport::Song::samples beide.
    template <typename SampleArray>
    inline int applySamples (SampleArray& samples, int count, TrackerEngine& engine)
    {
        int loaded = 0;
        for (int i = 0; i < TrackerEngine::kInstruments; ++i)
        {
            if (i < count && samples[(size_t) i].data.getNumSamples() > 1)
            {
                auto inst = std::make_unique<TrackerEngine::Instrument>();
                inst->kind       = TrackerEngine::Instrument::Kind::Sample;
                inst->data       = std::move (samples[(size_t) i].data);
                inst->sourceRate = samples[(size_t) i].sourceRate;
                inst->name       = samples[(size_t) i].name.isNotEmpty()
                                     ? samples[(size_t) i].name
                                     : juce::String ("Sample ") + juce::String (i + 1);
                engine.setInstrument (i, std::move (inst));
                ++loaded;
            }
            else
            {
                engine.setInstrument (i, nullptr);   // leeren Slot freiraeumen
            }
        }
        return loaded;
    }

    // Wie applySamples, aber KOPIEREND - fuer const-Songs (S3M/IT gehen als
    // const durch applyImportedSong, da darf nichts herausgemoved werden).
    template <typename SampleArray>
    inline int applySamplesCopy (const SampleArray& samples, int count, TrackerEngine& engine)
    {
        int loaded = 0;
        for (int i = 0; i < TrackerEngine::kInstruments; ++i)
        {
            if (i < count && samples[(size_t) i].data.getNumSamples() > 1)
            {
                auto inst = std::make_unique<TrackerEngine::Instrument>();
                inst->kind       = TrackerEngine::Instrument::Kind::Sample;
                inst->data       = samples[(size_t) i].data;      // Kopie
                inst->sourceRate = samples[(size_t) i].sourceRate;
                inst->name       = samples[(size_t) i].name.isNotEmpty()
                                     ? samples[(size_t) i].name
                                     : juce::String ("Sample ") + juce::String (i + 1);
                engine.setInstrument (i, std::move (inst));
                ++loaded;
            }
            else
            {
                engine.setInstrument (i, nullptr);
            }
        }
        return loaded;
    }

    // Pattern-Zellen + Reihenfolge uebernehmen. Kanaele/Patterns werden auf das
    // gekappt, was die Engine kann (der Aufrufer sagt dem Nutzer Bescheid).
    //
    // variableRows=false -> MOD: jedes Pattern hat feste 64 Zeilen.
    // variableRows=true  -> XM/S3M/IT: Zeilenzahl steht am Pattern selbst. Ist es
    //   kuerzer als 64, bekommt die letzte Zeile auf einer freien Effekt-Zelle
    //   einen impliziten Pattern-Break (0xD) - sonst liefe die Wiedergabe durch
    //   leere Zeilen statt originalgetreu weiterzuspringen.
    template <typename Song>
    inline void applyPatternsAndOrder (const Song& song, TrackerEngine& engine, bool variableRows)
    {
        const int nch  = std::min (song.channels,    TrackerEngine::kTracks);
        const int npat = std::min (song.numPatterns, TrackerEngine::kMaxPatterns);

        for (int p = 0; p < npat; ++p)
        {
            const int prows = variableRows ? (int) song.patterns[(size_t) p].size() : 64;
            const int nrow  = std::min (prows, TrackerEngine::kRows);

            for (int r = 0; r < nrow; ++r)
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

            if (variableRows && prows > 0 && prows < TrackerEngine::kRows)
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
        {
            int v = song.order[(size_t) i];
            if (v < 0) v = 0;
            if (v > TrackerEngine::kMaxPatterns - 1) v = TrackerEngine::kMaxPatterns - 1;
            engine.order[nn++] = v;
        }
        engine.orderLen = nn > 0 ? nn : 1;
        engine.songMode = true;                 // ein MOD/XM ist ein ganzer Song
        engine.setEditPattern (engine.order[0]);
    }
}
