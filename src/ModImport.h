#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <vector>
#include <cmath>

// Liest ein klassisches Amiga-ProTracker-MOD (.mod, 31 Samples, 4/6/8/.. Kanaele).
// Reines Parsen in eine neutrale Struktur - das Einsetzen in die Engine macht der
// Processor. Periode -> RetroTrax-Note (Periode 428 ~ PT C-2 = Note 60 / C-5);
// Effekte mit gleicher Nummerierung (0 Arpeggio, 1/2 Slide, 3 Porta, 4 Vibrato,
// A Vol-Slide, C Lautstaerke, F Tempo) werden direkt uebernommen, der Rest fallen
// gelassen. Sample-Daten sind 8-Bit-signed-PCM, mono.
namespace ModImport
{
    struct Sample
    {
        juce::String name;
        juce::AudioBuffer<float> data; // mono, -1..1
        double sourceRate = 8287.0;
        int    volume = 64;            // 0..64 (Standard-Lautstaerke des Samples)
    };

    struct Cell { int note = -1, instrument = -1, volume = -1, effect = -1, effectParam = 0; };

    struct Song
    {
        bool ok = false;
        juce::String title;
        int channels = 4;
        int numPatterns = 0;
        int songLength = 1;
        std::array<int, 128> order {};
        Sample samples[31];
        std::vector<std::vector<std::vector<Cell>>> patterns; // [pattern][row][channel]
        juce::String message;
    };

    // PAL-Amiga: Sample-Abspielrate bei Periode P = 7093789.2/(2*P). Periode 428
    // (~PT C-2) legen wir auf RetroTrax-Note 60 (C-5) -> sourceRate ~8287 Hz.
    inline double sourceRateForMod() { return 7093789.2 / (2.0 * 428.0); }

    inline int noteFromPeriod (int period)
    {
        if (period <= 0) return -1;
        // Eine Oktave hoeher = halbe Periode = +12 Halbtoene.
        const double nn = 60.0 + 12.0 * std::log2 (428.0 / (double) period);
        return juce::jlimit (0, 119, (int) std::lround (nn));
    }

    inline int be16 (const juce::uint8* p) { return (p[0] << 8) | p[1]; }

    inline Song parse (const juce::File& file)
    {
        Song s;
        juce::MemoryBlock mb;
        if (! file.loadFileAsData (mb)) { s.message = "Datei nicht lesbar"; return s; }
        const auto* d = (const juce::uint8*) mb.getData();
        const size_t n = mb.getSize();
        if (n < 1084) { s.message = "Datei zu klein fuer ein MOD"; return s; }

        // Signatur bei Offset 1080 bestimmt die Kanalzahl (nur 31-Sample-MODs).
        const juce::String tag = juce::String::fromUTF8 ((const char*) (d + 1080), 4);
        int ch = 0;
        if (tag == "M.K." || tag == "M!K!" || tag == "FLT4" || tag == "4CHN") ch = 4;
        else if (tag == "6CHN") ch = 6;
        else if (tag == "8CHN" || tag == "FLT8" || tag == "OCTA" || tag == "CD81") ch = 8;
        else if (tag.substring (1, 4).equalsIgnoreCase ("CHN")
                 && tag.substring (0, 1).containsOnly ("0123456789"))
            ch = tag.substring (0, 1).getIntValue();
        else if (tag.substring (2, 4).equalsIgnoreCase ("CH")
                 && tag.substring (0, 2).containsOnly ("0123456789"))
            ch = tag.substring (0, 2).getIntValue();
        if (ch <= 0)
        {
            s.message = "Kein erkanntes 31-Sample-MOD (Signatur \"" + tag + "\")";
            return s;
        }
        s.channels = ch;
        s.title = juce::String::fromUTF8 ((const char*) d, 20).trim();

        // 31 Sample-Koepfe ab Offset 20, je 30 Bytes.
        struct Hdr { int lenBytes = 0, volume = 64; juce::String name; };
        Hdr hdr[31];
        size_t o = 20;
        for (int i = 0; i < 31; ++i)
        {
            const juce::uint8* h = d + o;
            hdr[i].name    = juce::String::fromUTF8 ((const char*) h, 22).trim();
            hdr[i].lenBytes = be16 (h + 22) * 2;          // Laenge steht in Worten
            hdr[i].volume   = juce::jlimit (0, 64, (int) h[25]);
            o += 30;
        }

        s.songLength = juce::jlimit (1, 128, (int) d[950]);
        int maxPat = 0;
        for (int i = 0; i < 128; ++i)
        {
            s.order[i] = d[952 + i];
            if (s.order[i] > maxPat) maxPat = s.order[i];
        }
        s.numPatterns = maxPat + 1;

        // Pattern-Daten ab Offset 1084: 64 Zeilen * Kanaele * 4 Bytes pro Pattern.
        const size_t pofs = 1084;
        const size_t patBytes = 64 * (size_t) ch * 4;
        s.patterns.resize ((size_t) s.numPatterns);
        for (int p = 0; p < s.numPatterns; ++p)
        {
            s.patterns[(size_t) p].assign (64, std::vector<Cell> ((size_t) ch));
            for (int r = 0; r < 64; ++r)
                for (int c = 0; c < ch; ++c)
                {
                    const size_t idx = pofs + (size_t) p * patBytes + ((size_t) r * ch + c) * 4;
                    if (idx + 4 > n) continue;
                    const juce::uint8* b = d + idx;
                    const int sample = (b[0] & 0xF0) | (b[2] >> 4);
                    const int period = ((b[0] & 0x0F) << 8) | b[1];
                    const int eff    = b[2] & 0x0F;
                    const int par    = b[3];

                    Cell& cell = s.patterns[(size_t) p][(size_t) r][(size_t) c];
                    cell.note       = noteFromPeriod (period);
                    cell.instrument = (sample > 0 && sample <= 31) ? sample - 1 : -1;
                    // Nur die Effekte mit gleicher Nummerierung uebernehmen.
                    if (eff == 0x0 && par != 0)            { cell.effect = 0x0; cell.effectParam = par; }
                    else if (eff == 0x1 || eff == 0x2 || eff == 0x3 || eff == 0x4
                          || eff == 0xA || eff == 0xC || eff == 0xF)
                                                            { cell.effect = eff; cell.effectParam = par; }
                }
        }

        // Sample-PCM (8-Bit signed) ab dem Ende der Pattern-Daten, in Sample-Reihenfolge.
        size_t sofs = pofs + (size_t) s.numPatterns * patBytes;
        const double sr = sourceRateForMod();
        for (int i = 0; i < 31; ++i)
        {
            s.samples[i].name       = hdr[i].name;
            s.samples[i].volume     = hdr[i].volume;
            s.samples[i].sourceRate = sr;

            const int len = hdr[i].lenBytes;
            if (len > 0)
            {
                const int avail = (int) juce::jmin ((size_t) len, n > sofs ? n - sofs : (size_t) 0);
                if (avail > 1)
                {
                    s.samples[i].data.setSize (1, avail);
                    auto* w = s.samples[i].data.getWritePointer (0);
                    const float vscale = (float) hdr[i].volume / 64.0f; // Standard-Lautstaerke einbacken
                    for (int k = 0; k < avail; ++k)
                    {
                        const int sv = (int) (juce::int8) d[sofs + (size_t) k];
                        w[k] = ((float) sv / 128.0f) * vscale;
                    }
                }
                sofs += (size_t) len;
            }
        }

        s.ok = true;
        s.message = "OK";
        return s;
    }
}
