#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>
#include <cstdint>
#include <algorithm>
#include "tfmxaudiodecoder.h" // vendored GPL-Decoder (libs/tfmxdecoder), C-API

// =============================================================================
//  TFMX (Chris Huelsbeck, Amiga) - Wiedergabe in RetroTrax.
//
//  TFMX ("The Final Musicsystem eXtended") ist KEIN Pattern-Raster wie MOD/XM,
//  sondern eine laufende Makro-Engine: der Sequencer (Tracksteps -> Patterns)
//  und der Synthesizer (Makros pro Stimme) laufen getrennt. Faithful TFMX heisst
//  deshalb Replayer (wie unser reSIDfp-Chip ein Renderer ist), nicht Import.
//
//  Diese Klasse macht zweierlei, beides eigener RetroTrax-Code:
//   1) Eigener Datei-Leser fuer die DIAGNOSE (Subsongs/Patterns/Makros/...),
//      gegen echte Dateien (Apidya, Turrican 2) verifiziert.
//   2) Duenner Wrapper um den bewaehrten, offenen (GPL) TFMX-Decoder aus
//      libs/tfmxdecoder fuer die eigentliche WIEDERGABE (wie reSIDfp den SID-
//      Klang liefert). Angesprochen nur ueber dessen C-API.
//
//  Dateien: .mdat (Big-Endian Song-/Pattern-/Makro-Daten), .smpl (8-Bit-Samples).
//  Header-Offsets (verifiziert): Magic beginnt mit "TFMX"; Subsong-Tabellen
//  start/end/tempo je 32xuint16 ab 0x100/0x140/0x180; Zeiger trackTbl@0x1D0,
//  patTbl@0x1D4, macTbl@0x1D8. Daten vorn, Zeiger-Tabellen hinten:
//  #Patterns=(mac-pat)/4, #Makros=(EOF-mac)/4, #Tracksteps=(be32(pat)-trk)/16.
// =============================================================================
class TfmxPlayer
{
public:
    struct Info
    {
        bool ok = false;
        juce::String title;
        int subsongs    = 0;
        int patterns    = 0;
        int macros      = 0;
        int tracksteps  = 0;
        int sampleBytes = 0;
        juce::String message; // Fehlertext, falls ok == false
    };

    TfmxPlayer() = default;
    ~TfmxPlayer()
    {
        const juce::SpinLock::ScopedLockType sl (lock_);
        if (dec_ != nullptr) tfmxdec_delete (dec_);
    }

    // Liest mdat (+ smpl) ein: fuellt info() fuer die Diagnose UND initialisiert
    // den Decoder fuer die Wiedergabe. Big-Endian. Liefert false, wenn die Datei
    // nicht wie ein TFMX-Modul aussieht (Diagnose); isPlayable() sagt separat,
    // ob die Wiedergabe bereit ist.
    bool load (const juce::File& mdatFile, const juce::File& smplFile)
    {
        info_ = Info();

        // --- 1) Diagnose: eigener Leser ------------------------------------
        juce::MemoryBlock mb;
        if (! mdatFile.loadFileAsData (mb) || mb.getSize() < 0x200)
        {
            info_.message = "mdat-Datei zu klein oder nicht lesbar.";
            return false;
        }
        mdat_.assign ((const uint8_t*) mb.getData(),
                      (const uint8_t*) mb.getData() + mb.getSize());

        if (! (mdat_[0] == 'T' && mdat_[1] == 'F' && mdat_[2] == 'M' && mdat_[3] == 'X'))
        {
            info_.message = "Kein TFMX-Modul (Kennung \"TFMX\" fehlt am Anfang).";
            return false;
        }

        // Sample-Speicher mitladen (fuer Wiedergabe-Diagnose UND den Grabber).
        smpl_.clear();
        if (smplFile.existsAsFile())
        {
            juce::MemoryBlock sb;
            if (smplFile.loadFileAsData (sb))
                smpl_.assign ((const uint8_t*) sb.getData(),
                              (const uint8_t*) sb.getData() + sb.getSize());
            info_.sampleBytes = (int) smpl_.size();
        }

        trackTbl_ = be32 (0x1D0);
        patTbl_   = be32 (0x1D4);
        macTbl_   = be32 (0x1D8);

        const uint32_t fileSize = (uint32_t) mdat_.size();
        if (patTbl_ < 0x200 || macTbl_ <= patTbl_ || macTbl_ > fileSize)
        {
            info_.message = "TFMX-Tabellen-Zeiger unplausibel (evtl. exotische Variante).";
            return false;
        }

        info_.patterns = (int) ((macTbl_ - patTbl_) / 4);
        info_.macros   = (int) ((fileSize - macTbl_) / 4);

        const uint32_t firstPatData = be32At (patTbl_);
        if (firstPatData > trackTbl_ && firstPatData <= fileSize && trackTbl_ >= 0x200)
            info_.tracksteps = (int) ((firstPatData - trackTbl_) / 16);

        info_.subsongs = 0;
        for (int i = 0; i < 32; ++i)
        {
            const uint16_t s = be16 (0x100 + (size_t) i * 2);
            const uint16_t e = be16 (0x140 + (size_t) i * 2);
            if (s >= 0x1FF) break;
            if (e >= s && ! (s == 0 && e == 0 && i > 0))
                ++info_.subsongs;
        }
        if (info_.subsongs == 0) info_.subsongs = 1;

        info_.title = mdatTitleFromFile (mdatFile);
        info_.ok = true;

        // --- 2) Wiedergabe: bewaehrten Decoder initialisieren --------------
        initDecoder (mdatFile);
        return true;
    }

