#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>
#include <cstdint>
#include <cstring>
#include "Mos6502.h"
#include "SID.h"   // reSIDfp-Chip (libs/residfp)

// =============================================================================
//  C64-SID-Musik (.sid) in RetroTrax - der eigene kleine C64-Kern.
//
//  Eine .sid-Datei (PSID/RSID) enthaelt 6502-Maschinencode (init/play) + Daten.
//  Wir spielen sie ab, indem wir den Code in 64 KB RAM laufen lassen (Mos6502) und
//  jeden Schreibzugriff auf den SID-Bereich D400.. an einen echten reSIDfp-Chip
//  weiterreichen. Kein C64-ROM noetig -> reine RAM-Tunes (die meisten PSID) laufen.
//
//  Header BIG-ENDIAN: 0 magic | 4 version | 6 dataOffset | 8 load | 10 init
//  12 play | 14 songs | 16 startSong | 18 speed | 22 title | 54 author | 86 released
//  load==0 -> erste zwei Datenbytes (LE) sind die Ladeadresse.
// =============================================================================
class SidPlayer
{
public:
    struct Info
    {
        bool ok = false;
        juce::String magic, title, author, released, message;
        int version = 0, songs = 0, startSong = 1;
        int loadAddr = 0, initAddr = 0, playAddr = 0;
        juce::uint32 speed = 0;
        int dataBytes = 0;
    };

    bool load (const juce::File& file)
    {
        info_ = Info();
        ready_ = false;
        juce::MemoryBlock mb;
        if (! file.loadFileAsData (mb) || mb.getSize() < 0x7C)
        {
            info_.message = "Datei zu klein oder nicht lesbar.";
            return false;
        }
        raw_.assign ((const uint8_t*) mb.getData(),
                     (const uint8_t*) mb.getData() + mb.getSize());

        const bool psid = match ("PSID"), rsid = match ("RSID");
        if (! psid && ! rsid)
        {
            info_.message = "Keine SID-Datei (Kennung PSID/RSID fehlt).";
            return false;
        }
        info_.magic     = psid ? "PSID" : "RSID";
        info_.version   = be16 (4);
        const int dataOff = be16 (6);
        info_.loadAddr  = be16 (8);
        info_.initAddr  = be16 (10);
        info_.playAddr  = be16 (12);
        info_.songs     = juce::jmax (1, (int) be16 (14));
        info_.startSong = juce::jmax (1, (int) be16 (16));
        info_.speed     = be32 (18);
        info_.title     = text (22);
        info_.author    = text (54);
        info_.released  = text (86);

        int d = dataOff;
        if (info_.loadAddr == 0 && d + 1 < (int) raw_.size())
        {
            info_.loadAddr = raw_[(size_t) d] | (raw_[(size_t) d + 1] << 8); // LE
            d += 2;
        }
        if (info_.initAddr == 0) info_.initAddr = info_.loadAddr;
        dataStart_ = d;
        info_.dataBytes = juce::jmax (0, (int) raw_.size() - d);
        currentSong_ = info_.startSong;
        info_.ok = true;
        return true;
    }

    const Info& info() const { return info_; }
    bool isLoaded() const    { return info_.ok; }
    bool isPlayable() const  { return info_.ok && ready_; }

    void prepareToPlay (double sampleRate)
    {
        sr_ = sampleRate > 0.0 ? sampleRate : 44100.0;
        cyclesPerSample_ = kClock / sr_;
        ready_ = false;
        try
        {
            chip_.setChipModel (reSIDfp::MOS6581);
            chip_.setSamplingParameters (kClock, reSIDfp::RESAMPLE, sr_);
            chip_.reset();
            ready_ = true;
        }
        catch (const reSIDfp::SIDError&) { ready_ = false; }
        cpu_.mem = ram_;
        cpu_.writeHook = [this] (uint16_t a, uint8_t v)
        {
            const int reg = (a - 0xD400) & 0x1F;
            if (reg <= 0x18) chip_.write (reg, v);
        };
        pendingCount_ = pendingPos_ = 0;
        cycleRemainder_ = 0.0;
    }

