#pragma once

#include "ImportCommon.h"
#include <cmath>

// Liest ein Scream-Tracker-3-Modul (.s3m). Pragmatisch wie XmImport: Samples in
// Slots, Patterns (64 Zeilen), Reihenfolge, Effekt-Teilmenge. Adlib-Instrumente
// und gepackte Samples (selten) werden ausgelassen. Stereo-Samples werden als
// Mono (erster Kanal) gelesen.
//
// Tonhoehe: ST3-Note = (Oktave<<4)|Halbton, C-4 (=Byte 0x40, abs 48) spielt das
// Sample bei seiner c2spd. RetroTrax spielt sourceRate bei Note 60 (C-5), also
// sourceRate = 2 * c2spd; ST3-Note-Abs faellt direkt auf die RetroTrax-Note.
namespace S3mImport
{
    using ImportCommon::Cell;
    using ImportCommon::Sample;
    using ImportCommon::Song;
    using ImportCommon::le16;
    using ImportCommon::le32;
    using ImportCommon::kNoteOff;

    // ST3-Befehlsbuchstabe (1=A..26=Z) auf einen Tracker-Effekt abbilden.
    inline void mapS3mCommand (int cmd, int info, Cell& cell)
    {
        switch (cmd)
        {
            case 1:  ImportCommon::mapTrackerEffect (0xF, info, cell); break; // A Speed
            case 2:  ImportCommon::mapTrackerEffect (0xB, info, cell); break; // B Position-Jump
            case 3:  ImportCommon::mapTrackerEffect (0xD, (info >> 4) * 10 + (info & 0xF), cell); break; // C Pattern-Break (dezimal)
            case 4:  ImportCommon::mapTrackerEffect (0xA, info, cell); break; // D Vol-Slide
            case 5:  ImportCommon::mapTrackerEffect (0x2, info, cell); break; // E Porta runter
            case 6:  ImportCommon::mapTrackerEffect (0x1, info, cell); break; // F Porta hoch
            case 7:  ImportCommon::mapTrackerEffect (0x3, info, cell); break; // G Tone-Porta
            case 8:  ImportCommon::mapTrackerEffect (0x4, info, cell); break; // H Vibrato
            case 10: ImportCommon::mapTrackerEffect (0x0, info, cell); break; // J Arpeggio
            case 20: ImportCommon::mapTrackerEffect (0xF, info, cell); break; // T Tempo (BPM)
            default: break;
        }
    }

    inline int rtNote (int n)
    {
        if (n == 255) return -1;       // leer
        if (n == 254) return kNoteOff; // Note-Aus
        return juce::jlimit (0, 119, (n >> 4) * 12 + (n & 0x0F));
    }

