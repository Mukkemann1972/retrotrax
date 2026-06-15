#pragma once

#include "SID.h"          // reSIDfp-Motor (liegt in libs/residfp, per Include-Pfad erreichbar)
#include <juce_audio_basics/juce_audio_basics.h>
#include <cmath>
#include <algorithm>

// Reine Klang-Parameter einer SID-Stimme - entkoppelt SidChip vom TrackerEngine
// (sonst Zirkelbezug). Die Zahlencodes folgen der Reihenfolge der Instrument-Enums:
// wave 0=Dreieck 1=Saege 2=Puls 3=Rauschen, filter 0=aus 1=Tiefpass 2=Hochpass 3=Bandpass.
struct SidParams
{
    int   wave       = 2;
    float pulseWidth = 0.5f;
    float attack     = 0.004f, decay = 0.18f, sustain = 0.65f, release = 0.25f;
    int   filter     = 0;
    float cutoff     = 0.7f, resonance = 0.12f;
    bool  ringMod    = false, sync = false;
    float modTune    = 12.0f, pwmRate = 0.0f, pwmDepth = 0.0f;
};

// Kapselt EINEN echten reSIDfp-Chip und spielt damit genau eine Tracker-Stimme.
// Pro Spur eine eigene Instanz: so behalten wir die 8 Spuren (ein echter C64 haette
// nur 3 Stimmen). Die Melodie liegt auf SID-Stimme 1 (Index 0); fuer Ring/Sync nutzt
// sie SID-Stimme 3 (Index 2) als Modulator - das ist im SID die Quelle fuer Stimme 1.
//
// Wir uebersetzen die Klang-Parameter in die originalen SID-Register und takten den
// Chip in C64-Zyklen vorwaerts; reSIDfp resampelt selbst auf unsere Ausgaberate.
class SidChip
{
public:
    // PAL-C64-Systemtakt; bestimmt Tonhoehe und Klangcharakter des Filters.
    static constexpr double kClock = 985248.0;

    void prepare (double sampleRate)
    {
        sr = sampleRate;
        ready = false;
        try
        {
            sid.setChipModel (reSIDfp::MOS6581);            // der klassische, warme C64-Sound
            sid.setSamplingParameters (kClock, reSIDfp::RESAMPLE, sampleRate);
            sid.reset();
            cyclesPerSample = kClock / sampleRate;
            ready = true;
        }
        catch (const reSIDfp::SIDError&) { ready = false; }
        released = true;
        silentCount = 0;
        cycleRemainder = 0.0;
        pendingCount = pendingPos = 0;
    }

    // Neue Note: Register frisch setzen und Huellkurve ausloesen (Gate 0 -> 1 = Attack).
    void noteOn (const SidParams& p, double freqHz)
    {
        if (! ready) return;
        params  = p;
        pwBase  = juce::jlimit (0.02f, 0.98f, p.pulseWidth);
        pwmOn   = (p.wave == 2 && p.pwmDepth > 0.0f && p.pwmRate > 0.0f);
        pwmInc  = p.pwmRate / (float) sr;
        pwmPhase = 0.0;

        writeAdsr (p);
        writeFilter (p);
        setFrequency (freqHz);
        setPulseWidth (pwBase);

        // Gate sauber neu ausloesen, auch wenn die Stimme schon klang.
        sid.write (0 * 7 + 4, controlByte (false)); // Gate aus
        sid.write (0 * 7 + 4, controlByte (true));  // Gate an -> Attack
        if (p.ringMod || p.sync)
            sid.write (2 * 7 + 4, 0x10);            // Modulator-Stimme: Dreieck, kein Gate

        released = false;
        silentCount = 0;
        pendingCount = pendingPos = 0; // Rest der vorigen Note verwerfen
    }

    // Note loslassen: Gate aus -> die Huellkurve geht ins Release.
    void noteOff()
    {
        if (! ready) return;
        sid.write (0 * 7 + 4, controlByte (false));
        released = true;
    }

