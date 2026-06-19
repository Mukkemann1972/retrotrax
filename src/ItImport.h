#pragma once

#include "ImportCommon.h"
#include <cmath>

// Liest ein Impulse-Tracker-Modul (.it). Pragmatisch wie XmImport/S3mImport:
// Samples in Slots, Patterns (auf 64 Zeilen gekuerzt), Reihenfolge, Effekt-
// Teilmenge. GEPACKTE Samples (IT214-Kompression) werden ausgelassen (nicht
// dekomprimiert). Im Instrument-Modus wird die Instrument-Nummer direkt als
// Sample-Slot genommen (Naeherung; Huellkurven/NNA bleiben aussen vor).
//
// Tonhoehe: IT-Note 60 = C-5 = RetroTrax-Note 60; C5Speed ist die Abspielrate
// bei C-5 -> sourceRate = C5Speed direkt.
namespace ItImport
{
    using ImportCommon::Cell;
    using ImportCommon::Sample;
    using ImportCommon::Song;
    using ImportCommon::le16;
    using ImportCommon::le32;
    using ImportCommon::kNoteOff;

    // IT-Befehlsbuchstabe (1=A..26=Z) auf einen Tracker-Effekt abbilden.
    inline void mapItCommand (int cmd, int info, Cell& cell)
    {
        switch (cmd)
        {
            case 1:  ImportCommon::mapTrackerEffect (0xF, info, cell); break; // A Speed
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
        if (n < 120)  return n;        // 60 = C-5 = RetroTrax-Note 60
        if (n >= 253) return kNoteOff; // 253 Fade, 254 Cut, 255 Off
        return -1;
    }

    inline Song parse (const juce::File& file)
    {
        Song s;
        juce::MemoryBlock mb;
        if (! file.loadFileAsData (mb)) { s.message = "Datei nicht lesbar"; return s; }
        const auto* d = (const juce::uint8*) mb.getData();
        const size_t n = mb.getSize();
        if (n < 192) { s.message = "Datei zu klein fuer ein IT"; return s; }
        if (juce::String::fromUTF8 ((const char*) d, 4) != "IMPM")
        { s.message = "Keine IMPM-Signatur"; return s; }

        s.title = juce::String::fromUTF8 ((const char*) (d + 4), 26).trim();
        const int ordNum = (int) le16 (d + 32);
        const int insNum = (int) le16 (d + 34);
        const int smpNum = (int) le16 (d + 36);
        const int patNum = (int) le16 (d + 38);

        size_t o = 192;
        const size_t ordOff   = o; o += (size_t) ordNum;
        o += (size_t) insNum * 4;                       // Instrument-Zeiger (uebersprungen)
        const size_t smpPtrOff = o; o += (size_t) smpNum * 4;
        const size_t patPtrOff = o; o += (size_t) patNum * 4;
        if (o > n) { s.message = "IT-Tabellen abgeschnitten"; return s; }

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
        // Kanaldimension fest 64 (IT erlaubt bis 64 Spuren); applyImportedSong
        // nimmt davon nur die ersten 16. channels = hoechster benutzter Kanal + 1.
        constexpr int kCh = 64;
        int maxChan = 0;
        s.patterns.assign ((size_t) s.numPatterns,
                           std::vector<std::vector<Cell>> (64, std::vector<Cell> (kCh)));
        for (int p = 0; p < patNum; ++p)
        {
            const juce::uint32 ptr = le32 (d + patPtrOff + (size_t) p * 4);
            if (ptr == 0 || (size_t) ptr + 8 > n) continue; // leeres Pattern
            const size_t off  = (size_t) ptr;
            const size_t len  = le16 (d + off);
            const int    rows = (int) le16 (d + off + 2);
            size_t q          = off + 8;
            const size_t qEnd = juce::jmin (n, q + len);

            juce::uint8 prevMask[kCh] = {};
            juce::uint8 prevNote[kCh] = {}, prevInst[kCh] = {}, prevVol[kCh] = {};
            juce::uint8 prevCmd[kCh]  = {}, prevVal[kCh]  = {};

            for (int row = 0; row < rows && q < qEnd; ++row)
            {
                for (;;)
                {
                    if (q >= qEnd) break;
                    const juce::uint8 cv = d[q++];
                    if (cv == 0) break; // Zeilenende
                    const int ch = (cv - 1) & 63;

                    juce::uint8 mask = prevMask[ch];
                    if (cv & 128) { if (q < qEnd) mask = d[q++]; prevMask[ch] = mask; }

                    int note = -1000, inst = 0, vol = -1, cmd = 0, info = 0;
                    if ((mask & 1)  && q < qEnd)     { prevNote[ch] = d[q++]; note = prevNote[ch]; }
                    if ((mask & 2)  && q < qEnd)     { prevInst[ch] = d[q++]; inst = prevInst[ch]; }
                    if ((mask & 4)  && q < qEnd)     { prevVol[ch]  = d[q++]; vol  = prevVol[ch]; }
                    if ((mask & 8)  && q + 1 < qEnd) { prevCmd[ch]  = d[q++]; prevVal[ch] = d[q++];
                                                       cmd = prevCmd[ch]; info = prevVal[ch]; }
                    if (mask & 16)  { note = prevNote[ch]; }
                    if (mask & 32)  { inst = prevInst[ch]; }
                    if (mask & 64)  { vol  = prevVol[ch]; }
                    if (mask & 128) { cmd  = prevCmd[ch]; info = prevVal[ch]; }

                    if (row < 64 && ch < kCh)
                    {
                        if (ch + 1 > maxChan) maxChan = ch + 1;
                        Cell& cell = s.patterns[(size_t) p][(size_t) row][(size_t) ch];
                        if (note != -1000) cell.note = rtNote (note);
                        if (inst > 0)      cell.instrument = inst - 1;
                        if (vol >= 0 && vol <= 64) cell.volume = vol;
                        if (cmd != 0)      mapItCommand (cmd, info, cell);
                    }
                }
            }
        }
        s.channels = juce::jlimit (1, kCh, maxChan);

        // --- Samples --------------------------------------------------------
        s.samples.assign ((size_t) smpNum, Sample());
        int packedSkipped = 0;
        for (int i = 0; i < smpNum; ++i)
        {
            const juce::uint32 ptr = le32 (d + smpPtrOff + (size_t) i * 4);
            if (ptr == 0 || (size_t) ptr + 80 > n) continue;
            const size_t off = (size_t) ptr;
            if (juce::String::fromUTF8 ((const char*) (d + off), 4) != "IMPS") continue;

            const juce::uint8 flg = d[off + 18];
            if ((flg & 0x01) == 0) continue;             // kein Sample vorhanden
            const bool bits16     = (flg & 0x02) != 0;
            const bool compressed = (flg & 0x08) != 0;
            const juce::uint8 cvt = d[off + 46];
            const bool isSigned   = (cvt & 0x01) != 0;
            const int  volume     = juce::jlimit (0, 64, (int) d[off + 19]);
            const juce::uint32 length  = le32 (d + off + 48);
            juce::uint32 c5speed       = le32 (d + off + 60);
            const juce::uint32 dataPtr = le32 (d + off + 72);
            const juce::String iname   = juce::String::fromUTF8 ((const char*) (d + off + 20), 26).trim();
            if (c5speed < 1000 || c5speed > 192000) c5speed = 8363;

            Sample& out = s.samples[(size_t) i];
            out.name       = iname.isNotEmpty() ? iname : juce::String ("Sample ") + juce::String (i + 1);
            out.sourceRate = (double) c5speed;

            if (compressed) { ++packedSkipped; continue; } // IT214 nicht entpackt

            const size_t dataOff = (size_t) dataPtr;
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
                            const int v = isSigned ? (int) (juce::int16) raw : (int) raw - 32768;
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
                            const int v = isSigned ? (int) (juce::int8) d[dataOff + (size_t) f]
                                                   : (int) d[dataOff + (size_t) f] - 128;
                            w[f] = ((float) v / 128.0f) * vscale;
                        }
                    }
                }
            }
        }

        s.ok = true;
        s.message = packedSkipped > 0
                      ? juce::String (packedSkipped) + " gepackte Samples uebersprungen"
                      : "OK";
        return s;
    }
}
