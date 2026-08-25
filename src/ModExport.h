#pragma once
//
// ModExport.h - TrackerEngine -> klassisches Amiga-ProTracker-MOD (.mod), JUCE-frei.
//
// Gegenstueck zu ModImport.h (liest MOD -> neutrale Struktur). Bewusst als
// klassisches 4-Kanal-31-Sample-MOD exportiert statt eines eigenen Formats:
// die Amiga-Szene kennt .MOD bereits, und der bestehende Importer (ModImport.h)
// kann den Export sofort wieder einlesen - ein Rundreise-Test ist damit moeglich,
// ganz ohne 68k-Code (s. tools/rtx_amiga/export/).
//
// Voraussetzung: hoechstens 4 der 16 Spuren duerfen im Song ueberhaupt belegt
// sein (Paula hat nur 4 DMA-Kanaele) und alle Instrumente muessen bereits
// Sample-Instrumente sein (Synth-Spuren vorher mit rt_freeze.h einfrieren -
// macht das Export-Tool automatisch).
//
// Effekt-Abdeckung: nur Codes mit 1:1-MOD-Aequivalent (0 Arpeggio, 1/2/3
// Slides/Porta, 4 Vibrato, 9 Sample-Offset, A Vol-Slide, B Position-Jump,
// C Set-Volume, D Pattern-Break, F Speed/Tempo) werden uebernommen. Alles
// andere landet im Report als Warnung statt stillschweigend zu verschwinden.
// Klassisches MOD hat keine eigene Lautstaerke-Spalte (nur den 0xC-Effekt) -
// eine Zellen-Lautstaerke ohne eigenen Effekt wird deshalb zu 0xC; kollidiert
// sie mit einem anderen Effekt in derselben Zelle, wird sie verworfen (nur
// eine Effekt-Spalte pro Zelle im Format) und das steht im Report.