    inline Song parse (const juce::File& file)
    {
        Song s;
        juce::MemoryBlock mb;
        if (! file.loadFileAsData (mb)) { s.message = "Datei nicht lesbar"; return s; }
        const auto* d = (const juce::uint8*) mb.getData();
        const size_t n = mb.getSize();
        if (n < 96) { s.message = "Datei zu klein fuer ein S3M"; return s; }
        if (juce::String::fromUTF8 ((const char*) (d + 44), 4) != "SCRM")
        { s.message = "Keine SCRM-Signatur"; return s; }

        s.title = juce::String::fromUTF8 ((const char*) d, 28).trim();
        const int ordNum = (int) le16 (d + 32);
        const int insNum = (int) le16 (d + 34);
        const int patNum = (int) le16 (d + 36);
        const int ffi    = (int) le16 (d + 42); // 1 = signed, 2 = unsigned
        const bool unsignedSamples = (ffi != 1);

        // Aktive Kanaele zaehlen (Wert < 16 = benutzt; 16..31 Adlib, 255 ungenutzt).
        int channels = 0;
        for (int i = 0; i < 32; ++i)
            if (d[64 + i] < 16)
                channels = i + 1; // hoechster benutzter Index + 1
        channels = juce::jlimit (1, 32, channels);
        s.channels = channels;

        size_t o = 96;
        const size_t ordOff = o;
        o += (size_t) ordNum;
        const size_t insParaOff = o; o += (size_t) insNum * 2;
        const size_t patParaOff = o; o += (size_t) patNum * 2;
        if (o > n) { s.message = "S3M-Tabellen abgeschnitten"; return s; }

        // Reihenfolge (254 = Marker, 255 = Ende -> auslassen).
        for (int i = 0; i < ordNum; ++i)
        {
            const int v = d[ordOff + (size_t) i];
            if (v < patNum)
                s.order.push_back (v);
        }
        if (s.order.empty()) s.order.push_back (0);
        s.songLength  = (int) s.order.size();
        s.numPatterns = juce::jmax (1, patNum);

        // --- Patterns -------------------------------------------------------
        s.patterns.assign ((size_t) s.numPatterns,
                           std::vector<std::vector<Cell>> (64, std::vector<Cell> ((size_t) channels)));
        for (int p = 0; p < patNum; ++p)
        {
            const juce::uint32 para = le16 (d + patParaOff + (size_t) p * 2);
            if (para == 0) continue; // leeres Pattern
            size_t q = (size_t) para * 16;
            if (q + 2 > n) continue;
            const size_t packedLen = le16 (d + q);
            q += 2;
            const size_t qEnd = juce::jmin (n, q + packedLen);

            for (int row = 0; row < 64 && q < qEnd; ++row)
            {
                for (;;)
                {
                    if (q >= qEnd) break;
                    const juce::uint8 what = d[q++];
                    if (what == 0) break; // Zeilenende
                    const int chan = what & 31;
                    int note = 255, inst = 0, vol = 255, cmd = 0, info = 0;
                    if ((what & 32) && q + 1 < qEnd) { note = d[q++]; inst = d[q++]; }
                    if ((what & 64) && q < qEnd)      { vol  = d[q++]; }
                    if ((what & 128) && q + 1 < qEnd) { cmd  = d[q++]; info = d[q++]; }

                    if (chan < channels)
                    {
                        Cell& cell = s.patterns[(size_t) p][(size_t) row][(size_t) chan];
                        if (note != 255) cell.note = rtNote (note);
                        if (inst > 0)    cell.instrument = inst - 1;
                        if (vol <= 64)   cell.volume = vol;
                        if (cmd != 0)    mapS3mCommand (cmd, info, cell);
                    }
                }
            }
        }

        // --- Instrumente / Samples -----------------------------------------
        s.samples.assign ((size_t) insNum, Sample());
        for (int i = 0; i < insNum; ++i)
        {
            const juce::uint32 para = le16 (d + insParaOff + (size_t) i * 2);
            if (para == 0) continue;
            const size_t off = (size_t) para * 16;
            if (off + 80 > n) continue;
            if (d[off] != 1) continue; // nur Typ 1 = Sample (kein Adlib/leer)

            const juce::uint32 length = le32 (d + off + 16);
            const juce::uint32 memseg = ((juce::uint32) d[off + 13] << 16) | le16 (d + off + 14);
            const size_t dataOff = (size_t) memseg * 16;
            const juce::uint8 flags = d[off + 31];
            const bool bits16 = (flags & 0x04) != 0;
            juce::uint32 c2spd = le32 (d + off + 32);
            if (c2spd < 1000 || c2spd > 192000) c2spd = 8363;
            const int volume = juce::jlimit (0, 64, (int) d[off + 28]);
            const juce::String iname = juce::String::fromUTF8 ((const char*) (d + off + 48), 28).trim();

            Sample& out = s.samples[(size_t) i];
            out.name       = iname.isNotEmpty() ? iname : juce::String ("Sample ") + juce::String (i + 1);
            out.sourceRate = 2.0 * (double) c2spd;

            const int frames = (int) juce::jmin ((juce::uint32) 0x3FFFFF, length);
            const float vscale = (float) volume / 64.0f;
            if (frames > 1 && dataOff < n)
            {
                if (bits16)
                {
                    const int avail = (int) juce::jmin ((size_t) frames, (n - dataOff) / 2);
                    if (avail > 1)
                    {
                        out.data.setSize (1, avail);
                        auto* w = out.data.getWritePointer (0);
                        for (int f = 0; f < avail; ++f)
                        {
                            const juce::uint32 raw = le16 (d + dataOff + (size_t) f * 2);
                            const int v = unsignedSamples ? (int) raw - 32768 : (int) (juce::int16) raw;
                            w[f] = ((float) v / 32768.0f) * vscale;
                        }
                    }
                }
                else
                {
                    const int avail = (int) juce::jmin ((size_t) frames, n - dataOff);
                    if (avail > 1)
                    {
                        out.data.setSize (1, avail);
                        auto* w = out.data.getWritePointer (0);
                        for (int f = 0; f < avail; ++f)
                        {
                            const int v = unsignedSamples ? (int) d[dataOff + (size_t) f] - 128
                                                          : (int) (juce::int8) d[dataOff + (size_t) f];
                            w[f] = ((float) v / 128.0f) * vscale;
                        }
                    }
                }
            }
        }

        s.ok = true;
        s.message = "OK";
        return s;
    }
}
