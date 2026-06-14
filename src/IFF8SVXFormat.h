#pragma once

#include <juce_audio_formats/juce_audio_formats.h>
#include <vector>

// Liest das native Amiga-Sample-Format 8SVX (im IFF-Container) ein - das
// Format der klassischen ProTracker/SoundTracker-Welt. Frei dokumentiert,
// also voll im Geist von RetroTrax. Unterstuetzt unkomprimierte Samples und
// die Fibonacci-Delta-Kompression; mehroktavige Samples spielen die erste
// (tiefste) Oktave. 8 Bit, mono, big-endian.
class IFF8SVXReader : public juce::AudioFormatReader
{
public:
    explicit IFF8SVXReader (juce::InputStream* in)
        : juce::AudioFormatReader (in, "IFF/8SVX")
    {
        usesFloatingPointData = true;
        bitsPerSample = 8;
        numChannels   = 1;
        sampleRate    = 8363; // Amiga-Standard, falls das VHDR keine Rate nennt

        valid = parse();
        lengthInSamples = (juce::int64) pcm.size();
    }

    bool isValid() const noexcept { return valid; }

    // Der AudioFormat ruft das, wenn der Stream doch nicht uns gehoeren soll.
    void releaseStreamOwnership() noexcept { input = nullptr; }

    bool readSamples (int* const* destChannels, int numDestChannels,
                      int startOffsetInDestBuffer, juce::int64 startSampleInFile,
                      int numSamples) override
    {
        for (int ch = 0; ch < numDestChannels; ++ch)
        {
            if (destChannels[ch] == nullptr)
                continue;

            auto* dest = reinterpret_cast<float*> (destChannels[ch]) + startOffsetInDestBuffer;
            for (int i = 0; i < numSamples; ++i)
            {
                const juce::int64 pos = startSampleInFile + i;
                dest[i] = (ch == 0 && pos >= 0 && pos < (juce::int64) pcm.size())
                              ? pcm[(size_t) pos] : 0.0f;
            }
        }
        return true;
    }

private:
    bool valid = false;
    std::vector<float> pcm;

    // Fibonacci-Delta-Dekompression (sCompression == 1). Die ersten beiden
    // Bytes sind Vorspann (zweites = Startwert), danach je Byte zwei 4-Bit-Codes.
    static void fibonacciDecode (const std::vector<unsigned char>& src,
                                 std::vector<signed char>& out)
    {
        static const int delta[16] = { -34, -21, -13, -8, -5, -3, -2, -1,
                                          0,   1,   2,  3,  5,  8, 13, 21 };
        if (src.size() < 2)
        {
            out.clear();
            return;
        }

        int x = (signed char) src[1];
        const size_t n = src.size() - 2;
        out.resize (n * 2);
        for (size_t i = 0; i < n * 2; ++i)
        {
            const unsigned char d = src[2 + (i >> 1)];
            const int code = (i & 1) ? (d & 0x0f) : (d >> 4);
            x += delta[code];
            x = juce::jlimit (-128, 127, x);
            out[i] = (signed char) x;
        }
    }

    bool parse()
    {
        if (input == nullptr)
            return false;
        input->setPosition (0);

        char id[4];
        if (input->read (id, 4) != 4 || memcmp (id, "FORM", 4) != 0)
            return false;
        input->readIntBigEndian(); // FORM-Groesse (egal)
        if (input->read (id, 4) != 4 || memcmp (id, "8SVX", 4) != 0)
            return false;

        juce::uint32 oneShot = 0, repeat = 0;
        int  rate        = 0;
        juce::uint8 octaves     = 1;
        juce::uint8 compression = 0;
        std::vector<unsigned char> body;

        while (! input->isExhausted())
        {
            if (input->read (id, 4) != 4)
                break;
            const int sz = input->readIntBigEndian();
            if (sz < 0)
                break;
            const juce::int64 next = input->getPosition() + sz + (sz & 1); // Chunks sind gerade ausgerichtet

            if (memcmp (id, "VHDR", 4) == 0)
            {
                oneShot     = (juce::uint32) input->readIntBigEndian();
                repeat      = (juce::uint32) input->readIntBigEndian();
                input->readIntBigEndian(); // samplesPerHiCycle
                rate        = (int) (juce::uint16) input->readShortBigEndian();
                octaves     = (juce::uint8) input->readByte();
                compression = (juce::uint8) input->readByte();
            }
            else if (memcmp (id, "BODY", 4) == 0)
            {
                if (sz > 0)
                {
                    body.resize ((size_t) sz);
                    input->read (body.data(), sz);
                }
            }

            input->setPosition (next);
        }

        if (body.empty())
            return false;
        if (rate > 0)
            sampleRate = rate;

        std::vector<signed char> samples;
        if (compression == 1)
        {
            fibonacciDecode (body, samples);
        }
        else
        {
            samples.resize (body.size());
            for (size_t i = 0; i < body.size(); ++i)
                samples[i] = (signed char) body[i];
        }

        // Mehroktavige Samples: nur die erste (tiefste) Oktave verwenden.
        size_t count = samples.size();
        const size_t firstOctave = (size_t) oneShot + (size_t) repeat;
        if (octaves > 1 && firstOctave > 0 && firstOctave < count)
            count = firstOctave;

        pcm.resize (count);
        for (size_t i = 0; i < count; ++i)
            pcm[i] = (float) samples[i] / 128.0f;

        return count > 0;
    }
};

// Bindet den 8SVX-Reader in den AudioFormatManager ein.
class IFF8SVXAudioFormat : public juce::AudioFormat
{
public:
    IFF8SVXAudioFormat()
        : juce::AudioFormat ("IFF/8SVX (Amiga)", juce::StringArray { ".iff", ".8svx", ".svx" }) {}

    juce::Array<int> getPossibleSampleRates() override { return {}; }
    juce::Array<int> getPossibleBitDepths()   override { return { 8 }; }
    bool canDoStereo()  override { return false; }
    bool canDoMono()    override { return true; }
    bool isCompressed() override { return true; }

    juce::AudioFormatReader* createReaderFor (juce::InputStream* in,
                                              bool deleteStreamIfOpeningFails) override
    {
        std::unique_ptr<IFF8SVXReader> reader (new IFF8SVXReader (in));
        if (reader->isValid())
            return reader.release();

        // Kein gueltiges 8SVX (z. B. eine IFF-Bilddatei): nichts liefern.
        if (! deleteStreamIfOpeningFails)
            reader->releaseStreamOwnership(); // Aufrufer will den Stream behalten
        return nullptr;
    }

    juce::AudioFormatWriter* createWriterFor (juce::OutputStream*, double, unsigned int,
                                              int, const juce::StringPairArray&, int) override
    {
        return nullptr; // RetroTrax schreibt kein 8SVX
    }
};
