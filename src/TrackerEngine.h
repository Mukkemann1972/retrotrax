#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <atomic>
#include <cmath>
#include <memory>

// Der Kern des Trackers: Pattern-Daten, Instrumente und der Sequencer,
// der im Audio-Thread laeuft. Eine Stimme pro Spur, wie beim ProTracker.
class TrackerEngine
{
public:
    static constexpr int kTracks      = 8;
    static constexpr int kRows        = 64;
    static constexpr int kInstruments = 16;
    static constexpr int kMaxNote     = 119; // bis Oktave 9

    struct Cell
    {
        int note       = -1; // -1 = leer, sonst 0..119 (Oktave * 12 + Halbton)
        int instrument = -1; // -1 = leer, sonst 0..15
        int volume     = -1; // -1 = voll, sonst 0..64
    };

    struct Instrument
    {
        juce::AudioBuffer<float> data;
        double sourceRate = 44100.0;
        juce::String name;
        juce::String filePath;
    };

    void prepare (double newSampleRate)
    {
        const juce::ScopedLock sl (lock);
        sampleRate = newSampleRate;
        samplesUntilRow = 0.0;
        for (auto& v : voices)
            v.active = false;
    }

    void play()
    {
        const juce::ScopedLock sl (lock);
        currentRow = kRows - 1; // der naechste Schritt landet auf Zeile 0
        samplesUntilRow = 0.0;
        playing = true;
    }

    void stop()
    {
        const juce::ScopedLock sl (lock);
        playing = false;
        for (auto& v : voices)
            v.active = false;
    }

    // Instrument tauschen; Stimmen, die noch auf dem alten Sample spielen, werden gestoppt.
    void setInstrument (int slot, std::unique_ptr<Instrument> inst)
    {
        if (slot < 0 || slot >= kInstruments)
            return;
        const juce::ScopedLock sl (lock);
        for (auto& v : voices)
            if (v.inst == instruments[slot].get())
                v.active = false;
        instruments[slot] = std::move (inst);
    }

    juce::String getInstrumentName (int slot) const
    {
        if (slot < 0 || slot >= kInstruments)
            return {};
        const juce::ScopedLock sl (lock);
        return instruments[slot] != nullptr ? instruments[slot]->name : juce::String();
    }

    // Sofort anspielen (Vorhoeren beim Eintippen / MIDI-Eingang), eigene Extra-Stimme.
    void audition (int note, int instrumentIdx)
    {
        const juce::ScopedLock sl (lock);
        triggerVoice (kTracks, note, instrumentIdx, -1);
    }

    // Vorschau ohne Instrument-Slot (ST-Disks-Browser): das Sample gehoert
    // nur der Vorschau und ueberschreibt keinen der 16 Slots.
    void previewInstrument (std::unique_ptr<Instrument> inst)
    {
        const juce::ScopedLock sl (lock);
        for (auto& v : voices)
            if (v.inst == preview.get())
                v.active = false;
        preview = std::move (inst);
        if (preview != nullptr)
            startVoice (kTracks, 60, preview.get(), -1); // C-5 = Originaltonhoehe
    }

    void clearAllCells()
    {
        for (auto& row : cells)
            for (auto& c : row)
                c = Cell();
    }

    void process (juce::AudioBuffer<float>& buffer)
    {
        const juce::ScopedLock sl (lock);
        buffer.clear();

        int numSamples = buffer.getNumSamples();
        int offset = 0;

        while (numSamples > 0)
        {
            int chunk = numSamples;
            if (playing.load())
            {
                if (samplesUntilRow <= 0.5)
                {
                    advanceRow();
                    samplesUntilRow += samplesPerRow();
                }
                chunk = juce::jmin (numSamples, (int) std::ceil (samplesUntilRow));
                samplesUntilRow -= chunk;
            }
            render (buffer, offset, chunk);
            offset += chunk;
            numSamples -= chunk;
        }
    }

    // Zellen sind einfache ints; UI und Audio-Thread duerfen sie direkt lesen/schreiben.
    Cell cells[kRows][kTracks];
    std::unique_ptr<Instrument> instruments[kInstruments];

    std::atomic<float> bpm { 125.0f };
    std::atomic<bool>  playing { false };
    std::atomic<int>   currentRow { 0 };

    mutable juce::CriticalSection lock;

private:
    static constexpr int kFade = 96; // ~2 ms Ein-/Ausblende gegen Knacken

    struct Voice
    {
        const Instrument* inst = nullptr;
        double pos   = 0.0;
        double step  = 1.0;
        float  gainL = 0.5f; // linker/rechter Pegel (enthaelt schon Lautstaerke + Panorama)
        float  gainR = 0.5f;
        int    fadeIn = 0;   // verbleibende Samples der Anstiegsblende
        bool   active = false;
    };