#include "TrackerEngine.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace rtmodexport
{
    struct Report
    {
        bool ok = false;
        std::string error;                       // gesetzt, wenn ok == false
        std::vector<std::string> warnings;       // z.B. verworfene Effekte/Lautstaerken
        int usedChannels = 0;
        int numInstruments = 0;
        int numPatterns = 0;
    };

    namespace detail
    {
        // Kehrfunktion zu ModImport::noteFromPeriod(): Note 60 (C-5, unsere
        // Referenztonhoehe) entspricht der klassischen PT-Periode 428.
        inline int periodForNote (int note)
        {
            const double period = 428.0 * std::pow (2.0, (60.0 - (double) note) / 12.0);
            const int p = (int) std::lround (period);
            return std::clamp (p, 1, 4095);      // 12-Bit-Feld im MOD-Format
        }

        inline void putBE16 (std::vector<uint8_t>& b, size_t offset, int value)
        {
            b[offset]     = (uint8_t) ((value >> 8) & 0xFF);
            b[offset + 1] = (uint8_t) (value & 0xFF);
        }

        // Welche der 16 Spuren haben irgendwo im Song ueberhaupt eine Zelle?
        inline std::vector<int> usedTracks (const TrackerEngine& engine)
        {
            std::vector<int> used;
            for (int t = 0; t < TrackerEngine::kTracks; ++t)
            {
                bool any = false;
                for (int p = 0; p < TrackerEngine::kMaxPatterns && ! any; ++p)
                    for (int r = 0; r < TrackerEngine::kRows && ! any; ++r)
                    {
                        const auto& c = engine.patterns[p][r][t];
                        if (c.note != -1 || c.instrument != -1 || c.volume != -1 || c.effect != -1)
                            any = true;
                    }
                if (any) used.push_back (t);
            }
            return used;
        }
    }

    // engine: MUSS bereits eingefroren sein (keine Synth-Instrumente mehr) -
    // der Aufrufer erledigt das vorher mit rt_freeze.h (siehe rtx_to_mod-Tool).
    inline std::vector<uint8_t> exportMod (const TrackerEngine& engine, const std::string& title,
                                           Report& report)
    {
        using Inst = TrackerEngine::Instrument;
        report = Report();

        const auto tracks = detail::usedTracks (engine);
        if ((int) tracks.size() > 4)
        {
            report.error = "Song nutzt " + std::to_string (tracks.size())
                          + " Spuren gleichzeitig - Paula kann nur 4. Song entschaerfen "
                            "(hoechstens 4 gleichzeitig belegte Spuren).";
            return {};
        }
        report.usedChannels = std::max (1, (int) tracks.size());

        // Slot -> 1-basierter MOD-Sample-Index (0 = leer/kein Instrument).
        int slotToModIndex[TrackerEngine::kInstruments];
        for (auto& v : slotToModIndex) v = 0;
        std::vector<const Inst*> samplesInOrder;
        for (int slot = 0; slot < TrackerEngine::kInstruments; ++slot)
        {
            const Inst* in = engine.instruments[slot].get();
            if (in == nullptr) continue;
            if (in->kind != Inst::Kind::Sample)
            {
                report.error = "Instrument \"" + in->name.toStdString() + "\" (Slot "
                              + std::to_string (slot) + ") ist noch ein Synth - erst einfrieren.";
                return {};
            }
            if (in->data.getNumSamples() < 2) continue;    // leer -> ueberspringen
            if ((int) samplesInOrder.size() >= 31)
            {
                report.warnings.push_back ("Mehr als 31 Instrumente mit Daten - Rest abgeschnitten.");
                break;
            }
            slotToModIndex[slot] = (int) samplesInOrder.size() + 1;
            samplesInOrder.push_back (in);
        }
        report.numInstruments = (int) samplesInOrder.size();

        // --- Header: 20 Name + 31x30 Sample-Koepfe + 1 Songlaenge + 1 Restart
        //     + 128 Order + 4 "M.K."-Kennung = 1084 Bytes -------------------------
        std::vector<uint8_t> out (1084, 0);
        {
            const std::string nm = title.substr (0, 20);
            std::memcpy (out.data(), nm.data(), nm.size());
        }

        std::vector<std::vector<int8_t>> pcm (31);
        for (int i = 0; i < 31; ++i)
        {
            const size_t hdrOfs = 20 + (size_t) i * 30;
            if (i >= (int) samplesInOrder.size()) continue;
            const Inst& in = *samplesInOrder[(size_t) i];

            std::string nm = in.name.toStdString();
            if (nm.size() > 22) nm = nm.substr (0, 22);
            std::memcpy (out.data() + hdrOfs, nm.data(), nm.size());

            // WICHTIG: inst->gain ist im Plugin ein reiner PLAYBACK-Multiplikator
            // (renderSample() in TrackerEngine.h wendet ihn erst beim Abspielen an,
            // die rohen Sample-Daten selbst sind unveraendert). MOD kennt so ein
            // zusaetzliches Gain-Feld nicht - deshalb hier fest in die PCM-Daten
            // einrechnen, sonst klingen v.a. frisch eingefrorene Synth-Instrumente
            // (calibrateGain() in rt_freeze.h setzt oft einen deutlichen Faktor)
            // im Export viel zu leise.
            const float gainAmp = in.gain;
            const int ch = in.data.getNumChannels();
            const int nf = in.data.getNumSamples();
            float peak = 0.0f;
            for (int f = 0; f < nf; ++f)
            {
                float v = 0.0f;
                for (int c = 0; c < ch; ++c) v += in.data.getReadPointer (c)[f];
                peak = std::max (peak, std::abs (v / (float) ch * gainAmp));
            }
            const float scale = peak > 1.0f ? 1.0f / peak : 1.0f;   // nie clippen, sonst unveraendert

            std::vector<int8_t> mono ((size_t) nf);
            for (int f = 0; f < nf; ++f)
            {
                float v = 0.0f;
                for (int c = 0; c < ch; ++c) v += in.data.getReadPointer (c)[f];
                v = (v / (float) ch) * gainAmp * scale;
                const int s = (int) std::lround (v * 127.0f);
                mono[(size_t) f] = (int8_t) std::clamp (s, -128, 127);
            }
            if (! mono.empty() && mono.size() % 2 != 0) mono.pop_back();  // MOD speichert in Worten
            pcm[(size_t) i] = std::move (mono);

            detail::putBE16 (out, hdrOfs + 22, (int) (pcm[(size_t) i].size() / 2));
            out[hdrOfs + 24] = 0;                       // Finetune: kein Aequivalent -> 0
            out[hdrOfs + 25] = 64;                       // Lautstaerke voll (Pegel steckt im PCM)

            // Loop: WICHTIG fuer gefrorene Synth-Instrumente (rt_freeze.h liefert
            // immer Loop::Forward mit loopStart=0 - der ganze Sample IST die Loop,
            // gedacht zum endlosen Wiederholen waehrend die Note gehalten wird).
            // Ohne das klingt jede gehaltene Note nur wie ein kurzer Klick statt
            // eines gehaltenen Tons. PingPong gibt es im klassischen MOD nicht -
            // Annaeherung als Forward-Loop (gemeldet).
            const int lenWords = (int) (pcm[(size_t) i].size() / 2);
            int loopStartWords = 0, loopLenWords = lenWords > 0 ? 1 : 0;  // 1 = kein Loop
            if (in.loopMode != Inst::Loop::Off && lenWords > 0)
            {
                if (in.loopMode == Inst::Loop::PingPong)
                    report.warnings.push_back ("Instrument \"" + in.name.toStdString()
                        + "\": Ping-Pong-Loop gibt es im klassischen MOD nicht, als "
                          "Vorwaerts-Loop angenaehert.");
                const int lenSamples = (int) pcm[(size_t) i].size();
                int startSamp = (int) (in.loopStart * (float) (lenSamples - 1));
                startSamp = std::clamp (startSamp, 0, lenSamples - 2);
                loopStartWords = startSamp / 2;
                loopLenWords   = std::max (1, (lenSamples / 2) - loopStartWords);
            }
            detail::putBE16 (out, hdrOfs + 26, loopStartWords);
            detail::putBE16 (out, hdrOfs + 28, loopLenWords);
        }

        // --- Order + Pattern-Anzahl ---------------------------------------------
        const int orderLen = std::clamp (engine.orderLen, 1, TrackerEngine::kMaxOrder);
        out[950] = (uint8_t) orderLen;
        out[951] = 0x7F;
        int maxPatUsed = 0;
        for (int i = 0; i < orderLen; ++i)
        {
            const int p = std::clamp (engine.order[i], 0, TrackerEngine::kMaxPatterns - 1);
            out[952 + i] = (uint8_t) p;
            maxPatUsed = std::max (maxPatUsed, p);
        }
        const int numPatterns = maxPatUsed + 1;
        report.numPatterns = numPatterns;
        std::memcpy (out.data() + 1080, "M.K.", 4);

        // --- Pattern-Daten -------------------------------------------------------
        // IMMER 4 Kanaele schreiben (auch wenn weniger Spuren belegt sind) - die
        // "M.K."-Kennung sagt jedem Leser (auch unserem eigenen ModImport.h) fest
        // 4 Kanaele zu; ungenutzte Kanaele bleiben einfach leer. Eine variable
        // Kanalzahl wuerde die Kennung mitaendern muessen (z.B. "1CHN"), was nur
        // unser eigener Importer versteht, sonst niemand im Amiga-Oekosystem -
        // "immer 4, manche leer" ist robuster UND MOD-kompatibler.
        constexpr int nch = 4;
        std::vector<uint8_t> patData ((size_t) numPatterns * 64 * (size_t) nch * 4, 0);
        for (int p = 0; p < numPatterns; ++p)
            for (int r = 0; r < TrackerEngine::kRows; ++r)
                for (int ci = 0; ci < nch; ++ci)
                {
                    if (ci >= (int) tracks.size()) continue;   // Kanal bleibt leer (Nullen)
                    const int track = tracks[(size_t) ci];
                    const auto& c = engine.patterns[p][r][track];
                    const size_t idx = ((size_t) p * 64 + (size_t) r) * (size_t) nch * 4
                                      + (size_t) ci * 4;
                    uint8_t* b = patData.data() + idx;

                    int period = 0;
                    if (c.note >= 0) period = detail::periodForNote (c.note);
                    const int sampleNum = (c.instrument >= 0 && c.instrument < TrackerEngine::kInstruments)
                                        ? slotToModIndex[c.instrument] : 0;

                    int eff = 0, par = 0;
                    bool haveEffect = false;
                    if (c.effect == 0x0 || c.effect == 0x1 || c.effect == 0x2 || c.effect == 0x3
                     || c.effect == 0x4 || c.effect == 0x9 || c.effect == 0xA || c.effect == 0xB
                     || c.effect == 0xC || c.effect == 0xF)
                    {
                        eff = c.effect; par = std::clamp (c.effectParam, 0, 255); haveEffect = true;
                    }
                    else if (c.effect == 0xD)
                    {
                        const int row = std::clamp (c.effectParam, 0, 63);
                        eff = 0xD; par = ((row / 10) << 4) | (row % 10); haveEffect = true;
                    }
                    else if (c.effect >= 0)
                    {
                        report.warnings.push_back ("P" + std::to_string (p) + "/Z" + std::to_string (r)
                            + "/Sp" + std::to_string (track) + ": Effekt " + std::to_string (c.effect)
                            + " hat kein MOD-Aequivalent, verworfen.");
                    }
                    if (! haveEffect && c.volume >= 0)
                    {
                        eff = 0xC; par = std::clamp (c.volume, 0, 64);
                        haveEffect = true;
                    }
                    else if (haveEffect && c.volume >= 0 && eff != 0xC)
                    {
                        report.warnings.push_back ("P" + std::to_string (p) + "/Z" + std::to_string (r)
                            + "/Sp" + std::to_string (track) + ": Lautstaerke UND Effekt gleichzeitig - "
                              "klassisches MOD hat nur eine Effekt-Spalte, Lautstaerke verworfen.");
                    }

                    b[0] = (uint8_t) ((sampleNum & 0xF0) | ((period >> 8) & 0x0F));
                    b[1] = (uint8_t) (period & 0xFF);
                    b[2] = (uint8_t) (((sampleNum & 0x0F) << 4) | (eff & 0x0F));
                    b[3] = (uint8_t) par;
                }

        out.insert (out.end(), patData.begin(), patData.end());
        for (int i = 0; i < 31; ++i)
            if (! pcm[(size_t) i].empty())
                out.insert (out.end(), (const uint8_t*) pcm[(size_t) i].data(),
                            (const uint8_t*) pcm[(size_t) i].data() + pcm[(size_t) i].size());

        report.ok = true;
        return out;
    }
}
