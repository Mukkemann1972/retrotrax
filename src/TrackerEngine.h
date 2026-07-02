#pragma once

#ifdef RETROTRAX_NO_JUCE
  #include "rt_juce_shim.h"   // schlanker Replayer-Build: JUCE-Ersatz
#else
  #include <juce_audio_basics/juce_audio_basics.h>
#endif
#include "SidChip.h"
#include <atomic>
#include <cmath>
#include <memory>

// Der Kern des Trackers: Pattern-Daten, Instrumente und der Sequencer,
// der im Audio-Thread laeuft. Eine Stimme pro Spur, wie beim ProTracker.
class TrackerEngine
{
public:
    static constexpr int kTracks      = 16;
    static constexpr int kRows        = 64;
    static constexpr int kInstruments = 31;  // 31 wie im Amiga-MOD (frei mischbar Sample/SID)
    static constexpr int kPads        = 16;  // Drum-Kit: 4x4 Pads (MPC60/SP-1200-Stil), eigene Samples
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

        // --- Akai-Sampler-Filter (nur bei kind == Sample) ---
        // Resonanter Tiefpass im Stil der Akai S900/S950/S1000 (2 kaskadierte
        // SVF-Stufen = 24 dB/Okt, waermer/steiler als der SID-Filter) plus ein
        // optionaler 12-Bit-Crunch (der lo-fi-Charakter der alten 12-Bit-Sampler).
        // Standard AUS bzw. ganz offen -> bestehende Songs klingen unveraendert.
        bool  akaiOn        = false;
        float akaiCutoff    = 1.0f;   // 0..1 (1 = ganz offen, praktisch unhoerbar)
        float akaiResonance = 0.12f;  // 0..1 (0 zahm .. 1 klingelt)
        bool  akai12bit     = false;  // 12-Bit-Quantisierung (Crunch)

        // --- 8-Bit (Mirage / Fairlight I / Ensoniq) ---
        // Noch groebere Quantisierung als 12-Bit: roh, koernig, kristallin-metallisch
        // wie die fruehen 8-Bit-Sampler. Geht VOR 12-Bit (wenn beide an, gewinnt 8).
        // Standard AUS -> bestehende Songs klingen unveraendert.
        bool  akai8bit      = false;

        // --- Companding (EMU II) ---
        // Der Emulator II quantisierte nicht linear, sondern KOMPANDIERT: beim
        // Aufnehmen mit einer mu-law-Kennlinie gestaucht, beim Abspielen wieder
        // gespreizt. Leise Anteile bekommen so mehr Aufloesung, laute saettigen
        // weich -> der typische "warm + druckvoll"-Charakter (Depeche Mode/Vangelis).
        // Wirkt rund um die 12-Bit-Stufe; ohne 12-Bit allein als sanfte mu-law-
        // Faerbung. Standard AUS -> bestehende Songs klingen unveraendert.
        bool  companding    = false;

        // --- Sampler-Effekte (nur bei kind == Sample) ---
        // Reverse spielt das Sample rueckwaerts; SR-Reduktion (Decimator) haelt
        // jeden Quellwert ueber mehrere Ausgabe-Samples -> koerniger lo-fi-Klang
        // mit Aliasing, wie ein heruntergetakteter alter Sampler. Beide Standard
        // AUS -> bestehende Songs klingen unveraendert.
        bool  reverse       = false;
        float srReduction   = 0.0f;   // 0 = aus .. 1 = extrem grob

        // Loop: das Sample laeuft in der Schleife weiter, solange die Note klingt.
        // Off = einmal abspielen (wie bisher), Forward = vorne wieder anfangen,
        // PingPong = vor und zurueck (knackfrei, weil am Rand gespiegelt).
        enum class Loop { Off, Forward, PingPong };
        Loop  loopMode      = Loop::Off;

        // Loop-Crossfade: blendet beim Vorwaerts-Loop das Schleifen-Ende sanft in
        // den Anfang ueber, statt hart umzuspringen -> kurze Samples loopen smooth
        // statt abgehackt (Fairlight-Gefuehl). 0 = harter Sprung (wie bisher),
        // 1 = lange Ueberblendung (bis halbe Sample-Laenge). Nur bei Loop::Forward.
        // Standard 0 -> bestehende Songs klingen unveraendert.
        float loopXfade     = 0.0f;

        // Loop-Punkt: Bruchteil 0..1, ab dem die Vorwaerts-Schleife wieder einsetzt
        // (statt immer ab 0). >0 schaltet den Crossfade aus (eigener Punkt). Std 0.
        float loopStart     = 0.0f;

        // --- Analoge Waerme (nur bei kind == Sample) ---
        // Drive: weiche tanh-Saettigung wie die analogen Filter/Wandler alter
        // Sampler - hohe Resonanz/heisse Signale saettigen musikalisch statt
        // digital zu clippen, bringt Druck + natuerliche Kompression (Punch).
        // VintagePitch: ohne Interpolation lesen (rohe Wandlung) -> crunchy,
        // aliasing-reicher Klang beim Pitchen wie bei Fairlight/Emulator.
        // Beide Standard AUS -> bestehende Songs klingen unveraendert.
        float drive         = 0.0f;   // 0 = clean .. 1 = stark gesaettigt
        bool  vintagePitch  = false;

