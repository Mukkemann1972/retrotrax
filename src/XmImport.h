#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>
#include <cmath>

// Liest ein FastTracker-2-Modul (.xm) - der "groessere Bruder" des MOD: mehr
// Kanaele, bis 256 Zeilen pro Pattern, Instrumente mit mehreren Samples,
// 8/16-Bit-Samples (delta-kodiert), Finetune + relative Note pro Sample.
//
// Bewusst pragmatisch, im selben Geist wie ModImport.h: rein parsen in eine
// neutrale Struktur, das Einsetzen in die Engine macht der Processor. Was
// RetroTrax (noch) nicht abbilden kann, wird fallen gelassen statt zu raten:
//   * pro Instrument nehmen wir nur das ERSTE Sample (RetroTrax: 1 Sample/Slot)
//   * Huellkurven / Loops / Panning / Vibrato der XM-Instrumente bleiben aussen vor
//   * nur Effekte mit gleicher Nummerierung wie im Tracker (0/1/2/3/4/A/C/F)
//   * Volume-Spalte 0x10..0x50 -> Lautstaerke 0..64, der Rest faellt weg
//
// Tonhoehe: XM-Note N (1 = C-0, 49 = C-4, 97 = Key-Off). RetroTrax: Note 60 =
// C-5, also faellt XM-C-4 (49) sauber auf RetroTrax-Note 48 (= C-4). Relative
// Note + Finetune eines Samples backen wir in dessen sourceRate ein, denn die
// Engine spielt ein Sample bei Note 60 mit "sourceRate" ab.
namespace XmImport
{
    struct Sample
    {
        juce::String name;
        juce::AudioBuffer<float> data; // mono, -1..1
        double sourceRate = 16726.0;
        int    volume = 64;            // 0..64 (Standard-Lautstaerke, schon eingebacken)
    };

    struct Cell { int note = -1, instrument = -1, volume = -1, effect = -1, effectParam = 0; };

    struct Song
    {
        bool ok = false;
        juce::String title;
        int channels = 4;
        int numPatterns = 0;
        int songLength = 1;
        int order[256] = { 0 };
        std::vector<Sample> samples;                          // ein Eintrag je Instrument-Slot
        std::vector<std::vector<std::vector<Cell>>> patterns; // [pattern][row][channel]
        juce::String message;
    };

    constexpr int kNoteOff = -2; // gleich wie TrackerEngine::kNoteOff

    inline juce::uint32 le16 (const juce::uint8* p) { return (juce::uint32) (p[0] | (p[1] << 8)); }
    inline juce::uint32 le32 (const juce::uint8* p)
    {
        return (juce::uint32) (p[0] | (p[1] << 8) | (p[2] << 16) | ((juce::uint32) p[3] << 24));
    }

    // XM-Note 1..96 -> RetroTrax-Note (49/C-4 -> 48/C-4). 97 = Key-Off, 0 = leer.
    inline int rtNote (int xmNote)
    {
        if (xmNote <= 0)  return -1;
        if (xmNote >= 97) return kNoteOff;
        return juce::jlimit (0, 119, xmNote - 1);
    }

    // C-4 = 8363 Hz Referenz. Engine spielt Sample bei Note 60 (C-5) mit sourceRate
    // -> sourceRate = 8363 * 2^((relNote + 12 + finetune/128)/12).
    inline double sourceRateFor (int relNote, int finetune)
    {
        return 8363.0 * std::pow (2.0, (relNote + 12 + finetune / 128.0) / 12.0);
    }

