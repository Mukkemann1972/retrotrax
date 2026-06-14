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
    static constexpr int kMaxPatterns = 64;  // so viele eigene Patterns kann ein Song haben
    static constexpr int kMaxOrder    = 128; // so lang darf die Abspiel-Reihenfolge sein

    struct Cell
    {
        int note       = -1; // -1 = leer, sonst 0..119 (Oktave * 12 + Halbton)
        int instrument = -1; // -1 = leer, sonst 0..15
        int volume     = -1; // -1 = voll, sonst 0..64
        int effect     = -1; // -1 = leer, sonst 0..15 (Effekt-Befehl, z.B. 0=Arpeggio, C=Lautstaerke, F=Tempo)
        int effectParam = 0; // 0..255 (zwei Hex-Stellen)
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
        samplesUntilTick = 0.0;
        for (auto& v : voices)
            v.active = false;
    }

    void play()
    {
        const juce::ScopedLock sl (lock);
        currentRow = kRows - 1;     // der naechste Tick landet auf Zeile 0
        speed = 6;                  // klassische Vorgabe; Fxx im Pattern kann das aendern
        currentTick = speed.load() - 1; // ++ -> wickelt auf 0 und loest Zeile 0 aus
        samplesUntilTick = 0.0;
        // Song-Modus: vorn in der Reihenfolge starten. Loop-Modus: aktuelles Pattern.
        if (songMode.load())
        {
            songPos = 0;
            playPattern = juce::jlimit (0, kMaxPatterns - 1, order[0]);
        }
        else
        {
            playPattern = editPattern.load();
        }
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
        for (auto& pat : patterns)
            for (auto& row : pat)
                for (auto& c : row)
                    c = Cell();
        orderLen = 1;
        order[0] = 0;
        songPos = 0;
        playPattern = 0;
        setEditPattern (0);
    }

    // Welches Pattern der Editor zeigt/bearbeitet; "cells" zeigt darauf.
    void setEditPattern (int p)
    {
        p = juce::jlimit (0, kMaxPatterns - 1, p);
        editPattern = p;
        cells = patterns[p];
    }

    // Welches Pattern gerade im Grid gezeigt werden soll: im Song-Lauf das
    // klingende, sonst das bearbeitete.
    int displayPattern() const
    {
        return (playing.load() && songMode.load()) ? playPattern.load() : editPattern.load();
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
                if (samplesUntilTick <= 0.5)
                {
                    advanceTick();
                    samplesUntilTick += samplesPerTick();
                }
                chunk = juce::jmin (numSamples, (int) std::ceil (samplesUntilTick));
                samplesUntilTick -= chunk;
            }
            render (buffer, offset, chunk);
            offset += chunk;
            numSamples -= chunk;
        }
    }

    // Alle Patterns; Zellen sind einfache ints, UI und Audio-Thread lesen/schreiben direkt.
    Cell patterns[kMaxPatterns][kRows][kTracks] = {};
    // "cells" zeigt immer auf das gerade bearbeitete Pattern - so bleibt der
    // gesamte bestehende Editor-Code (engine.cells[r][t]) unveraendert.
    Cell (*cells)[kTracks] = patterns[0];
    std::unique_ptr<Instrument> instruments[kInstruments];

    std::atomic<float> bpm { 125.0f };
    std::atomic<int>   speed { 6 }; // Ticks pro Zeile (Fxx mit Param < 0x20)
    std::atomic<bool>  playing { false };
    std::atomic<int>   currentRow { 0 };

    // --- Song-Modus: mehrere Patterns in einer Reihenfolge abspielen ---
    std::atomic<int>  editPattern { 0 }; // welches Pattern der Editor zeigt
    std::atomic<int>  playPattern { 0 }; // welches Pattern gerade klingt
    std::atomic<int>  songPos { 0 };     // Position in der Reihenfolge
    std::atomic<bool> songMode { false };// true = Reihenfolge abspielen, false = aktuelles Pattern loopen
    int order[kMaxOrder] = { 0 };        // Abspiel-Reihenfolge (Pattern-Indizes)
    int orderLen = 1;

    mutable juce::CriticalSection lock;