    const Info& info() const  { return info_; }
    bool isLoaded() const     { return info_.ok; }
    bool isPlayable() const   { return playable_.load(); }

    // -------------------------------------------------------------------------
    //  GRABBER: einzelne Samples (Instrumente) aus dem TFMX entnehmen.
    //
    //  TFMX legt alle Samples hintereinander in den .smpl-Speicher; WO ein
    //  Sample beginnt und wie lang es ist, steht NICHT in einer Tabelle, sondern
    //  in den Makro-Befehlen. Pro 4-Byte-Befehl: op, bb, cd, ee. Relevant
    //  (Opcodes wie im vendorten Decoder, Macro.cpp):
    //    0x01 StartSample (DMAon)  - aktuelles (Begin,Len) faengt an zu spielen
    //    0x02 SetBegin   begin = bb<<16 | cd<<8 | ee   (Byte-Offset in .smpl)
    //    0x03 SetLen     len   = cd<<8 | ee            (in WORTEN = 2 Byte)
    //    0x07 Stop / 0x16 Return - Makro-Ende
    //  Wir laufen jede Makro-Liste linear ab, merken den letzten Begin/Len und
    //  sammeln so alle wirklich benutzten Sample-Bereiche (entdoppelt).
    // -------------------------------------------------------------------------
    struct Grab
    {
        juce::String name;
        juce::AudioBuffer<float> audio; // mono, 8-Bit-signed -> float
    };

    std::vector<Grab> grabSamples() const
    {
        std::vector<Grab> out;
        if (! info_.ok || smpl_.empty() || macTbl_ == 0)
            return out;

        const uint32_t fileSize = (uint32_t) mdat_.size();
        struct Region { uint32_t begin; uint32_t bytes; };
        std::vector<Region> regions;

        auto record = [&] (int32_t begin, uint32_t words)
        {
            if (begin < 0 || words == 0)
                return;
            uint32_t bytes = words * 2;
            if ((uint32_t) begin >= smpl_.size())
                return;
            if ((uint32_t) begin + bytes > smpl_.size())
                bytes = (uint32_t) smpl_.size() - (uint32_t) begin;
            if (bytes < 4)
                return;
            // Dasselbe Sample wird oft mit mehreren Abspiel-Laengen referenziert
            // (z.B. kurz angetippt vs. ganz). Wir behalten je Startpunkt die
            // laengste Variante = ein Eintrag pro echtem Sample.
            for (auto& r : regions)
                if (r.begin == (uint32_t) begin)
                {
                    r.bytes = juce::jmax (r.bytes, bytes);
                    return;
                }
            regions.push_back ({ (uint32_t) begin, bytes });
        };

        for (int i = 0; i < info_.macros; ++i)
        {
            uint32_t p = be32At (macTbl_ + (uint32_t) i * 4);
            if (p < 0x200 || p + 4 > fileSize)
                continue;

            int32_t  curBegin = -1;
            uint32_t curLen   = 0;
            for (int step = 0; step < 512 && p + 4 <= fileSize; ++step, p += 4)
            {
                const uint8_t op = mdat_[p];
                const uint8_t bb = mdat_[p + 1];
                const uint8_t cd = mdat_[p + 2];
                const uint8_t ee = mdat_[p + 3];

                if (op == 0x02)
                    curBegin = (int32_t) (((uint32_t) bb << 16) | ((uint32_t) cd << 8) | ee);
                else if (op == 0x03)
                {
                    curLen = ((uint32_t) cd << 8) | ee;
                    record (curBegin, curLen);
                }
                else if (op == 0x01)
                    record (curBegin, curLen);
                else if (op == 0x07 || op == 0x16) // Stop / Return = Makro-Ende
                    break;
            }
        }

        std::sort (regions.begin(), regions.end(),
                   [] (const Region& a, const Region& b) { return a.begin < b.begin; });

        int idx = 1;
        for (const auto& r : regions)
        {
            Grab g;
            g.name = info_.title + " #" + juce::String (idx++);
            g.audio.setSize (1, (int) r.bytes);
            auto* w = g.audio.getWritePointer (0);
            for (uint32_t s = 0; s < r.bytes; ++s)
                w[s] = (float) (int8_t) smpl_[r.begin + s] / 128.0f;
            out.push_back (std::move (g));
        }
        return out;
    }