    // Nur Effekte uebernehmen, die der Tracker mit gleicher Nummer kennt.
    inline void mapEffect (int fxt, int fxp, Cell& cell)
    {
        if      (fxt == 0x0 && fxp != 0)  { cell.effect = 0x0; cell.effectParam = fxp; }
        else if (fxt == 0xB)              { cell.effect = 0xB; cell.effectParam = fxp; } // Position-Jump
        else if (fxt == 0xD)             { cell.effect = 0xD; cell.effectParam = (fxp >> 4) * 10 + (fxp & 0xF); } // Pattern-Break (dezimal)
        else if (fxt == 0x1 || fxt == 0x2 || fxt == 0x3 || fxt == 0x4
              || fxt == 0xA || fxt == 0xC || fxt == 0xF)
                                          { cell.effect = fxt; cell.effectParam = fxp; }
    }

    inline Song parse (const juce::File& file)
    {
        Song s;
        juce::MemoryBlock mb;
        if (! file.loadFileAsData (mb)) { s.message = "Datei nicht lesbar"; return s; }
        const auto* d = (const juce::uint8*) mb.getData();
        const size_t n = mb.getSize();
        if (n < 80) { s.message = "Datei zu klein fuer ein XM"; return s; }

        if (juce::String::fromUTF8 ((const char*) d, 17) != "Extended Module: ")
        { s.message = "Keine XM-Signatur"; return s; }
        if (d[37] != 0x1A) { s.message = "Beschaedigter XM-Kopf"; return s; }

        s.title = juce::String::fromUTF8 ((const char*) (d + 17), 20).trim();

        const juce::uint32 headerSize = le32 (d + 60); // ab Offset 60 gemessen
        if (60 + headerSize + 16 > n) { s.message = "XM-Kopf unvollstaendig"; return s; }

        const int songLen   = (int) le16 (d + 64);
        const int numChans  = (int) le16 (d + 68);
        const int numPats   = (int) le16 (d + 70);
        const int numInsts  = (int) le16 (d + 72);

        if (numChans <= 0 || numChans > 64) { s.message = "Ungueltige Kanalzahl"; return s; }
        s.channels    = numChans;
        s.songLength  = juce::jlimit (1, 256, songLen);
        s.numPatterns = juce::jmax (1, numPats);

        for (int i = 0; i < 256; ++i)
            s.order[i] = (i < (int) headerSize - 20) ? d[80 + i] : 0; // Reihenfolge ab Offset 80

        // --- Patterns (direkt nach dem Kopf) --------------------------------
        size_t o = 60 + headerSize;
        s.patterns.resize ((size_t) s.numPatterns);
        for (int p = 0; p < s.numPatterns; ++p)
        {
            if (o + 9 > n) { s.message = "Pattern-Kopf abgeschnitten"; return s; }
            const juce::uint32 patHdr  = le32 (d + o);
            const int rows             = juce::jlimit (1, 256, (int) le16 (d + o + 5));
            const juce::uint32 packed  = le16 (d + o + 7);

            s.patterns[(size_t) p].assign ((size_t) rows,
                                           std::vector<Cell> ((size_t) numChans));

            size_t q   = o + patHdr;          // Anfang der gepackten Notendaten
            const size_t qEnd = q + packed;   // (packed == 0 -> leeres Pattern)
            for (int r = 0; r < rows && q < qEnd; ++r)
                for (int c = 0; c < numChans && q < qEnd; ++c)
                {
                    const juce::uint8 b = d[q++];
                    int note = 0, ins = 0, vol = 0, fxt = 0, fxp = 0;
                    if (b & 0x80)
                    {
                        if ((b & 0x01) && q < qEnd) note = d[q++];
                        if ((b & 0x02) && q < qEnd) ins  = d[q++];
                        if ((b & 0x04) && q < qEnd) vol  = d[q++];
                        if ((b & 0x08) && q < qEnd) fxt  = d[q++];
                        if ((b & 0x10) && q < qEnd) fxp  = d[q++];
                    }
                    else
                    {
                        note = b;
                        if (q < qEnd) ins = d[q++];
                        if (q < qEnd) vol = d[q++];
                        if (q < qEnd) fxt = d[q++];
                        if (q < qEnd) fxp = d[q++];
                    }

                    Cell& cell = s.patterns[(size_t) p][(size_t) r][(size_t) c];
                    cell.note       = rtNote (note);
                    cell.instrument = (ins > 0) ? ins - 1 : -1;
                    if (vol >= 0x10 && vol <= 0x50) cell.volume = vol - 0x10; // Lautstaerke 0..64
                    mapEffect (fxt, fxp, cell);
                }

            o += patHdr + packed;
        }

        // --- Instrumente (je Instrument: Kopf + Sample-Koepfe + Sample-Daten) -
        s.samples.assign ((size_t) numInsts, Sample());
        for (int i = 0; i < numInsts; ++i)
        {
            if (o + 29 > n) break; // Rest fehlt - so viel wie da ist behalten
            const juce::uint32 instHdr  = le32 (d + o);
            const juce::String iname    = juce::String::fromUTF8 ((const char*) (d + o + 4), 22).trim();
            const int numSamp           = (int) le16 (d + o + 27);

            if (numSamp <= 0) { o += instHdr; continue; } // leeres Instrument

            const juce::uint32 sampHdr = le32 (d + o + 29);
            size_t sh = o + instHdr;                       // erster Sample-Kopf

            struct SH { int lenBytes, vol, finetune, relNote; bool bits16; juce::String name; };
            std::vector<SH> sh16; sh16.reserve ((size_t) numSamp);
            for (int k = 0; k < numSamp; ++k)
            {
                if (sh + 18 > n) break;
                SH h;
                h.lenBytes = (int) le32 (d + sh);
                h.vol      = juce::jlimit (0, 64, (int) d[sh + 12]);
                h.finetune = (int) (juce::int8) d[sh + 13];
                h.bits16   = (d[sh + 14] & 0x10) != 0;
                h.relNote  = (int) (juce::int8) d[sh + 16];
                h.name     = juce::String::fromUTF8 ((const char*) (d + sh + 18),
                                                     juce::jmin (22, (int) sampHdr - 18)).trim();
                sh16.push_back (h);
                sh += sampHdr;
            }

            // Sample-Daten folgen ALLEN Sample-Koepfen, hintereinander.
            size_t data = o + instHdr + (size_t) numSamp * sampHdr;

            // Wir nehmen nur das erste Sample fuer den Slot.
            if (! sh16.empty())
            {
                const SH& h = sh16[0];
                Sample& out = s.samples[(size_t) i];
                out.name       = h.name.isNotEmpty() ? h.name : iname;
                out.volume     = h.vol;
                out.sourceRate = sourceRateFor (h.relNote, h.finetune);

                const int avail = (int) juce::jmin ((size_t) juce::jmax (0, h.lenBytes),
                                                    n > data ? n - data : (size_t) 0);
                const float vscale = (float) h.vol / 64.0f;
                if (h.bits16)
                {
                    const int frames = avail / 2;
                    if (frames > 1)
                    {
                        out.data.setSize (1, frames);
                        auto* w = out.data.getWritePointer (0);
                        juce::int16 acc = 0; // delta-kodiert
                        for (int f = 0; f < frames; ++f)
                        {
                            acc = (juce::int16) (acc + (juce::int16) le16 (d + data + (size_t) f * 2));
                            w[f] = ((float) acc / 32768.0f) * vscale;
                        }
                    }
                }
                else
                {
                    if (avail > 1)
                    {
                        out.data.setSize (1, avail);
                        auto* w = out.data.getWritePointer (0);
                        juce::int8 acc = 0; // delta-kodiert
                        for (int f = 0; f < avail; ++f)
                        {
                            acc = (juce::int8) (acc + (juce::int8) d[data + (size_t) f]);
                            w[f] = ((float) acc / 128.0f) * vscale;
                        }
                    }
                }
            }

            // Hinter alle Sample-Daten springen (auch die ungenutzten).
            for (const auto& h : sh16) data += (size_t) juce::jmax (0, h.lenBytes);
            o = data;
        }

        s.ok = true;
        s.message = "OK";
        return s;
    }
}