private:
    static constexpr int kFade = 96; // ~2 ms Ein-/Ausblende gegen Knacken

    struct Voice
    {
        const Instrument* inst = nullptr;
        double pos   = 0.0;
        double step  = 1.0;  // aktueller Abspielschritt (jede Tick neu gesetzt)
        double baseStep = 1.0;   // Grund-Tonhoehe (von Slides/Portamento dauerhaft veraendert)
        double portaTarget = 1.0; // Ziel-Step fuer Tone-Portamento (3xx)
        float  gainL = 0.5f; // linker/rechter Pegel (enthaelt schon Lautstaerke + Panorama)
        float  gainR = 0.5f;
        float  vol   = 1.0f; // 0..1 aktuelle Lautstaerke (von Cxx/Axy)
        int    fadeIn = 0;   // verbleibende Samples der Anstiegsblende
        bool   active = false;
        // --- Effekt-Status der laufenden Zeile ---
        int    effect = -1;
        int    effectParam = 0;
        int    voiceIdx = 0; // fuer Panorama beim Lautstaerke-Update
        int    note = 60;    // aktuelle Note (Basis fuer Arpeggio)
        int    vibPhase = 0; // Vibrato-Phasenzaehler (0..63)
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

    // Tickdauer haengt nur am Tempo (klassisch). Bei Speed 6 ergeben 6 Ticks
    // genau eine Zeile wie frueher; ein anderer Speed macht Zeilen schneller/langsamer.
    double samplesPerTick() const
    {
        return sampleRate * 2.5 / juce::jmax (20.0f, bpm.load());
    }

    // Abspielschritt (Rate) fuer eine Note; C-5 (60) = Originaltonhoehe.
    double stepForNote (const Instrument* inst, int note) const
    {
        note = juce::jlimit (0, kMaxNote, note);
        return (inst->sourceRate / sampleRate) * std::pow (2.0, (note - 60) / 12.0);
    }

    // Ein Tick weiter: Tick 0 = neue Zeile (Noten ausloesen + Effekte einrichten),
    // Ticks 1..speed-1 = laufende Effekte nachfuehren (Arpeggio, Slides, Vibrato ...).
    void advanceTick()
    {
        if (++currentTick >= speed.load())
        {
            currentTick = 0;
            advanceRow();
        }
        applyTickEffects (currentTick);
    }

    void advanceRow()
    {
        int row = currentRow.load() + 1;
        if (row >= kRows)
        {
            row = 0;
            // Pattern zu Ende: im Song-Modus zum naechsten Eintrag der Reihenfolge.
            if (songMode.load())
            {
                int sp = songPos.load() + 1;
                if (sp >= orderLen) sp = 0; // Song laeuft in Schleife
                songPos = sp;
                playPattern = juce::jlimit (0, kMaxPatterns - 1, order[sp]);
            }
        }
        currentRow = row;

        const int pat = playPattern.load();
        for (int t = 0; t < kTracks; ++t)
        {
            const auto& c = patterns[pat][row][t];
            auto& v = voices[t];

            // Effekt dieser Zeile in die Stimme uebernehmen.
            v.effect      = c.effect;
            v.effectParam = c.effectParam;

            // Tone-Portamento (3xx) schlaegt die Note NICHT neu an, sondern gleitet hin.
            const bool tonePorta = (c.effect == 0x3);

            if (c.note >= 0 && ! tonePorta)
                triggerVoice (t, c.note, c.instrument, c.volume);

            applyRowEffect (t, c); // Cxx/Fxx/3xx-Ziel: einmal pro Zeile (Tick 0)
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
        v.inst        = inst;
        v.pos         = 0.0;
        v.note        = note;
        v.baseStep    = stepForNote (inst, note); // C-5 = Originaltonhoehe
        v.step        = v.baseStep;
        v.portaTarget = v.baseStep;
        v.vibPhase    = 0;
        v.voiceIdx    = voiceIdx;
        v.vol         = (volume >= 0 ? juce::jmin (volume, 64) : 64) / 64.0f;
        panGains (voiceIdx, v.vol, v.gainL, v.gainR);
        v.fadeIn = kFade;
        v.active = true;
    }

    // Lautstaerke (0..64) einer Stimme setzen und Pegel inkl. Panorama neu berechnen.
    void setVoiceVolume (Voice& v, int vol0to64)
    {
        v.vol = juce::jlimit (0, 64, vol0to64) / 64.0f;
        panGains (v.voiceIdx, v.vol, v.gainL, v.gainR);
    }

    // Einmal-pro-Zeile-Effekte (am Tick 0 ausgewertet).
    void applyRowEffect (int t, const Cell& c)
    {
        auto& v = voices[t];
        switch (c.effect)
        {
            case 0xC: // Lautstaerke setzen (00..40 hex = 0..64)
                if (v.active)
                    setVoiceVolume (v, c.effectParam);
                break;

            case 0xF: // Speed / Tempo: < 0x20 -> Ticks pro Zeile, sonst BPM
                if (c.effectParam > 0 && c.effectParam < 0x20)
                    speed = c.effectParam;
                else if (c.effectParam >= 0x20)
                    bpm = (float) c.effectParam;
                break;

            case 0x3: // Tone-Portamento: Ziel ist die Note dieser Zeile
                if (c.note >= 0 && v.inst != nullptr)
                    v.portaTarget = stepForNote (v.inst, c.note);
                break;

            default: break;
        }
    }

    // Pro-Tick-Effekte (Arpeggio, Slides, Portamento, Vibrato, Lautstaerke-Slide).
    void applyTickEffects (int tick)
    {
        for (int t = 0; t < kTracks; ++t)
        {
            auto& v = voices[t];
            if (! v.active || v.inst == nullptr)
                continue;

            const int p  = v.effectParam;
            const int px = (p >> 4) & 0xF; // obere Hex-Stelle
            const int py =  p       & 0xF; // untere Hex-Stelle

            // Effekte, die die Grund-Tonhoehe / Lautstaerke dauerhaft veraendern
            // (nur an den Zwischenticks, nicht am Zeilenanfang).
            if (tick > 0)
            {
                if (v.effect == 0x1)        // Slide hoch
                    v.baseStep *= std::pow (2.0, (p / 64.0) / 12.0);
                else if (v.effect == 0x2)   // Slide runter
                    v.baseStep *= std::pow (2.0, -(p / 64.0) / 12.0);
                else if (v.effect == 0x3)   // Tone-Portamento Richtung Ziel
                {
                    const double r = std::pow (2.0, (p / 64.0) / 12.0);
                    if (v.baseStep < v.portaTarget) v.baseStep = juce::jmin (v.portaTarget, v.baseStep * r);
                    else                            v.baseStep = juce::jmax (v.portaTarget, v.baseStep / r);
                }
                else if (v.effect == 0xA)   // Lautstaerke-Slide: x hoch, y runter
                    setVoiceVolume (v, (int) std::lround (v.vol * 64.0) + px - py);
            }

            // Step jede Tick frisch aus baseStep; Arpeggio/Vibrato legen einen
            // voruebergehenden Tonhoehen-Versatz obendrauf.
            v.step = v.baseStep;

            if (v.effect == 0x0 && p != 0) // Arpeggio: Grundton, +x, +y im Wechsel
            {
                const int sel = tick % 3;
                const int off = sel == 0 ? 0 : (sel == 1 ? px : py);
                v.step = stepForNote (v.inst, v.note + off);
            }
            else if (v.effect == 0x4) // Vibrato: x = Tempo, y = Tiefe
            {
                v.vibPhase = (v.vibPhase + px) & 0x3F;
                const double s = std::sin (v.vibPhase * juce::MathConstants<double>::twoPi / 64.0);
                v.step = v.baseStep * std::pow (2.0, (s * (py / 16.0)) / 12.0);
            }
        }
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
    double samplesUntilTick = 0.0;
    int    currentTick = 0; // 0 = Zeilenanfang, 1..speed-1 = Zwischenticks
};