    void prepare (double sampleRate)
    {
        sampleRate_ = sampleRate;
        scratch_.assign ((size_t) kMaxFrames * 2, 0);
        const juce::SpinLock::ScopedLockType sl (lock_);
        if (dec_ != nullptr && playable_.load())
            tfmxdec_mixer_init (dec_, (int) sampleRate_, 16, 2, 0, 75);
    }

    // PLAY startet den Song von vorn (vom Audio-Thread beim Wiedergabe-Beginn).
    void restart()
    {
        const juce::SpinLock::ScopedLockType sl (lock_);
        if (dec_ != nullptr && playable_.load())
            tfmxdec_reinit (dec_, -1); // aktuellen Song neu starten
    }

    // Audio-Thread: fuellt buffer[start .. start+num] mit der TFMX-Wiedergabe.
    void render (juce::AudioBuffer<float>& buffer, int start, int num)
    {
        const juce::SpinLock::ScopedTryLockType sl (lock_);
        const int chs = buffer.getNumChannels();
        if (! sl.isLocked() || dec_ == nullptr || ! playable_.load()
            || num > kMaxFrames || (int) scratch_.size() < num * 2)
        {
            for (int ch = 0; ch < chs; ++ch)
                buffer.clear (ch, start, num);
            return;
        }

        // Decoder liefert interleaved signed-16-bit Stereo (L,R,L,R, ...).
        tfmxdec_buffer_fill (dec_, scratch_.data(),
                             (uint32_t) (num * 2 * (int) sizeof (int16_t)));

        auto* L = chs > 0 ? buffer.getWritePointer (0, start) : nullptr;
        auto* R = chs > 1 ? buffer.getWritePointer (1, start) : nullptr;
        for (int i = 0; i < num; ++i)
        {
            const float l = (float) scratch_[(size_t) i * 2]     / 32768.0f;
            const float r = (float) scratch_[(size_t) i * 2 + 1] / 32768.0f;
            if (L != nullptr) L[i] = l;
            if (R != nullptr) R[i] = r;
        }
    }

private:
    static constexpr int kMaxFrames = 16384; // groessere Bloecke gibt es praktisch nie

    void initDecoder (const juce::File& mdatFile)
    {
        const juce::SpinLock::ScopedLockType sl (lock_);
        playable_ = false;
        if (dec_ != nullptr) { tfmxdec_delete (dec_); dec_ = nullptr; }

        dec_ = tfmxdec_new();
        if (dec_ == nullptr) return;

        const auto path = mdatFile.getFullPathName();
        tfmxdec_set_path (dec_, path.toRawUTF8());            // findet die .smpl daneben
        if (tfmxdec_load (dec_, path.toRawUTF8(), 0) != 0)    // Song 0
        {
            tfmxdec_mixer_init (dec_, (int) sampleRate_, 16, 2, 0, 75);
            playable_ = true;
        }
    }

    static juce::String mdatTitleFromFile (const juce::File& f)
    {
        auto name = f.getFileName();
        if (name.startsWithIgnoreCase ("mdat."))
            return name.substring (5);
        if (f.getFileExtension().equalsIgnoreCase (".mdat"))
            return f.getFileNameWithoutExtension();
        return name;
    }

    uint16_t be16 (size_t off) const
    {
        if (off + 1 >= mdat_.size()) return 0;
        return (uint16_t) ((mdat_[off] << 8) | mdat_[off + 1]);
    }
    uint32_t be32 (size_t off) const { return be32At ((uint32_t) off); }
    uint32_t be32At (uint32_t off) const
    {
        if ((size_t) off + 3 >= mdat_.size()) return 0;
        return ((uint32_t) mdat_[off]     << 24) | ((uint32_t) mdat_[off + 1] << 16)
             | ((uint32_t) mdat_[off + 2] <<  8) |  (uint32_t) mdat_[off + 3];
    }

    Info info_;
    std::vector<uint8_t> mdat_;
    std::vector<uint8_t> smpl_;   // roher 8-Bit-Sample-Speicher (fuer den Grabber)
    double sampleRate_ = 44100.0;

    uint32_t trackTbl_ = 0, patTbl_ = 0, macTbl_ = 0;

    // Wiedergabe (bewaehrter Decoder hinter der C-API).
    void* dec_ = nullptr;
    std::atomic<bool> playable_ { false };
    juce::SpinLock lock_;                 // schuetzt dec_ zwischen Laden + Audio-Thread
    std::vector<int16_t> scratch_;        // interleaved 16-bit Stereo-Zwischenpuffer
};