    void start (int song)
    {
        if (! info_.ok) return;
        currentSong_ = juce::jlimit (1, info_.songs, song);

        std::memset (ram_, 0, sizeof (ram_));
        ram_[0x01] = 0x37; // Standard-Bankumschaltung (RAM + I/O sichtbar)

        int addr = info_.loadAddr;
        for (int i = dataStart_; i < (int) raw_.size() && addr <= 0xFFFF; ++i, ++addr)
            ram_[addr] = raw_[(size_t) i];

        if (ready_) chip_.reset();
        pendingCount_ = pendingPos_ = 0;
        cycleRemainder_ = 0.0;

        cpu_.reset ((uint16_t) info_.initAddr);
        cpu_.runRoutine ((uint16_t) info_.initAddr, (uint8_t) (currentSong_ - 1), 0, 0);

        playIsIrq_ = (info_.playAddr == 0); // playAddr==0 -> IRQ-Handler (endet mit RTI)
        playAddr_ = (uint16_t) info_.playAddr;
        if (playAddr_ == 0)
            playAddr_ = (uint16_t) (ram_[0x0314] | (ram_[0x0315] << 8));

        cyclesToNextPlay_ = 0;
    }

    void restart() { start (currentSong_); }
    int  currentSong() const { return currentSong_; }

    void renderBlock (juce::AudioBuffer<float>& buffer, int offset, int num)
    {
        const int outCh = buffer.getNumChannels();
        if (! ready_ || playAddr_ == 0)
        {
            for (int ch = 0; ch < outCh; ++ch)
                buffer.clear (ch, offset, num);
            return;
        }

        int got = 0;
        while (pendingPos_ < pendingCount_ && got < num)
            emit (buffer, offset + got++, sampleBuf_[pendingPos_++], outCh);

        int guard = 0;
        while (got < num && guard++ < 512)
        {
            if (cyclesToNextPlay_ <= 0)
            {
                if (playAddr_ != 0)
                    cpu_.runRoutine (playAddr_, 0, 0, 0, playIsIrq_, kCyclesPerFrame * 8);
                cyclesToNextPlay_ += kCyclesPerFrame;
            }

            cycleRemainder_ += cyclesPerSample_ * (double) (num - got);
            int cyc = (int) cycleRemainder_;
            if (cyc <= 0) cyc = 1;
            if (cyc > cyclesToNextPlay_) cyc = juce::jmax (1, cyclesToNextPlay_);
            cycleRemainder_ -= cyc;
            cyclesToNextPlay_ -= cyc;

            pendingCount_ = chip_.clock ((unsigned int) cyc, sampleBuf_);
            pendingPos_ = 0;
            while (pendingPos_ < pendingCount_ && got < num)
                emit (buffer, offset + got++, sampleBuf_[pendingPos_++], outCh);
        }
        while (got < num)
            emit (buffer, offset + got++, 0, outCh);
    }

private:
    static constexpr double kClock = 985248.0;
    static constexpr int    kCyclesPerFrame = 19656;

    void emit (juce::AudioBuffer<float>& buffer, int pos, short raw, int outCh)
    {
        const float s = (float) raw * (1.0f / 32768.0f);
        for (int ch = 0; ch < outCh; ++ch)
            buffer.setSample (ch, pos, s);
    }

    bool match (const char* m) const
    {
        return raw_.size() >= 4 && raw_[0] == (uint8_t) m[0] && raw_[1] == (uint8_t) m[1]
            && raw_[2] == (uint8_t) m[2] && raw_[3] == (uint8_t) m[3];
    }
    juce::uint16 be16 (size_t o) const
    {
        return (o + 1 < raw_.size()) ? (juce::uint16) ((raw_[o] << 8) | raw_[o + 1]) : 0;
    }
    juce::uint32 be32 (size_t o) const
    {
        if (o + 3 >= raw_.size()) return 0;
        return ((juce::uint32) raw_[o] << 24) | ((juce::uint32) raw_[o + 1] << 16)
             | ((juce::uint32) raw_[o + 2] << 8) | (juce::uint32) raw_[o + 3];
    }
    juce::String text (size_t o) const
    {
        juce::String s;
        for (size_t i = 0; i < 32 && o + i < raw_.size(); ++i)
        {
            const char c = (char) raw_[o + i];
            if (c == 0) break;
            s += c;
        }
        return s.trim();
    }

    Info info_;
    std::vector<uint8_t> raw_;
    int dataStart_ = 0;

    uint8_t  ram_[65536] = {};
    Mos6502  cpu_;
    reSIDfp::SID chip_;
    double sr_ = 44100.0, cyclesPerSample_ = 22.0, cycleRemainder_ = 0.0;
    int    cyclesToNextPlay_ = 0, currentSong_ = 1;
    uint16_t playAddr_ = 0;
    bool   playIsIrq_ = false;
    short  sampleBuf_[2048] = {};
    int    pendingCount_ = 0, pendingPos_ = 0;
    bool   ready_ = false;
};