        // Stimmung in Halbtoenen (fein, darf gebrochen sein) - verschiebt die
        // Tonhoehe relativ zur Note. Vor allem fuer Drum-Pads (jedes Pad eigene
        // Stimmung wie SP-1200/MPC), wirkt aber auf jedes Instrument. Standard 0.
        float tuneSemis     = 0.0f;

        // Tape-Wow/Flutter (Mellotron): langsames Band-Eiern (~0.7 Hz) + leichtes
        // Flattern (~8 Hz) moduliert die Tonhoehe. Jede Note eiert mit eigener Phase
        // (wie ein eigener Bandstreifen pro Taste beim Mellotron). 0 = aus.
        float tapeWow       = 0.0f;

        // Sampler-Huellkurve (ADSR) + Lautstaerke: wie an einem echten Sampler den
        // Klang formen. ampEnv=false -> Sample spielt unveraendert (nur Anti-Knack).
        // Bei ampEnv nutzt das Sample die unten stehenden attack/decay/sustain/release.
        bool  ampEnv        = false;
        float gain          = 1.0f;   // Lautstaerke 0..2 (1 = unveraendert)

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

        // Akkord aus EINER Note: die Stapel-Stimmen liegen dann auf festen
        // Halbton-Abstaenden (Dreiklang u.ae.) statt nur leicht verstimmt -> aus
        // einem einzigen Tastendruck klingt ein ganzer Akkord. 0 = Aus (dann gilt
        // der Unisono-Stack). Nutzt dieselben Stapel-Stimmen wie Unisono.
        int    chord      = 0;           // 0 Aus, 1 Dur, 2 Moll, 3 Sus4, 4 Quinte, 5 Oktave
        static constexpr int kNumChords = 6; // inkl. "Aus"

