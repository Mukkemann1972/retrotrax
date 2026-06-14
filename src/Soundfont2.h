#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>
#include <cstring>

// Minimaler, dependency-freier SoundFont-2-Leser (.sf2). SF2 ist ein offenes
// RIFF-Format: ein "smpl"-Block enthaelt alle Samples als 16-Bit-PCM hintereinander,
// ein "shdr"-Block die Kopfdaten jedes Samples (Name, Lage, Rate). Wir lesen nur
// die Kopfdaten (schnell, auch bei riesigen Banks) und holen die PCM-Daten eines
// einzelnen Samples erst beim Laden. Praeset-/Huellkurven-Logik lassen wir bewusst
// weg - RetroTrax will je Slot ein rohes Sample, wie bei WAV.
namespace sf2
{

struct Sample
{
    juce::String name;
    int         sampleRate = 44100;
    juce::int64 start = 0; // Frame-Index ab Anfang des smpl-Blocks
    juce::int64 end   = 0;
};

struct Bank
{
    juce::int64 smplOffset = 0; // Datei-Offset der PCM-Daten in Bytes
    juce::Array<Sample> samples;
    bool valid = false;
};

// Liest nur die Sample-Kopfdaten + die Lage des PCM-Blocks.
inline Bank readBank (const juce::File& file)
{
    Bank bank;
    juce::FileInputStream in (file);
    if (! in.openedOk())
        return bank;

    char id[4];
    auto readId = [&in, &id] { return in.read (id, 4) == 4; };

    if (! readId() || memcmp (id, "RIFF", 4) != 0) return bank;
    in.readInt(); // RIFF-Groesse
    if (! readId() || memcmp (id, "sfbk", 4) != 0) return bank;

    while (! in.isExhausted())
    {
        if (! readId()) break;
        const juce::uint32 size = (juce::uint32) in.readInt();
        const juce::int64 chunkStart = in.getPosition();
        const juce::int64 chunkEnd   = chunkStart + size + (size & 1);

        if (memcmp (id, "LIST", 4) == 0)
        {
            char listType[4];
            if (in.read (listType, 4) != 4) break;

            if (memcmp (listType, "sdta", 4) == 0)
            {
                while (in.getPosition() < chunkEnd && ! in.isExhausted())
                {
                    char sid[4];
                    if (in.read (sid, 4) != 4) break;
                    const juce::uint32 ssize = (juce::uint32) in.readInt();
                    const juce::int64 sstart = in.getPosition();
                    if (memcmp (sid, "smpl", 4) == 0)
                        bank.smplOffset = sstart;
                    in.setPosition (sstart + ssize + (ssize & 1));
                }
            }
            else if (memcmp (listType, "pdta", 4) == 0)
            {
                while (in.getPosition() < chunkEnd && ! in.isExhausted())
                {
                    char sid[4];
                    if (in.read (sid, 4) != 4) break;
                    const juce::uint32 ssize = (juce::uint32) in.readInt();
                    const juce::int64 sstart = in.getPosition();

                    if (memcmp (sid, "shdr", 4) == 0)
                    {
                        const int count = (int) (ssize / 46); // jeder shdr-Eintrag ist 46 Bytes
                        for (int i = 0; i < count; ++i)
                        {
                            char nm[21] = { 0 };
                            in.read (nm, 20);
                            const juce::uint32 dwStart = (juce::uint32) in.readInt();
                            const juce::uint32 dwEnd   = (juce::uint32) in.readInt();
                            in.readInt();  // dwStartloop
                            in.readInt();  // dwEndloop
                            const juce::uint32 rate    = (juce::uint32) in.readInt();
                            in.readByte(); // byOriginalKey
                            in.readByte(); // chCorrection
                            in.readShort(); // wSampleLink
                            const juce::uint16 type = (juce::uint16) in.readShort();

                            Sample s;
                            s.name       = juce::String (juce::CharPointer_UTF8 (nm)).trim();
                            s.sampleRate = rate > 0 ? (int) rate : 44100;
                            s.start      = (juce::int64) dwStart;
                            s.end        = (juce::int64) dwEnd;

                            const bool rom = (type & 0x8000) != 0; // ROM-Samples koennen wir nicht lesen
                            if (! rom && s.end > s.start && s.name.isNotEmpty()
                                && ! s.name.equalsIgnoreCase ("EOS"))
                                bank.samples.add (s);
                        }
                    }

                    in.setPosition (sstart + ssize + (ssize & 1));
                }
            }
            // INFO und andere Listen ueberspringen wir.
        }

        in.setPosition (chunkEnd);
    }

    bank.valid = bank.smplOffset > 0 && ! bank.samples.isEmpty();
    return bank;
}

// Holt die PCM-Daten genau eines Samples (mono, float). 60-Sekunden-Grenze wie
// beim normalen Laden, damit der Speicher nicht explodiert.
inline bool readSamplePCM (const juce::File& file, const Bank& bank, const Sample& s,
                           juce::AudioBuffer<float>& out, double& sampleRateOut)
{
    if (! bank.valid || s.end <= s.start)
        return false;

    juce::FileInputStream in (file);
    if (! in.openedOk())
        return false;

    const juce::int64 frames    = s.end - s.start;
    const juce::int64 maxFrames = (juce::int64) (s.sampleRate * 60.0);
    const int n = (int) juce::jmin (frames, maxFrames);
    if (n < 2)
        return false;

    in.setPosition (bank.smplOffset + s.start * 2);
    juce::HeapBlock<juce::uint8> raw ((size_t) n * 2);
    if (in.read (raw, n * 2) != n * 2)
        return false;

    out.setSize (1, n);
    auto* d = out.getWritePointer (0);
    for (int i = 0; i < n; ++i)
    {
        const auto v = (juce::int16) juce::ByteOrder::littleEndianShort (raw + (size_t) i * 2);
        d[i] = (float) v / 32768.0f;
    }
    sampleRateOut = (double) s.sampleRate;
    return true;
}

} // namespace sf2
