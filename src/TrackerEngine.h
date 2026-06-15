#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include "SidChip.h"
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
    static constexpr int kNoteOff     = -2;  // "Note aus": laesst eine SID-Stimme ausklingen

    struct Cell
    {
        int note       = -1; // -1 = leer, -2 = Note-Aus, sonst 0..119 (Oktave * 12 + Halbton)
        int instrument = -1; // -1 = leer, sonst 0..15
        int volume     = -1; // -1 = voll, sonst 0..64
        int effect     = -1; // -1 = leer, sonst 0..15 (Effekt-Befehl, z.B. 0=Arpeggio, C=Lautstaerke, F=Tempo)
        int effectParam = 0; // 0..255 (zwei Hex-Stellen)
    };

    struct Instrument
    {
        // Ein Instrument ist entweder ein gesampelter Klang (Sample) oder ein
        // selbst erzeugter SID-Synth (Synth) - beide Arten leben friedlich in
        // denselben 16 Slots, frei mischbar in einem Song.
        enum class Kind { Sample, Synth };
        enum class Wave { Triangle, Saw, Pulse, Noise }; // klassische C64-Wellenformen
        enum class Filter { Off, LowPass, HighPass, BandPass }; // SID-Multimode-Filter
        // Welcher Klangmotor: der selbstgebaute (wie seit jeher) oder die echte
        // reSIDfp-Chip-Emulation. Pro Instrument frei waehlbar; Standard bleibt
        // Classic, damit bestehende Songs unveraendert klingen.
        enum class Engine { Classic, RealChip };

        Kind kind = Kind::Sample;

        // --- Sample-Klang ---
        juce::AudioBuffer<float> data;
        double sourceRate = 44100.0;
        juce::String name;
        juce::String filePath;

        // --- SID-Synth (nur bei kind == Synth) ---
        Engine engine     = Engine::Classic; // Klangmotor: selbstgebaut oder echter Chip
        Wave  wave        = Wave::Pulse;
        float pulseWidth  = 0.5f;   // 0..1, nur fuer die Puls-Welle
        float attack      = 0.004f; // Huellkurve in Sekunden / Sustain als Pegel 0..1
        float decay       = 0.18f;
        float sustain     = 0.65f;
        float release      = 0.25f;

        Filter filter     = Filter::Off; // Filtertyp (Off = ungefiltert)
        float  cutoff     = 0.7f;        // Grenzfrequenz 0..1 (exponentiell auf Hz abgebildet)
        float  resonance  = 0.12f;       // Resonanz/Betonung an der Grenzfrequenz 0..1

        // Zweiter Oszillator: Ring-Modulation (metallisch) und/oder Hard-Sync (schreiend).
        bool   ringMod    = false;
        bool   sync       = false;
        float  modTune    = 12.0f;       // Tonhoehe des zweiten Oszillators in Halbtoenen (rel. zur Note)

        // Pulsweiten-Modulation: ein langsamer LFO laesst die Pulsweite wabern.
        float  pwmRate    = 0.0f;        // LFO-Tempo in Hz (0 = aus)
        float  pwmDepth   = 0.0f;        // Tiefe 0..1 (nur bei der Puls-Welle wirksam)

        // Unisono-Stack: mehrere leicht verstimmte Stimmen pro Note -> fetter,
        // breiter Klang. Standard 1 (aus), damit bestehende Songs unveraendert klingen.
        int    unison     = 1;           // Anzahl gestapelter Stimmen 1..3
        float  detune     = 0.25f;       // Verstimmung 0..1 (0 = sauber, 1 = max ~25 Cent)
    };

    void prepare (double newSampleRate)
    {
        const juce::ScopedLock sl (lock);
        sampleRate = newSampleRate;
        samplesUntilTick = 0.0;
        for (auto& v : voices)
            v.active = false;
        for (auto& c : sidChips)
            c.prepare (newSampleRate); // echte reSIDfp-Chips an die Ausgaberate anpassen
    }

    // Klang-Parameter eines SID-Instruments in die schlanke SidChip-Struktur kippen.
    static SidParams toSidParams (const Instrument& inst)
    {
        SidParams p;
        p.wave       = (int) inst.wave;   // Enum-Reihenfolge passt 1:1 (Dreieck/Saege/Puls/Rauschen)
        p.pulseWidth = inst.pulseWidth;
        p.attack     = inst.attack;
        p.decay      = inst.decay;
        p.sustain    = inst.sustain;
        p.release    = inst.release;
        p.filter     = (int) inst.filter; // 0=aus 1=Tiefpass 2=Hochpass 3=Bandpass
        p.cutoff     = inst.cutoff;
        p.resonance  = inst.resonance;
        p.ringMod    = inst.ringMod;
        p.sync       = inst.sync;
        p.modTune    = inst.modTune;
        p.pwmRate    = inst.pwmRate;
        p.pwmDepth   = inst.pwmDepth;
        p.unison     = inst.unison;
        p.detune     = inst.detune;
        return p;
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
    // gateSamples > 0: Note nach dieser Dauer automatisch loslassen (Note-Aus) -
    // so demonstriert das SID-Fenster die ganze Huellkurve inkl. Ausklang.
    void audition (int note, int instrumentIdx, int gateSamples = -1)
    {
        const juce::ScopedLock sl (lock);
        triggerVoice (kTracks, note, instrumentIdx, -1);
        voices[kTracks].gate = gateSamples;
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
        int    fadeOut = 0;  // Note-Aus bei Sample-Stimmen: sanft ausblenden
        int    gate = -1;    // > 0: nach so vielen Samples automatisch Note-Aus (Vorhoeren)
        bool   active = false;
        // --- SID-Synth-Status ---
        int          envStage = 0;          // 0 leer, 1 Attack, 2 Decay, 3 Sustain, 4 Release
        float        envLevel = 0.0f;       // aktueller Huellkurven-Pegel 0..1
        float        noiseVal = 0.0f;       // gehaltener Rauschwert (-1..1)
        juce::uint32 noiseReg = 0x7FFFF8u;  // 23-Bit-Schieberegister wie im echten SID
        float        fic1 = 0.0f, fic2 = 0.0f; // Filter-Speicher (State-Variable-Filter)
        double       modPhase = 0.0;        // Phase des zweiten Oszillators (Ring/Sync)
        double       pwmPhase = 0.0;        // Phase des Pulsweiten-LFO
        double       uniPhase[2] = { 0.0, 0.0 }; // Phasen der gestapelten Unisono-Stimmen
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

    // Abspielschritt fuer eine Note. Beim Sample: Samples pro Ausgabe-Sample
    // (C-5 = Originaltonhoehe). Beim SID-Synth: Schwingungen pro Ausgabe-Sample
    // (Phasenzuwachs), C-5/Note 60 ergibt sich aus der A4=440-Stimmung.
    double stepForNote (const Instrument* inst, int note) const
    {
        note = juce::jlimit (0, kMaxNote, note);
        if (inst->kind == Instrument::Kind::Synth)
            return (440.0 * std::pow (2.0, (note - 69) / 12.0)) / sampleRate;
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

            if (c.note == kNoteOff)
                releaseVoice (v);
            else if (c.note >= 0 && ! tonePorta)
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
        const bool synth = (inst != nullptr && inst->kind == Instrument::Kind::Synth);
        if (inst == nullptr || (! synth && inst->data.getNumSamples() < 2))
        {
            v.active = false;
            return;
        }
        note = juce::jlimit (0, kMaxNote, note);
        v.inst        = inst;
        v.pos         = 0.0; // beim Synth = Oszillator-Phase 0..1
        v.note        = note;
        v.baseStep    = stepForNote (inst, note); // C-5 = Originaltonhoehe
        v.step        = v.baseStep;
        v.portaTarget = v.baseStep;
        v.vibPhase    = 0;
        v.voiceIdx    = voiceIdx;
        v.vol         = (volume >= 0 ? juce::jmin (volume, 64) : 64) / 64.0f;
        panGains (voiceIdx, v.vol, v.gainL, v.gainR);
        v.fadeOut = 0;
        v.gate    = -1; // nur das Vorhoeren setzt ein automatisches Note-Aus
        if (synth)
        {
            if (inst->engine == Instrument::Engine::RealChip)
            {
                // Echten reSIDfp-Chip dieser Stimme anwerfen; er loest die Attack-Phase
                // selbst aus (Gate-Flanke) -> kein Klick. step = Schwingungen/Sample,
                // also ist step * Abtastrate die Tonhoehe in Hz.
                sidChips[voiceIdx].noteOn (toSidParams (*inst), v.step * sampleRate);
            }
            else
            {
                // Klassisch: Huellkurve startet im Attack; sie blendet selbst ein.
                v.envStage = 1;
                v.envLevel = 0.0f;
                v.noiseReg = 0x7FFFF8u;
                v.fic1     = 0.0f;
                v.fic2     = 0.0f;
                v.modPhase = 0.0;
                v.pwmPhase = 0.0;
                v.uniPhase[0] = 0.33; // leicht versetzte Startphasen -> sofort breit
                v.uniPhase[1] = 0.66;
            }
            v.fadeIn = 0;
        }
        else
        {
            v.envStage = 0;
            v.fadeIn   = kFade;
        }
        v.active = true;
    }

    // Note-Aus: SID-Stimmen gehen ins Release, Sample-Stimmen blenden kurz aus.
    void releaseVoice (Voice& v)
    {
        if (! v.active || v.inst == nullptr)
            return;
        if (v.inst->kind == Instrument::Kind::Synth)
        {
            if (v.inst->engine == Instrument::Engine::RealChip)
                sidChips[v.voiceIdx].noteOff(); // Gate aus -> Chip-Huellkurve ins Release
            else
                v.envStage = 4;                 // Klassisch: Release-Phase
        }
        else if (v.fadeOut == 0)
            v.fadeOut = kFade;
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

            // Vorhoer-Note nach Ablauf der Gate-Zeit automatisch loslassen.
            if (v.gate > 0)
            {
                v.gate -= num;
                if (v.gate <= 0)
                {
                    v.gate = -1;
                    releaseVoice (v);
                }
            }

            if (v.inst->kind == Instrument::Kind::Synth)
            {
                if (v.inst->engine == Instrument::Engine::RealChip)
                    renderSynthChip (buffer, v, offset, num, outCh);
                else
                    renderSynthClassic (buffer, v, offset, num, outCh);
            }
            else
                renderSample (buffer, v, offset, num, outCh);
        }
    }

    void renderSample (juce::AudioBuffer<float>& buffer, Voice& v, int offset, int num, int outCh)
    {
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

            // Note-Aus: in kFade Samples sanft auf Null fahren, dann Stimme aus.
            bool finish = false;
            if (v.fadeOut > 0)
            {
                env *= (float) v.fadeOut / (float) kFade;
                if (--v.fadeOut == 0)
                    finish = true;
            }

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

            if (finish)
            {
                v.active = false;
                break;
            }
        }
        v.pos = pos;
    }

    // KLASSISCH (selbstgebaut): Oszillator (Dreieck/Saege/Puls/Rauschen) + ADSR-
    // Huellkurve + eigenes State-Variable-Filter. Der vertraute RetroTrax-Klang.
    void renderSynthClassic (juce::AudioBuffer<float>& buffer, Voice& v, int offset, int num, int outCh)
    {
        const auto* inst = v.inst;
        const float sr     = (float) sampleRate;
        const float atkInc = inst->attack  > 0.0f ? 1.0f / (inst->attack  * sr) : 1.0f;
        const float decInc = inst->decay   > 0.0f ? (1.0f - inst->sustain) / (inst->decay * sr) : 1.0f;
        const float relInc = inst->release > 0.0f ? 1.0f / (inst->release * sr) : 1.0f;
        const float pw     = juce::jlimit (0.02f, 0.98f, inst->pulseWidth);
        double ph = v.pos; // Phase 0..1

        // Zweiter Oszillator fuer Ring-Mod / Hard-Sync. Bei Sync ist der zweite
        // Oszillator der "Master" auf Notentonhoehe (haelt die Melodie sauber),
        // der hoerbare laeuft hoeher (modTune) und wird vom Master zurueckgesetzt.
        // Bei Ring laeuft der hoerbare auf Notentonhoehe, der zweite (modTune)
        // wird aufmultipliziert -> metallischer Klang.
        const bool  ring = inst->ringMod;
        const bool  sync = inst->sync;
        const double r   = std::pow (2.0, inst->modTune / 12.0);
        double mph        = v.modPhase;
        const double mainStep = sync ? v.step * r : v.step;
        const double modStep  = sync ? v.step     : v.step * r;

        // Pulsweiten-Modulation (nur Puls-Welle): LFO laesst die Pulsweite wabern.
        const bool   pwmOn  = (inst->wave == Instrument::Wave::Pulse
                                && inst->pwmDepth > 0.0f && inst->pwmRate > 0.0f);
        const float  pwmInc = inst->pwmRate / sr;
        double       pwmPh  = v.pwmPhase;

        // Unisono-Stack: bis zu 3 leicht verstimmte Stimmen fuer einen fetten,
        // breiten Klang. Bei Ring/Sync bleibt es einstimmig - der zweite Oszillator
        // ist dort schon belegt. Eine Stimme laeuft hoeher, eine tiefer (Cent-Spreizung).
        const int    uni      = (ring || sync) ? 1 : juce::jlimit (1, 3, inst->unison);
        const double detCents = juce::jlimit (0.0f, 1.0f, inst->detune) * 25.0;
        const double upRatio  = std::pow (2.0, detCents / 1200.0);
        const double uniStep0 = mainStep * upRatio; // 2. Stimme einen Tick hoeher
        const double uniStep1 = mainStep / upRatio; // 3. Stimme einen Tick tiefer
        double uph0 = v.uniPhase[0];
        double uph1 = v.uniPhase[1];
        const float  uniNorm  = 1.0f / std::sqrt ((float) uni); // Pegel halten

        // Eine Wellenform an einer beliebigen Phase auslesen (fuer die Stack-Stimmen).
        auto waveAt = [&] (double p, float pwc) -> float
        {
            switch (inst->wave)
            {
                case Instrument::Wave::Triangle: return p < 0.5 ? (float) (4.0 * p - 1.0) : (float) (3.0 - 4.0 * p);
                case Instrument::Wave::Saw:      return (float) (2.0 * p - 1.0);
                case Instrument::Wave::Noise:    return v.noiseVal;
                case Instrument::Wave::Pulse:
                default:                         return p < pwc ? 1.0f : -1.0f;
            }
        };

        // Filter-Koeffizienten einmal pro Block (TPT-State-Variable-Filter).
        const auto ftype = inst->filter;
        float fg = 0.0f, fk = 0.0f, fa1 = 0.0f, fa2 = 0.0f, fa3 = 0.0f;
        if (ftype != Instrument::Filter::Off)
        {
            // Cutoff exponentiell: 0 -> ~30 Hz, 1 -> ~11 kHz (gut musikalisch).
            float fc = 30.0f * std::pow (380.0f, juce::jlimit (0.0f, 1.0f, inst->cutoff));
            fc = juce::jlimit (20.0f, (float) (sampleRate * 0.45), fc);
            fg  = std::tan (juce::MathConstants<float>::pi * fc / sr);
            fk  = 2.0f - 1.9f * juce::jlimit (0.0f, 1.0f, inst->resonance); // 2 (zahm) .. 0.1 (klingelt)
            fa1 = 1.0f / (1.0f + fg * (fg + fk));
            fa2 = fg * fa1;
            fa3 = fg * fa2;
        }

        for (int i = 0; i < num; ++i)
        {
            // Huellkurve einen Schritt weiterfahren.
            switch (v.envStage)
            {
                case 1: v.envLevel += atkInc; if (v.envLevel >= 1.0f)          { v.envLevel = 1.0f;          v.envStage = 2; } break;
                case 2: v.envLevel -= decInc; if (v.envLevel <= inst->sustain) { v.envLevel = inst->sustain; v.envStage = 3; } break;
                case 3: break; // Sustain haelt, bis Note-Aus kommt
                case 4: v.envLevel -= relInc; if (v.envLevel <= 0.0f)          { v.envLevel = 0.0f;          v.active = false; } break;
                default: break;
            }

            // Aktuelle Pulsweite (ggf. vom LFO moduliert).
            float pwNow = pw;
            if (pwmOn)
            {
                const float lfo = std::sin (pwmPh * juce::MathConstants<double>::twoPi);
                pwNow = juce::jlimit (0.05f, 0.95f, pw + inst->pwmDepth * 0.45f * lfo);
                pwmPh += pwmInc;
                if (pwmPh >= 1.0)
                    pwmPh -= 1.0;
            }

            float osc = waveAt (ph, pwNow);

            // Ring-Modulation: hoerbare Welle mal Dreieck des zweiten Oszillators.
            if (ring)
            {
                const float modTri = mph < 0.5 ? (float) (4.0 * mph - 1.0) : (float) (3.0 - 4.0 * mph);
                osc *= modTri;
            }

            // Unisono-Stack dazumischen (verstimmte Kopien) und Pegel normieren.
            if (uni >= 2) osc += waveAt (uph0, pwNow);
            if (uni >= 3) osc += waveAt (uph1, pwNow);
            osc *= uniNorm;

            // Filter (TPT-SVF): erst die Wellenform filtern, dann die Huellkurve
            // formt die Lautstaerke - klassische Synth-Reihenfolge OSC -> Filter -> Pegel.
            if (ftype != Instrument::Filter::Off)
            {
                const float n3 = osc   - v.fic2;
                const float n1 = fa1 * v.fic1 + fa2 * n3;
                const float n2 = v.fic2 + fa2 * v.fic1 + fa3 * n3;
                v.fic1 = 2.0f * n1 - v.fic1;
                v.fic2 = 2.0f * n2 - v.fic2;
                osc = ftype == Instrument::Filter::LowPass  ? n2
                    : ftype == Instrument::Filter::HighPass ? (osc - fk * n1 - n2)
                                                            : n1; // BandPass
            }

            const float s = osc * v.envLevel;
            for (int ch = 0; ch < outCh; ++ch)
            {
                const float g = (ch == 0 ? v.gainL : ch == 1 ? v.gainR : 0.5f * (v.gainL + v.gainR));
                buffer.addSample (ch, offset + i, s * g * 0.42f);
            }

            ph += mainStep;
            if (ph >= 1.0)
            {
                ph -= 1.0;
                // Pro Schwingung ein neuer Rauschwert (23-Bit-LFSR wie im SID).
                const juce::uint32 bit = ((v.noiseReg >> 22) ^ (v.noiseReg >> 17)) & 1u;
                v.noiseReg = ((v.noiseReg << 1) | bit) & 0x7FFFFFu;
                v.noiseVal = (float) ((v.noiseReg >> 11) & 0xFFFu) / 2048.0f - 1.0f;
            }

            // Die gestapelten Unisono-Stimmen mit ihrer eigenen (verstimmten) Tonhoehe.
            if (uni >= 2) { uph0 += uniStep0; if (uph0 >= 1.0) uph0 -= 1.0; }
            if (uni >= 3) { uph1 += uniStep1; if (uph1 >= 1.0) uph1 -= 1.0; }

            // Zweiten Oszillator weiterdrehen; bei Hard-Sync setzt sein Ueberlauf
            // die hoerbare Phase auf 0 zurueck (das "Zerreissen").
            if (ring || sync)
            {
                mph += modStep;
                if (mph >= 1.0)
                {
                    mph -= 1.0;
                    if (sync)
                        ph = 0.0;
                }
            }

            if (! v.active)
                break;
        }
        v.pos = ph;
        v.modPhase = mph;
        v.pwmPhase = pwmPh;
        v.uniPhase[0] = uph0;
        v.uniPhase[1] = uph1;
    }

    // ECHTER CHIP: laeuft ueber die reSIDfp-Emulation dieser Stimme. Die Tonhoehe
    // kommt laufend aus v.step (von Slides/Vibrato/Arpeggio/Portamento veraendert)
    // - so wirken alle Effekte unveraendert weiter. Der Chip rendert, resampelt
    // selbst auf unsere Ausgaberate und meldet, wenn die Stimme verstummt ist.
    void renderSynthChip (juce::AudioBuffer<float>& buffer, Voice& v, int offset, int num, int outCh)
    {
        const double freqHz = v.step * sampleRate;
        if (! sidChips[v.voiceIdx].render (buffer, offset, num, freqHz, v.gainL, v.gainR, outCh))
            v.active = false;
    }

    Voice voices[kTracks + 1]; // +1 = Vorhoer-Stimme
    SidChip sidChips[kTracks + 1]; // ein echter reSIDfp-Chip pro Stimme (inkl. Vorhoeren)
    std::unique_ptr<Instrument> preview; // Sample der ST-Disks-Vorschau
    double sampleRate = 44100.0;
    double samplesUntilTick = 0.0;
    int    currentTick = 0; // 0 = Zeilenanfang, 1..speed-1 = Zwischenticks
};