    // Amiga-Stereo: Spuren abwechselnd leicht nach links/rechts (LRRL wie ProTracker);
    // die Vorhoer-/MIDI-Stimme (voiceIdx == kTracks) bleibt mittig. vol = 0..1.
    static void panGains (int voiceIdx, float vol, float& gL, float& gR)
    {
        float pan = 0.0f; // -1 = links, +1 = rechts
        if (voiceIdx >= 0 && voiceIdx < kTracks)
        {
            static const float pattern[4] = { -1.0f, 1.0f, 1.0f, -1.0f };
            pan = pattern[voiceIdx % 4] * 0.40f; // 40 % Breite, angenehm auch im Kopfhoerer
        }
        const float angle = (pan + 1.0f) * 0.25f * juce::MathConstants<float>::pi;
        gL = std::cos (angle) * vol; // gleiche Leistung links/rechts (Mitte = 0.707)
        gR = std::sin (angle) * vol;
    }

    double samplesPerRow() const
    {
        // 4 Zeilen pro Beat = 16tel-Noten, wie Speed 6 bei 125 BPM am Amiga
        return sampleRate * 60.0 / (juce::jmax (20.0f, bpm.load()) * 4.0);
    }

    void advanceRow()
    {
        const int row = (currentRow.load() + 1) % kRows;
        currentRow = row;
        for (int t = 0; t < kTracks; ++t)
        {
            const auto& c = cells[row][t];
            if (c.note >= 0)
                triggerVoice (t, c.note, c.instrument, c.volume);
        }
    }

    void triggerVoice (int voiceIdx, int note, int instrumentIdx, int volume)
    {
        const Instrument* inst = (instrumentIdx >= 0 && instrumentIdx < kInstruments)
                                     ? instruments[instrumentIdx].get() : nullptr;
        startVoice (voiceIdx, note, inst, volume);
    }

    void startVoice (int voiceIdx, int note, const Instrument* inst, int volume)
    {
        if (voiceIdx < 0 || voiceIdx > kTracks)
            return;
        auto& v = voices[voiceIdx];
        if (inst == nullptr || inst->data.getNumSamples() < 2)
        {
            v.active = false;
            return;
        }
        note = juce::jlimit (0, kMaxNote, note);
        v.inst = inst;
        v.pos  = 0.0;
        v.step = (inst->sourceRate / sampleRate) * std::pow (2.0, (note - 60) / 12.0); // Note C-5 = Originaltonhoehe
        const float vol = (volume >= 0 ? juce::jmin (volume, 64) : 64) / 64.0f;
        panGains (voiceIdx, vol, v.gainL, v.gainR);
        v.fadeIn = kFade;
        v.active = true;
    }

    void render (juce::AudioBuffer<float>& buffer, int offset, int num)
    {
        const int outCh = buffer.getNumChannels();
        for (auto& v : voices)
        {
            if (! v.active || v.inst == nullptr)
                continue;

            const auto& d  = v.inst->data;
            const int len  = d.getNumSamples();
            const int srcCh = d.getNumChannels();
            double pos = v.pos;

            for (int i = 0; i < num; ++i)
            {
                if (pos >= (double) (len - 1))
                {
                    v.active = false;
                    break;
                }
                const int   i0   = (int) pos;
                const float frac = (float) (pos - i0);

                // Huellkurve: am Notenanfang kurz einblenden, kurz vor Sample-Ende
                // wieder ausblenden - so knackt es weder beim Start noch beim Stopp.
                float env = 1.0f;
                if (v.fadeIn > 0)
                {
                    env = (float) (kFade - v.fadeIn) / (float) kFade;
                    --v.fadeIn;
                }
                const double remain = (double) (len - 1) - pos;
                if (remain < (double) kFade)
                    env *= (float) (remain / (double) kFade);

                for (int ch = 0; ch < outCh; ++ch)
                {
                    const float* src = d.getReadPointer (juce::jmin (ch, srcCh - 1));
                    const float  s   = src[i0] + (src[i0 + 1] - src[i0]) * frac;
                    const float  g   = (ch == 0 ? v.gainL
                                      : ch == 1 ? v.gainR
                                                : 0.5f * (v.gainL + v.gainR));
                    buffer.addSample (ch, offset + i, s * g * env * 0.5f);
                }
                pos += v.step;
            }
            v.pos = pos;
        }
    }

    Voice voices[kTracks + 1]; // +1 = Vorhoer-Stimme
    std::unique_ptr<Instrument> preview; // Sample der ST-Disks-Vorschau
    double sampleRate = 44100.0;
    double samplesUntilRow = 0.0;
};