    // Einen Block rendern: aktuelle Tonhoehe (fuer Slides/Vibrato/Arpeggio) und ggf.
    // wabernde Pulsweite nachfuehren, Chip takten, Ergebnis mit Pegel/Panorama mischen.
    // Liefert false zurueck, wenn die Stimme nach dem Release verstummt ist.
    bool render (juce::AudioBuffer<float>& buffer, int offset, int num,
                 double freqHz, float gainL, float gainR, int outCh)
    {
        if (! ready) return false;
        setFrequency (freqHz);

        int done = 0;
        while (done < num)
        {
            const int chunk = std::min (num - done, kSub);

            if (pwmOn)
            {
                const float lfo = std::sin (pwmPhase * juce::MathConstants<double>::twoPi);
                setPulseWidth (juce::jlimit (0.05f, 0.95f, pwBase + params.pwmDepth * 0.45f * lfo));
                pwmPhase += pwmInc * (float) chunk;
                if (pwmPhase >= 1.0) pwmPhase -= 1.0;
            }

            produce (buffer, offset + done, chunk, gainL, gainR, outCh);
            done += chunk;
        }
        // Erst nach dem Loslassen abschalten - und nur, wenn der Chip wirklich
        // still geworden ist (Release ausgeklungen).
        return ! (released && silentCount > (int) sr / 8);
    }

private:
    static constexpr int kSub = 64; // Teilblock fuer fluessige Pulsweiten-Modulation

    // Ein einzelnes resampeltes SID-Sample in den Ausgabepuffer mischen.
    void emit (juce::AudioBuffer<float>& buffer, int idx, short raw,
               float gainL, float gainR, int outCh)
    {
        const float s = (float) raw * (kSidGain / 32768.0f);
        if (std::abs (s) > 0.0008f) silentCount = 0; else ++silentCount;
        for (int ch = 0; ch < outCh; ++ch)
        {
            const float g = (ch == 0 ? gainL : ch == 1 ? gainR : 0.5f * (gainL + gainR));
            buffer.addSample (ch, idx, s * g);
        }
    }

    // Erzeugt 'num' Ausgabe-Samples, indem der Chip die passende Anzahl C64-Zyklen
    // getaktet wird; reSIDfp gibt fertig resampelte 16-Bit-Werte aus. Ueberzaehlige
    // Samples eines clock()-Aufrufs bleiben fuer den naechsten Aufruf liegen, damit
    // weder etwas verloren geht noch die Tonhoehe driftet.
    void produce (juce::AudioBuffer<float>& buffer, int offset, int num,
                  float gainL, float gainR, int outCh)
    {
        int got = 0;
        // 1) Erst den Rest aus dem vorigen Aufruf abgeben (sonst ueberschreibt clock() ihn).
        while (pendingPos < pendingCount && got < num)
            emit (buffer, offset + got++, sampleBuf[pendingPos++], gainL, gainR, outCh);

        // 2) So lange takten, bis der Block voll ist.
        int guard = 0;
        while (got < num && guard++ < 128)
        {
            cycleRemainder += cyclesPerSample * (double) (num - got);
            unsigned int cyc = (unsigned int) cycleRemainder;
            if (cyc == 0) cyc = 1;
            cycleRemainder -= cyc;

            pendingCount = sid.clock (cyc, sampleBuf);
            pendingPos = 0;
            while (pendingPos < pendingCount && got < num)
                emit (buffer, offset + got++, sampleBuf[pendingPos++], gainL, gainR, outCh);
        }
    }

    unsigned char controlByte (bool gate) const
    {
        unsigned char b = gate ? 0x01 : 0x00;
        if (params.sync)    b |= 0x02;
        if (params.ringMod) b |= 0x04;
        switch (params.wave)
        {
            case 0:  b |= 0x10; break; // Dreieck
            case 1:  b |= 0x20; break; // Saege
            case 3:  b |= 0x80; break; // Rauschen
            case 2:
            default: b |= 0x40; break; // Puls
        }
        return b;
    }