        // Der gemeinsame Akkord-"Bauplan": Halbton-Abstaende der zusaetzlichen
        // Stapel-Stimmen. Rueckgabe = Anzahl Extra-Stimmen (0..2), semi[] gefuellt.
        // Eine einzige Quelle, damit Classic- und Echter-Chip-Motor identisch bauen.
        static int chordSemis (int chord, float semi[2])
        {
            switch (chord)
            {
                case 1: semi[0] =  4.0f; semi[1] = 7.0f; return 2; // Dur (gross)
                case 2: semi[0] =  3.0f; semi[1] = 7.0f; return 2; // Moll (klein)
                case 3: semi[0] =  5.0f; semi[1] = 7.0f; return 2; // Sus4 (Vorhalt)
                case 4: semi[0] =  7.0f; semi[1] = 0.0f; return 1; // Quinte (Powerchord)
                case 5: semi[0] = 12.0f; semi[1] = 0.0f; return 1; // Oktave
                default: semi[0] = 0.0f; semi[1] = 0.0f; return 0; // Aus
            }
        }
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
        // Akkord-Bauplan mitgeben (Anzahl Zusatzstimmen + Halbton-Abstaende).
        float chSemi[2];
        p.chordVoices = Instrument::chordSemis (inst.chord, chSemi);
        p.chordSemi[0] = chSemi[0];
        p.chordSemi[1] = chSemi[1];
        return p;
    }

    void play()
    {
        const juce::ScopedLock sl (lock);
        currentRow = kRows - 1;     // der naechste Tick landet auf Zeile 0
        speed = 6;                  // klassische Vorgabe; Fxx im Pattern kann das aendern
        currentTick = speed.load() - 1; // ++ -> wickelt auf 0 und loest Zeile 0 aus
        samplesUntilTick = 0.0;
        songLoopCount = 0; // neuer Durchlauf beginnt (fuer den Offline-Export)
        pendingBreakRow = -1; // Pattern-Break/Position-Jump zuruecksetzen
        pendingPosJump  = -1;
        // Song-Modus: vorn in der Reihenfolge starten. Loop-Modus: aktuelles Pattern.
        if (songMode.load())
        {
            songPos = 0;
            playPattern = juce::jlimit (0, kMaxPatterns - 1, order[0]);
            songStartPending = true; // order[0] beim ersten Umlauf nicht ueberspringen
        }
        else
        {
            playPattern = editPattern.load();
            songStartPending = false;
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

    // MIDI-Aftertouch/Druck: moduliert die Lautstaerke der live gespielten
    // (Vorhoer-)Note 0..127 -> 50..100 %. Wirkt nur mit einem Controller, der
    // Aftertouch sendet; ohne Druck bleibt die Note bei vollem Pegel.
    void midiPressure (float p)
    {
        const juce::ScopedLock sl (lock);
        auto& v = voices[kTracks];
        if (v.active)
            setVoiceVolume (v, (int) std::lround (juce::jlimit (0.0f, 1.0f, 0.5f + 0.5f * p) * 64.0f));
    }

    // Vorschau ohne Instrument-Slot (ST-Disks-Browser): das Sample gehoert
    // nur der Vorschau und ueberschreibt keinen der 16 Slots.
    void previewInstrument (std::unique_ptr<Instrument> inst, int note = 60)
    {
        const juce::ScopedLock sl (lock);
        for (auto& v : voices)
            if (v.inst == preview.get())
                v.active = false;
        preview = std::move (inst);
        if (preview != nullptr)
            startVoice (kTracks, note, preview.get(), -1); // 60 = C-5 (Originaltonhoehe)
    }

    // Aktuelle Abspielposition der Vorschau-Stimme in Samples (-1 = spielt nicht).
    // Fuer die Laufmarke im Fairlight-Werkzeug.
    double previewPos() const
    {
        const juce::ScopedLock sl (lock);
        const auto& v = voices[kTracks];
        return (v.active && preview != nullptr && v.inst == preview.get()) ? v.pos : -1.0;
    }

    // --- Drum-Kit (16 Pads, MPC60/SP-1200-Stil) -----------------------------
    // Ein eigenstaendiges 16er-Sample-Bank NEBEN den Spur-Slots: zum Finger-
    // Trommeln/Vorhoeren. Jedes Pad ist ein ganz normales Instrument (Sample),
    // also frei zwischen Pad/Slot/Grabber/Editor schiebbar. Die Pads gehoeren der
    // Engine (wie 'preview'), damit der Audio-Thread sicher darauf zugreift.
    void setPad (int pad, std::unique_ptr<Instrument> inst)
    {
        const juce::ScopedLock sl (lock);
        if (pad < 0 || pad >= kPads)
            return;
        for (auto& v : voices)               // alte Stimme auf diesem Pad stoppen
            if (v.inst == kitPads[pad].get())
                v.active = false;
        kitPads[pad] = std::move (inst);
    }

    void clearPad (int pad)
    {
        setPad (pad, nullptr);
    }

    bool padHasSound (int pad) const
    {
        const juce::ScopedLock sl (lock);
        return pad >= 0 && pad < kPads && kitPads[pad] != nullptr;
    }

    // Lese-Zugriff auf ein Pad (Speichern/Kopieren). Aufrufer sollte engine.lock
    // halten, wenn parallel setPad laufen kann (JUCE-CriticalSection ist rekursiv).
    const Instrument* getPad (int pad) const
    {
        return (pad >= 0 && pad < kPads) ? kitPads[pad].get() : nullptr;
    }

    // Pad anschlagen (Vorhoer-/MIDI-Stimme, Index kTracks) - blockiert keinen Slot.
    // volume 0..64 = Anschlagdynamik (Velocity); -1 = volle Lautstaerke.
    void auditionPad (int pad, int note = 60, int gateSamples = -1, int volume = -1)
    {
        const juce::ScopedLock sl (lock);
        if (pad < 0 || pad >= kPads || kitPads[pad] == nullptr)
            return;
        startVoice (kTracks, note, kitPads[pad].get(), volume);
        voices[kTracks].gate = gateSamples;
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
        // Solo/Mute sind Arbeitszustand - bei neuem Inhalt (MOD/XM/TFMX/neues Lied)
        // alle Spuren wieder hoerbar machen, damit nichts ungewollt stumm bleibt.
        for (int t = 0; t < kTracks; ++t)
        {
            trackMute[t] = false;
            trackSolo[t] = false;
        }
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
        for (auto& tl : trackLevel) tl.store (0.0f, std::memory_order_relaxed); // VU-Block-Peak

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
                    samplesUntilTick += samplesPerTick() * swingFactor();
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
    // Swing/Groove (das MPC-Geheimnis): verlaengert paarweise die geraden Zeilen
    // und verkuerzt die ungeraden -> Shuffle-Feel; das Tempo bleibt gleich.
    // 0 = gerade (aus), bis ~0.8 = starker Shuffle.
    std::atomic<float> swing { 0.0f };
    std::atomic<bool>  playing { false };
    std::atomic<bool>  recording { false }; // REC scharf: nur dann landen Live-Noten im Pattern
    std::atomic<int>   currentRow { 0 };
    std::atomic<float> trackLevel[kTracks] = {}; // VU-Spitzenpegel pro Spur (Anzeige)

    // --- Song-Modus: mehrere Patterns in einer Reihenfolge abspielen ---
    std::atomic<int>  editPattern { 0 }; // welches Pattern der Editor zeigt
    std::atomic<int>  playPattern { 0 }; // welches Pattern gerade klingt
    std::atomic<int>  songPos { 0 };     // Position in der Reihenfolge
    std::atomic<bool> songMode { false };// true = Reihenfolge abspielen, false = aktuelles Pattern loopen
    int order[kMaxOrder] = { 0 };        // Abspiel-Reihenfolge (Pattern-Indizes)
    int orderLen = 1;
    // Zaehlt, wie oft die Reihenfolge komplett umgelaufen ist. Der Offline-WAV-
    // Export rendert, bis dieser Wert von 0 auf 1 springt = ein voller Durchlauf.
    std::atomic<int> songLoopCount { 0 };
    // Pattern-Break (Dxx) / Position-Jump (Bxx) aus der gerade gespielten Zeile,
    // wirken beim naechsten Zeilenwechsel (originalgetreue Modul-Wiedergabe).
    int pendingBreakRow = -1; // Dxx: Zielzeile (-1 = keiner)
    int pendingPosJump  = -1; // Bxx: Ziel-Position in der Reihenfolge (-1 = keiner)
    // True direkt nach play() im Song-Modus: der allererste Zeilen-Umlauf bleibt
    // bei order[0], statt songPos schon 0->1 hochzuzaehlen (sonst wird das erste
    // Pattern der Reihenfolge uebersprungen).
    bool songStartPending = false;

    // --- Solo / Mute pro Spur (wie in jeder DAW und jedem alten Tracker) ------
    // Reine Wiedergabe-Schalter: der Sequencer laeuft weiter, nur der Ton einer
    // stummen bzw. nicht ge-soloten Spur wird beim Rendern uebersprungen. Standard
    // alle hoerbar; Arbeitszustand, NICHT im Song gespeichert. So findet man, wo
    // im Mix etwas nicht passt.
    std::atomic<bool> trackMute[kTracks] = {};
    std::atomic<bool> trackSolo[kTracks] = {};

    // Ist irgendwo Solo aktiv? Dann sind nur die ge-soloten Spuren hoerbar.
    bool anySolo() const
    {
        for (int t = 0; t < kTracks; ++t)
            if (trackSolo[t].load())
                return true;
        return false;
    }

    // Ist die Spur gerade hoerbar? Solo (irgendwo aktiv) schlaegt Mute; die
    // Vorhoer-/MIDI-Stimme (Index kTracks) bleibt immer hoerbar.
    bool trackAudible (int t) const
    {
        if (t < 0 || t >= kTracks)
            return true;
        if (anySolo())
            return trackSolo[t].load();
        return ! trackMute[t].load();
    }

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
        float        akaiLp[2][4] = {};        // Akai-Filter-Speicher: [Kanal][2 Stufen x c1,c2]
        float        srHold[2] = { 0.0f, 0.0f }; // Sample-and-Hold-Wert je Kanal (SR-Reduktion)
        int          srCount = 0;               // verbleibende Halte-Samples
        int          loopDir = 1;               // Laufrichtung beim Ping-Pong-Loop (+1/-1)
        double       modPhase = 0.0;        // Phase des zweiten Oszillators (Ring/Sync)
        double       pwmPhase = 0.0;        // Phase des Pulsweiten-LFO
        double       uniPhase[2] = { 0.0, 0.0 }; // Phasen der gestapelten Unisono-Stimmen
        double       wowPhase  = 0.0;       // Phase der Tape-Wow-LFO (Mellotron-Eiern)
        double       flutPhase = 0.0;       // Phase des schnelleren Flutter-Anteils
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

    // Swing-Faktor fuer die GERADE klingende Zeile: paarweise laenger/kuerzer,
    // Summe konstant (Tempo bleibt). Wird auf die Tick-Laenge multipliziert.
    double swingFactor() const
    {
        const float s = juce::jlimit (0.0f, 0.8f, swing.load());
        if (s <= 0.0f)
            return 1.0;
        return (currentRow.load() % 2 == 0) ? (1.0 + 0.5 * (double) s)
                                            : (1.0 - 0.5 * (double) s);
    }

    // Abspielschritt fuer eine Note. Beim Sample: Samples pro Ausgabe-Sample
    // (C-5 = Originaltonhoehe). Beim SID-Synth: Schwingungen pro Ausgabe-Sample
    // (Phasenzuwachs), C-5/Note 60 ergibt sich aus der A4=440-Stimmung.
    double stepForNote (const Instrument* inst, int note) const
    {
        note = juce::jlimit (0, kMaxNote, note);
        const double tune = (double) inst->tuneSemis; // Feinstimmung (Halbtoene)
        if (inst->kind == Instrument::Kind::Synth)
            return (440.0 * std::pow (2.0, (note - 69 + tune) / 12.0)) / sampleRate;
        return (inst->sourceRate / sampleRate) * std::pow (2.0, (note - 60 + tune) / 12.0);
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
        int row;
        if (pendingPosJump >= 0 || pendingBreakRow >= 0)
        {
            // Position-Jump (Bxx) / Pattern-Break (Dxx) der vorherigen Zeile.
            if (songMode.load())
            {
                int sp = (pendingPosJump >= 0) ? pendingPosJump   // Bxx: zu dieser Position
                                               : songPos.load() + 1; // Dxx: naechste Position
                if (sp >= orderLen) { sp = 0; songLoopCount.fetch_add (1, std::memory_order_relaxed); }
                sp = juce::jlimit (0, juce::jmax (0, orderLen - 1), sp);
                songPos = sp;
                playPattern = juce::jlimit (0, kMaxPatterns - 1, order[sp]);
            }
            // Loop-Modus (ein Pattern): Dxx springt zur Zielzeile im selben Pattern,
            // Bxx faengt vorn an.
            row = (pendingBreakRow >= 0) ? pendingBreakRow : 0;
            row = juce::jlimit (0, kRows - 1, row);
            pendingPosJump  = -1;
            pendingBreakRow = -1;
            songStartPending = false; // ein Sprung auf Zeile 0 verbraucht den Start
        }
        else
        {
            row = currentRow.load() + 1;
            if (row >= kRows)
            {
                row = 0;
                // Pattern zu Ende: im Song-Modus zum naechsten Eintrag der Reihenfolge.
                if (songMode.load())
                {
                    if (songStartPending)
                    {
                        // Allererster Umlauf nach play(): bei order[0] bleiben,
                        // sonst faellt das erste Pattern der Reihenfolge weg.
                        songStartPending = false;
                    }
                    else
                    {
                        int sp = songPos.load() + 1;
                        if (sp >= orderLen) { sp = 0; songLoopCount.fetch_add (1, std::memory_order_relaxed); } // Reihenfolge umgelaufen
                        songPos = sp;
                        playPattern = juce::jlimit (0, kMaxPatterns - 1, order[sp]);
                    }
                }
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

            // Position-Jump (Bxx) / Pattern-Break (Dxx) fuer den naechsten Zeilenwechsel
            // vormerken (originalgetreue Modul-Wiedergabe; letzte Spur gewinnt).
            if (c.effect == 0xB)
                pendingPosJump = juce::jmax (0, c.effectParam);
            else if (c.effect == 0xD)
                pendingBreakRow = juce::jlimit (0, kRows - 1, c.effectParam);
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
        // Tape-Wow: jede Note startet mit eigener Phase (sonst eiern alle im Gleichtakt) -
        // deterministisch aus der Note abgeleitet, damit es reproduzierbar bleibt.
        v.wowPhase    = std::fmod ((double) note * 1.94161, juce::MathConstants<double>::twoPi);
        v.flutPhase   = 0.0;
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
            // Sample: optionale ADSR-Huellkurve (sonst envStage 0 = keine).
            v.envStage = inst->ampEnv ? 1 : 0;
            v.envLevel = 0.0f;
            v.fadeIn   = kFade;
            for (auto& chState : v.akaiLp) // Akai-Filter-Speicher leeren -> kein Knack
                for (auto& x : chState) x = 0.0f;
            v.srHold[0] = v.srHold[1] = 0.0f;
            v.srCount = 0;
            v.loopDir = 1;
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
        else if (v.inst->ampEnv)
            v.envStage = 4;             // Sample mit Huellkurve -> Release-Phase
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

    // VU-Spitzenpegel einer Spur hochziehen (max im aktuellen Block).
    void bumpTrackLevel (int vi, float pk)
    {
        if (vi >= 0 && vi < kTracks && pk > trackLevel[vi].load (std::memory_order_relaxed))
            trackLevel[vi].store (pk, std::memory_order_relaxed);
    }

    void render (juce::AudioBuffer<float>& buffer, int offset, int num)
    {
        const int outCh = buffer.getNumChannels();
        for (auto& v : voices)
        {
            if (! v.active || v.inst == nullptr)
                continue;

            // Solo/Mute: eine stumme bzw. nicht ge-solote Spur wird nicht hoerbar
            // gemacht - der Sequencer (Tick/Zeile) laeuft aber unveraendert weiter,
            // sodass die Spur beim Aufheben sofort wieder an der richtigen Stelle ist.
            if (! trackAudible (v.voiceIdx))
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
        const auto* inst = v.inst;
        const auto& d  = inst->data;
        const int len  = d.getNumSamples();
        const int srcCh = juce::jmin (d.getNumChannels(), 2); // Filter-Speicher fasst 2 Kanaele
        double pos = v.pos;
        float trackPk = 0.0f; // Spitzenpegel dieser Spur (VU-Anzeige)

        // Akai-Filter: Koeffizienten einmal pro Block (TPT-SVF, identische Mathe
        // wie der SID-Filter, hier aber in 2 Stufen kaskadiert = 24 dB/Okt).
        const bool  akaiOn = inst->akaiOn;
        const bool  crunch = inst->akai12bit;
        const bool  bit8   = inst->akai8bit;   // 8-Bit (Mirage/Fairlight)
        const bool  comp   = inst->companding; // EMU-II-Kompander (mu-law)
        const bool  rev    = inst->reverse;
        const auto  loop   = inst->loopMode;
        const bool  vintage = inst->vintagePitch;
        // Loop-Crossfade (nur Vorwaerts-Loop, nicht rueckwaerts): Laenge der
        // Ueberblendung am Schleifen-Ende, bis halbe Sample-Laenge. xfLen<2 = aus.
        int xfLen = 0;
        if (loop == Instrument::Loop::Forward && ! rev && inst->loopXfade > 0.0f && len > 8)
        {
            const int maxXf = (len - 1) / 2 - 1;
            xfLen = juce::jlimit (0, juce::jmax (0, maxXf),
                                  (int) std::round (juce::jlimit (0.0f, 1.0f, inst->loopXfade)
                                                    * 0.5f * (float) (len - 1)));
            if (xfLen < 2) xfLen = 0;
        }
        // Loop-Punkt: ab hier setzt die Vorwaerts-Schleife wieder ein (statt ab 0).
        // Ein eigener Loop-Punkt schaltet den Crossfade aus (Kopf laege sonst falsch).
        int loopStartSamp = 0;
        if (loop == Instrument::Loop::Forward && ! rev && inst->loopStart > 0.0f && len > 4)
        {
            loopStartSamp = juce::jlimit (0, len - 2, (int) (inst->loopStart * (float) (len - 1)));
            if (loopStartSamp > 0) xfLen = 0;
        }
        // Drive: Eingangs-Gain in die tanh-Saettigung; dry/wet ueber 'drive'
        // ueberblendet, damit drive=0 exakt der saubere Klang bleibt.
        const float drive   = juce::jlimit (0.0f, 1.0f, inst->drive);
        const float driveG  = 1.0f + drive * 8.0f;
        const float driveNorm = std::tanh (driveG); // Vollausschlag bleibt ~unveraendert laut
        // SR-Reduktion: jeden Quellwert ueber holdLen Ausgabe-Samples halten.
        // Quadratisch gestaffelt -> feines Steuern im unteren Bereich, bis ~48x.
        const float srAmt   = juce::jlimit (0.0f, 1.0f, inst->srReduction);
        const int   holdLen = srAmt > 0.0f ? 1 + (int) std::round (srAmt * srAmt * 47.0f) : 1;
        const float sr = (float) sampleRate;
        // Tape-Wow/Flutter (Mellotron): wow = langsames Band-Eiern (~0.7 Hz),
        // Flutter = schnelleres Flattern (~8 Hz). Beide modulieren multiplikativ
        // den Abspielschritt -> Tonhoehe schwankt. Tiefe steigt mit tapeWow.
        const float  wowAmt   = juce::jlimit (0.0f, 1.0f, inst->tapeWow);
        const double wowInc   = juce::MathConstants<double>::twoPi * 0.7 / (double) sr;
        const double flutInc  = juce::MathConstants<double>::twoPi * 8.3 / (double) sr;
        const double wowDepth = 0.025 * (double) wowAmt; // bis ~2.5 % Tonhoehe
        const double flutDepth= 0.006 * (double) wowAmt; // bis ~0.6 %
        // Sampler-Huellkurve (ADSR) + Lautstaerke.
        const bool  ampEnv  = inst->ampEnv;
        const float gainAmp = inst->gain;
        const float atkInc  = inst->attack  > 0.0f ? 1.0f / (inst->attack  * sr) : 1.0f;
        const float decInc  = inst->decay   > 0.0f ? (1.0f - inst->sustain) / (inst->decay * sr) : 1.0f;
        const float relInc  = inst->release > 0.0f ? 1.0f / (inst->release * sr) : 1.0f;
        float fa1 = 0.0f, fa2 = 0.0f, fa3 = 0.0f;
        if (akaiOn)
        {
            float fc = 30.0f * std::pow (380.0f, juce::jlimit (0.0f, 1.0f, inst->akaiCutoff));
            fc = juce::jlimit (20.0f, sr * 0.45f, fc);
            const float fg = std::tan (juce::MathConstants<float>::pi * fc / sr);
            const float fk = 2.0f - 1.9f * juce::jlimit (0.0f, 1.0f, inst->akaiResonance);
            fa1 = 1.0f / (1.0f + fg * (fg + fk));
            fa2 = fg * fa1;
            fa3 = fg * fa2;
        }
        // Eine Tiefpass-Stufe (TPT-SVF); c1/c2 = Speicher dieser Stufe.
        auto lpStage = [fa1, fa2, fa3] (float in, float& c1, float& c2)
        {
            const float n3 = in - c2;
            const float n1 = fa1 * c1 + fa2 * n3;
            const float n2 = c2 + fa2 * c1 + fa3 * n3;
            c1 = 2.0f * n1 - c1;
            c2 = 2.0f * n2 - c2;
            return n2;
        };

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
            // End-Ausblende nur beim einmaligen Abspielen - beim Loop laeuft es weiter.
            if (loop == Instrument::Loop::Off)
            {
                const double remain = (double) (len - 1) - pos;
                if (remain < (double) kFade)
                    env *= (float) (remain / (double) kFade);
            }

            // Note-Aus: in kFade Samples sanft auf Null fahren, dann Stimme aus.
            bool finish = false;
            if (v.fadeOut > 0)
            {
                env *= (float) v.fadeOut / (float) kFade;
                if (--v.fadeOut == 0)
                    finish = true;
            }

            // Sampler-Huellkurve (ADSR), falls aktiv: Attack/Decay/Sustain/Release.
            if (ampEnv)
            {
                switch (v.envStage)
                {
                    case 1: v.envLevel += atkInc; if (v.envLevel >= 1.0f)          { v.envLevel = 1.0f;          v.envStage = 2; } break;
                    case 2: v.envLevel -= decInc; if (v.envLevel <= inst->sustain) { v.envLevel = inst->sustain; v.envStage = 3; } break;
                    case 3: break; // Sustain halten, bis Note-Aus
                    case 4: v.envLevel -= relInc; if (v.envLevel <= 0.0f)          { v.envLevel = 0.0f;          finish = true; } break;
                    default: break;
                }
                env *= v.envLevel;
            }
            env *= gainAmp; // Instrument-Lautstaerke

            // Lesezeiger: bei Reverse vom Ende her. j0..j0+1 sind immer aufsteigend,
            // 'fr' ist der passende Bruchteil dazwischen.
            int   j0 = i0;
            float fr = frac;
            if (rev)
            {
                j0 = len - 2 - i0;          // Spiegelung; j0+1 bleibt im Puffer
                fr = 1.0f - frac;
                if (j0 < 0) { v.active = false; break; }
            }

            // SR-Reduktion: nur alle holdLen Samples einen frischen Quellwert holen,
            // sonst den gehaltenen weiterreichen (Sample-and-Hold -> Decimator).
            const bool fresh = (v.srCount <= 0);

            // Loop-Crossfade: liegt die Position im Ueberblend-Fenster kurz vor dem
            // Schleifen-Ende, das Ende (Tail) gleitend in den Anfang (Head, [0..xfLen])
            // ueberblenden -> nahtlose Schleife. xfTail < 0 = ausserhalb (kein Xfade).
            float  xfTail  = -1.0f;
            double headPos = 0.0;
            if (xfLen > 0 && pos > (double) ((len - 1) - xfLen))
            {
                const double t = ((double) (len - 1) - pos) / (double) xfLen; // 1..0
                xfTail  = (float) juce::jlimit (0.0, 1.0, t);
                headPos = pos - (double) ((len - 1) - xfLen);                  // 0..xfLen
            }

            // Quellsample je Kanal: lesen -> 12-Bit-Crunch -> Sample-and-Hold -> Akai-Tiefpass.
            float fs[2] = { 0.0f, 0.0f };
            for (int sc = 0; sc < srcCh; ++sc)
            {
                float s;
                if (fresh)
                {
                    const float* src = d.getReadPointer (sc);
                    // Vintage-Pitch: roh den naechsten Wert nehmen (keine Interpolation)
                    // -> crunchy/aliasing wie bei langsamer Wandler-Clock.
                    s = vintage ? src[j0]
                                : src[j0] + (src[j0 + 1] - src[j0]) * fr;
                    if (xfTail >= 0.0f) // equal-power-Ueberblendung Tail<->Head
                    {
                        const int   hb   = (int) headPos;
                        const float hf   = (float) (headPos - hb);
                        const float head = vintage ? src[hb]
                                                   : src[hb] + (src[hb + 1] - src[hb]) * hf;
                        const float wt = std::sin (0.5f * juce::MathConstants<float>::pi * xfTail);
                        const float wh = std::cos (0.5f * juce::MathConstants<float>::pi * xfTail);
                        s = s * wt + head * wh;
                    }
                    if (comp)
                    {
                        // EMU II: mu-law stauchen -> 12-Bit quantisieren -> spreizen.
                        // Leise Anteile feiner, laute saettigen weich (warm/druckvoll).
                        constexpr float mu = 255.0f;
                        const float L = std::log1p (mu);          // ln(1+mu)
                        const float a = juce::jlimit (-1.0f, 1.0f, s);
                        const float sg = a < 0.0f ? -1.0f : 1.0f;
                        float c = sg * std::log1p (mu * std::abs (a)) / L; // komprimiert -1..1
                        c = std::round (c * 2047.0f) / 2047.0f;            // 12-Bit
                        const float cg = c < 0.0f ? -1.0f : 1.0f;
                        s = cg * std::expm1 (std::abs (c) * L) / mu;       // gespreizt
                    }
                    else if (bit8)
                        s = std::round (juce::jlimit (-1.0f, 1.0f, s) * 127.0f) / 127.0f;   // 8 Bit (roh/koernig)
                    else if (crunch)
                        s = std::round (juce::jlimit (-1.0f, 1.0f, s) * 2047.0f) / 2047.0f; // 12 Bit
                    v.srHold[sc] = s;
                }
                else
                {
                    s = v.srHold[sc]; // gehaltener Wert (Treppenstufe)
                }
                if (akaiOn)
                {
                    s = lpStage (s, v.akaiLp[sc][0], v.akaiLp[sc][1]); // Stufe 1
                    s = lpStage (s, v.akaiLp[sc][2], v.akaiLp[sc][3]); // Stufe 2 -> 24 dB/Okt
                }
                // Drive: weiche Saettigung NACH dem Filter (so saettigt auch die
                // Resonanz musikalisch). dry/wet, damit drive=0 sauber bleibt.
                if (drive > 0.0f)
                {
                    const float wet = std::tanh (s * driveG) / driveNorm;
                    s = s * (1.0f - drive) + wet * drive;
                }
                fs[sc] = s;
            }
            v.srCount = fresh ? holdLen - 1 : v.srCount - 1;

            for (int ch = 0; ch < outCh; ++ch)
            {
                const float  s   = fs[juce::jmin (ch, srcCh - 1)];
                const float  g   = (ch == 0 ? v.gainL
                                  : ch == 1 ? v.gainR
                                            : 0.5f * (v.gainL + v.gainR));
                const float  out = s * g * env * 0.5f;
                buffer.addSample (ch, offset + i, out);
                if (ch == 0) trackPk = juce::jmax (trackPk, std::abs (out)); // VU-Pegel pro Spur
            }
            // Tape-Wow: effektiven Schritt fuer dieses Sample modulieren. wowAmt=0
            // -> Faktor exakt 1.0 (Sample bleibt unveraendert). Phasen pro Stimme.
            double effStep = v.step;
            if (wowAmt > 0.0f)
            {
                effStep *= 1.0 + wowDepth * std::sin (v.wowPhase)
                               + flutDepth * std::sin (v.flutPhase);
                v.wowPhase  += wowInc;
                v.flutPhase += flutInc;
                if (v.wowPhase  > juce::MathConstants<double>::twoPi) v.wowPhase  -= juce::MathConstants<double>::twoPi;
                if (v.flutPhase > juce::MathConstants<double>::twoPi) v.flutPhase -= juce::MathConstants<double>::twoPi;
            }

            // Position weiterfahren - je nach Loop-Modus.
            if (loop == Instrument::Loop::Off)
            {
                pos += effStep; // der Check oben faengt das Sample-Ende ab
            }
            else
            {
                const double hi = (double) (len - 1);
                pos += effStep * v.loopDir;
                if (loop == Instrument::Loop::PingPong)
                {
                    if (pos >= hi)      { pos = hi - (pos - hi); v.loopDir = -1; } // am Ende spiegeln
                    else if (pos < 0.0) { pos = -pos;           v.loopDir =  1; } // am Anfang spiegeln
                }
                else // Forward: an den Schleifen-Anfang zurueck
                {
                    // Schleifen-Boden: eigener Loop-Punkt, sonst der Crossfade-Beginn
                    // ([0..xfLen] ist schon ins Ende gemischt), sonst 0.
                    const double loopFloor = loopStartSamp > 0 ? (double) loopStartSamp
                                                              : (double) xfLen;
                    const double loopLen = hi - loopFloor;
                    if (pos >= hi)  pos -= loopLen;
                    if (pos < loopFloor) pos += loopLen;
                }
                pos = juce::jlimit (0.0, hi - 1.0e-4, pos); // robust in den Grenzen halten
            }

            if (finish)
            {
                v.active = false;
                break;
            }
        }
        v.pos = pos;
        bumpTrackLevel (v.voiceIdx, trackPk);
    }

    // KLASSISCH (selbstgebaut): Oszillator (Dreieck/Saege/Puls/Rauschen) + ADSR-
    // Huellkurve + eigenes State-Variable-Filter. Der vertraute RetroTrax-Klang.
    void renderSynthClassic (juce::AudioBuffer<float>& buffer, Voice& v, int offset, int num, int outCh)
    {
        const auto* inst = v.inst;
        float trackPk = 0.0f; // VU-Pegel pro Spur
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
        // Akkord aus einer Note: die Stapel-Stimmen liegen dann auf festen Halbton-
        // Abstaenden (Dreiklang u.ae.). Mit Ring/Sync ist der zweite Oszillator schon
        // belegt -> hoechstens eine Akkord-Zusatzstimme. Akkord schlaegt Unisono.
        float chSemi[2];
        int chExtra = Instrument::chordSemis (inst->chord, chSemi);
        if ((ring || sync) && chExtra > 1) chExtra = 1;

        const int    uni      = (chExtra > 0) ? (1 + chExtra)
                                              : ((ring || sync) ? 1 : juce::jlimit (1, 3, inst->unison));
        const double detCents = juce::jlimit (0.0f, 1.0f, inst->detune) * 25.0;
        const double upRatio  = std::pow (2.0, detCents / 1200.0);
        // Akkord-Ton (Halbton-Abstand) plus leichte Verstimmung fuer Breite; ohne
        // Akkord ist chSemi 0 -> exakt das alte Unisono-Verhalten.
        const double uniStep0 = mainStep * std::pow (2.0, chSemi[0] / 12.0) * upRatio; // 2. Stimme
        const double uniStep1 = mainStep * std::pow (2.0, chSemi[1] / 12.0) / upRatio; // 3. Stimme
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
                const float out = s * g * 0.42f;
                buffer.addSample (ch, offset + i, out);
                if (ch == 0) trackPk = juce::jmax (trackPk, std::abs (out)); // VU-Pegel
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
        bumpTrackLevel (v.voiceIdx, trackPk);
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
        else
            bumpTrackLevel (v.voiceIdx, 0.5f * v.vol); // grober VU-Pegel fuer den echten Chip
    }

    Voice voices[kTracks + 1]; // +1 = Vorhoer-Stimme
    SidChip sidChips[kTracks + 1]; // ein echter reSIDfp-Chip pro Stimme (inkl. Vorhoeren)
    std::unique_ptr<Instrument> preview; // Sample der ST-Disks-Vorschau
    std::unique_ptr<Instrument> kitPads[kPads]; // Drum-Kit: 16 Pads (eigene Samples)
    double sampleRate = 44100.0;
    double samplesUntilTick = 0.0;
    int    currentTick = 0; // 0 = Zeilenanfang, 1..speed-1 = Zwischenticks
};