    void setFrequency (double freqHz)
    {
        const auto reg = [] (double hz) -> int
        {
            const int fn = (int) std::lround (hz * 16777216.0 / kClock);
            return juce::jlimit (0, 65535, fn);
        };
        // Bei Sync klingt die Stimme hoeher, der Master haelt die Note; bei Ring
        // klingt die Note und der Modulator liegt um modTune daneben.
        const double r = std::pow (2.0, params.modTune / 12.0);
        const int fnMain = reg (params.sync ? freqHz * r : freqHz);
        sid.write (0, fnMain & 0xFF);
        sid.write (1, (fnMain >> 8) & 0xFF);
        if (params.ringMod || params.sync)
        {
            const int fnMod = reg (params.sync ? freqHz : freqHz * r);
            sid.write (2 * 7 + 0, fnMod & 0xFF);
            sid.write (2 * 7 + 1, (fnMod >> 8) & 0xFF);
        }
    }

    void setPulseWidth (float pw01)
    {
        const int pw = juce::jlimit (0, 4095, (int) std::lround (pw01 * 4095.0f));
        sid.write (2, pw & 0xFF);
        sid.write (3, (pw >> 8) & 0x0F);
    }

    void writeAdsr (const SidParams& p)
    {
        const int a  = nearestRate (kAttackMs, p.attack);
        const int d  = nearestRate (kDecRelMs, p.decay);
        const int rl = nearestRate (kDecRelMs, p.release);
        const int s  = juce::jlimit (0, 15, (int) std::lround (p.sustain * 15.0f));
        sid.write (5, (unsigned char) ((a << 4) | d));
        sid.write (6, (unsigned char) ((s << 4) | rl));
    }

    void writeFilter (const SidParams& p)
    {
        if (p.filter == 0) // aus
        {
            sid.write (23, 0x00);        // keine Stimme in den Filter
            sid.write (24, 0x0F);        // volle Lautstaerke, kein Filtermodus
            return;
        }
        const int fc = juce::jlimit (0, 2047, (int) std::lround (p.cutoff * 2047.0f));
        sid.write (21, fc & 0x07);
        sid.write (22, (fc >> 3) & 0xFF);
        const int res = juce::jlimit (0, 15, (int) std::lround (p.resonance * 15.0f));
        sid.write (23, (unsigned char) ((res << 4) | 0x01)); // Resonanz + Stimme 1 in den Filter
        unsigned char mode = 0x0F;                            // Lautstaerke voll
        mode |= (p.filter == 1) ? 0x10   // Tiefpass
              : (p.filter == 2) ? 0x40   // Hochpass
                                : 0x20;  // Bandpass
        sid.write (24, mode);
    }

    // Naechstgelegenen SID-Raten-Index (0..15) zu einer Zeit in Sekunden finden.
    static int nearestRate (const float* tableMs, float seconds)
    {
        const float ms = seconds * 1000.0f;
        int best = 0; float bestDiff = 1.0e9f;
        for (int i = 0; i < 16; ++i)
        {
            const float diff = std::abs (tableMs[i] - ms);
            if (diff < bestDiff) { bestDiff = diff; best = i; }
        }
        return best;
    }

    static constexpr float kSidGain = 0.7f; // Ausgabepegel, abgestimmt auf die Sample-Stimmen

    // Originale SID-Zeittabellen (Millisekunden) fuer Attack bzw. Decay/Release.
    static constexpr float kAttackMs[16] = {
        2, 8, 16, 24, 38, 56, 68, 80, 100, 250, 500, 800, 1000, 3000, 5000, 8000 };
    static constexpr float kDecRelMs[16] = {
        6, 24, 48, 72, 114, 168, 204, 240, 300, 750, 1500, 2400, 3000, 9000, 15000, 24000 };

    reSIDfp::SID sid;
    double sr = 44100.0;
    double cyclesPerSample = kClock / 44100.0;
    double cycleRemainder = 0.0;
    bool   ready = false;

    SidParams params;
    float  pwBase = 0.5f;
    bool   pwmOn = false;
    float  pwmInc = 0.0f;
    double pwmPhase = 0.0;

    bool   released = true;
    int    silentCount = 0;
    short  sampleBuf[2048];
    int    pendingCount = 0; // Anzahl Samples im letzten clock()-Aufruf
    int    pendingPos = 0;   // davon schon abgegeben
};
