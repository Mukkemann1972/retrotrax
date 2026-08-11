#pragma once
//
// rt_freeze.h - Synth-Instrument (SID/Classic/FM) zu einem gleichwertigen
// Sample-Instrument einfrieren. JUCE-frei (nutzt nur TrackerEngine).
//
// Motiv: ein Amiga-68k-Player (s. tools/rtx_amiga) kann keine SID-/FM-Synthese
// in Echtzeit rechnen - Synth-Spuren muessen vorher zu Samples eingefroren
// werden. Der Trick: die Huellkurve wird waehrend der Aufnahme kuenstlich
// flach gehalten (Attack ~0, Sustain voll), damit die eingeschwungene
// Wellenform ohne Lautstaerke-Verlauf herauskommt. Per Autokorrelation wird
// danach die genaue Periodenlaenge gefunden und ein phasentreuer Ausschnitt
// (mehrere volle Zyklen, Start an einem steigenden Nulldurchgang) als Loop-
// Sample entnommen. Die ORIGINALEN Huellkurven-Werte wandern anschliessend
// unveraendert auf die ampEnv/attack/decay/sustain/release-Felder des
// Ergebnis-Sample-Instruments - das spielt danach durch denselben Sample-
// Envelope-Pfad wie jedes handgemachte Sample (renderSample() in
// TrackerEngine.h), keine neue Wiedergabe-Logik noetig.
//
// Grenze bei FM: jeder der 4 Operatoren hat eine eigene Huellkurve, die bei
// Modulator-Operatoren die Klangfarbe (nicht nur die Lautstaerke) ueber die
// Zeit veraendert - das ist bei echter FM nicht in "eine Wellenform + eine
// generische ADSR" zerlegbar. Freeze uebernimmt darum die Huellkurve des
// ERSTEN hoerbaren (Carrier-)Operators als Stellvertreter; bei Patches mit
// staerker wandernder Klangfarbe (z.B. lange Decay-Zeiten auf Modulatoren)
// ist das eine bewusste Vereinfachung, keine exakte Rekonstruktion.
//
// Pegel: die Synth- und die Sample-Wiedergabe in TrackerEngine.h haben eine
// strukturell unterschiedliche Gain-Kette (renderSample() hat u.a. einen
// sample-eigenen Fixfaktor, den renderSynthChip/-Fm/-Classic nicht haben) -
// deshalb wird der gefrorene Pegel nicht theoretisch nachgerechnet, sondern
// einmal kurz probegehoert und die Differenz in 'gain' ausgeglichen
// (detail::calibrateGain). Siehe rtx_cli freeze fuer den Live-vs-gefroren-
// Nachweis inkl. gemessener Toleranzen.

#include "TrackerEngine.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <vector>

namespace rtfreeze
{
    struct FreezeResult
    {
        std::unique_ptr<TrackerEngine::Instrument> instrument;
        bool   ok = false;
        double detectedFrequencyHz = 0.0;
        int    loopLengthSamples = 0;
    };

    // Findet die dominante Periodenlaenge (in Samples) per Autokorrelation.
    // score (0..1) sagt, wie klar/periodisch das Signal ist - unter ~0.85 ist
    // meist kein tonaler Ton (Rauschen, Stille). Auch von main.cpp's
    // Freeze-Selbsttest genutzt, um Live- und gefrorene Tonhoehe zu vergleichen.
    //
    // Oktav-Falle: bei einem sauber periodischen Signal korreliert nicht nur
    // die wahre Grundperiode P nahezu perfekt, sondern jedes Vielfache davon
    // (2P, 3P, ...) auch - und durch das kleiner werdende Ueberlappungsfenster
    // bei groesserem Lag kann eines dieser Vielfachen durch Rauschen sogar
    // knapp hoeher ausfallen als P selbst. Deshalb zwei Durchgaenge: erst den
    // besten Score ueber alle Lags finden, dann den KLEINSTEN Lag nehmen, der
    // nah genug an diesen besten Score herankommt (= die wahre Grundperiode,
    // nicht ein Vielfaches davon).
    inline int detectPeriod (const float* data, int len, double sampleRate, float& scoreOut)
    {
        const int minLag = std::max (2, (int) (sampleRate / 2000.0));  // bis 2000 Hz
        const int maxLag = std::min ((int) (sampleRate / 20.0), len / 2); // ab 20 Hz
        std::vector<float> scores ((size_t) std::max (0, maxLag - minLag), 0.0f);
        float globalBest = 0.0f;
        for (int lag = minLag; lag < maxLag; ++lag)
        {
            double sum = 0.0, denomA = 0.0, denomB = 0.0;
            const int n = len - lag;
            for (int i = 0; i < n; ++i)
            {
                const float a = data[i];
                const float b = data[i + lag];
                sum    += (double) a * b;
                denomA += (double) a * a;
                denomB += (double) b * b;
            }
            const double denom = std::sqrt (denomA * denomB);
            const float score = denom > 1e-9 ? (float) (sum / denom) : 0.0f;
            scores[(size_t) (lag - minLag)] = score;
            if (score > globalBest) globalBest = score;
        }

        int bestLag = 0;
        for (int lag = minLag; lag < maxLag; ++lag)
        {
            if (scores[(size_t) (lag - minLag)] >= 0.95f * globalBest)
            {
                bestLag = lag;
                break;
            }
        }
        scoreOut = globalBest;
        return bestLag;
    }

    // Spielt eine Kopie von 'inst' auf einer frischen, eigenstaendigen Engine
    // (previewInstrument, Note gehalten) und rendert 'totalFrames' Samples
    // (Kanal 0) nach 'outMono'. Gemeinsame Grundlage fuer freezeInstrument()
    // und den Live-vs-gefroren-Vergleich im rtx_cli-Selbsttest.
    inline void renderInstrumentMono (const TrackerEngine::Instrument& inst, double sampleRate,
                                      int note, long totalFrames, std::vector<float>& outMono)
    {
        TrackerEngine engine;
        engine.prepare (sampleRate);
        engine.previewInstrument (std::make_unique<TrackerEngine::Instrument> (inst), note);

        const int block = 512;
        juce::AudioBuffer<float> buf (2, block);
        outMono.clear();
        outMono.reserve ((size_t) totalFrames);
        long rendered = 0;
        while (rendered < totalFrames)
        {
            buf.clear();
            engine.process (buf);
            const float* L = buf.getReadPointer (0);
            const int n = (int) std::min<long> (block, totalFrames - rendered);
            for (int i = 0; i < n; ++i)
                outMono.push_back (L[i]);
            rendered += block;
        }
    }

    namespace detail
    {
        // Baut aus der entnommenen Loop-Wellenform ein normales Sample-
        // Instrument, mit der originalen Huellkurve des Synths.
        inline std::unique_ptr<TrackerEngine::Instrument> buildSampleInstrument (
            const TrackerEngine::Instrument& synth, const std::vector<float>& loopData,
            double sampleRate)
        {
            using Inst = TrackerEngine::Instrument;
            auto out = std::make_unique<Inst> ();
            out->kind       = Inst::Kind::Sample;
            out->name       = synth.name + " (eingefroren)";
            out->sourceRate = sampleRate;
            out->data.setSize (1, (int) loopData.size());
            std::copy (loopData.begin(), loopData.end(), out->data.getWritePointer (0));

            out->ampEnv    = true;
            out->gain      = synth.gain;
            out->loopMode  = Inst::Loop::Forward;
            out->loopStart = 0.0f;
            out->loopXfade = 0.0f;

            if (synth.engine == Inst::Engine::Fm)
            {
                int carrier = 0;
                const auto alg = TrackerEngine::fmAlgorithm (synth.fmAlgo);
                for (int o = 0; o < Inst::kFmOps; ++o)
                    if (alg.carrier[o]) { carrier = o; break; }
                out->attack  = synth.fmAttack[carrier];
                out->decay   = synth.fmDecay[carrier];
                out->sustain = synth.fmSustain[carrier];
                out->release = synth.fmRelease[carrier];
            }
            else
            {
                out->attack  = synth.attack;
                out->decay   = synth.decay;
                out->sustain = synth.sustain;
                out->release = synth.release;
            }
            return out;
        }

        // Pegel-Abgleich: die aufgezeichnete Wellenform durchlief beim Einfangen
        // den SYNTH-Wiedergabepfad (renderSynthChip/-Fm/-Classic), beim spaeteren
        // Abspielen als Sample laeuft aber renderSample() - eine strukturell
        // andere Gain-Kette (u.a. ein sample-eigener Fixfaktor). Statt diese
        // Kette theoretisch nachzurechnen: einmal kurz mit flacher Huellkurve
        // probehoeren und den gemessenen Pegelunterschied in 'gain' ausgleichen,
        // damit das gefrorene Instrument beim Sustain-Pegel genauso laut klingt
        // wie das Original.
        inline void calibrateGain (TrackerEngine::Instrument& sampleInst,
                                   const std::vector<float>& loopData,
                                   double sampleRate, int referenceNote)
        {
            using Inst = TrackerEngine::Instrument;
            auto calib = std::make_unique<Inst> (sampleInst);
            calib->attack = 1.0f / (float) sampleRate;
            calib->sustain = 1.0f;
            calib->decay = 0.001f;
            calib->release = 0.001f;

            const long skipFrames = (long) (0.15 * sampleRate);
            const long total = skipFrames + (long) loopData.size();
            std::vector<float> playback;
            renderInstrumentMono (*calib, sampleRate, referenceNote, total, playback);
            if ((long) playback.size() < total)
                return;

            double srcSum = 0.0, playSum = 0.0;
            for (size_t i = 0; i < loopData.size(); ++i)
            {
                srcSum  += std::abs (loopData[i]);
                playSum += std::abs (playback[(size_t) skipFrames + i]);
            }
            if (playSum > 1e-9)
                sampleInst.gain *= (float) (srcSum / playSum);
        }
    }

    // referenceNote: Tonhoehe, bei der eingefroren wird (60 = C-5, wie previewInstrument).
    // loopCycles: so viele volle Wellenperioden landen im entnommenen Loop-Sample.
    inline FreezeResult freezeInstrument (const TrackerEngine::Instrument& synth,
                                          double sampleRate = 44100.0,
                                          int referenceNote = 60,
                                          int loopCycles = 8)
    {
        using Inst = TrackerEngine::Instrument;
        FreezeResult result;
        if (synth.kind != Inst::Kind::Synth)
            return result;

        // 1) Kopie mit kuenstlich flacher Huellkurve (Attack ~0, Sustain voll) -
        //    SID/Classic teilen sich attack/sustain, FM bekommt es je Operator.
        auto probe = std::make_unique<Inst> (synth);
        const float tinyAttack = 1.0f / (float) sampleRate;
        probe->attack  = tinyAttack;
        probe->sustain = 1.0f;
        for (int o = 0; o < Inst::kFmOps; ++o)
        {
            probe->fmAttack[o]  = tinyAttack;
            probe->fmSustain[o] = 1.0f;
        }

        // 2) Rendern: previewInstrument spielt auf einer eigenen Stimme
        //    (Index kTracks), unabhaengig von Patterns/Transport.
        const double skipSeconds    = 0.15;  // Einschwingzeit ueberspringen
        const double captureSeconds = 0.35;  // Material fuer die Periodenanalyse
        const long skipFrames    = (long) (skipSeconds * sampleRate);
        const long totalFrames   = skipFrames + (long) (captureSeconds * sampleRate);

        std::vector<float> mono;
        renderInstrumentMono (*probe, sampleRate, referenceNote, totalFrames, mono);
        if ((long) mono.size() < skipFrames + 64)
            return result;

        // 3) Periode per Autokorrelation im eingeschwungenen Bereich finden.
        const float* analysis = mono.data() + skipFrames;
        const int analysisLen = (int) mono.size() - (int) skipFrames;
        float bestScore = 0.0f;
        const int bestLag = detectPeriod (analysis, analysisLen, sampleRate, bestScore);

        if (bestLag <= 0 || bestScore < 0.85f)
        {
            // Keine klare Periode (z.B. reines Rauschen) - Rueckfall: festes
            // Fenster ohne Periodenausrichtung. Kann an der Schleifennaht
            // hoerbar klicken (dokumentierte Phase-1-Grenze fuer Noise-Wellen).
            const int fallbackLen = std::min (4096, analysisLen);
            std::vector<float> data (analysis, analysis + fallbackLen);
            result.instrument = detail::buildSampleInstrument (synth, data, sampleRate);
            if (result.instrument != nullptr)
                detail::calibrateGain (*result.instrument, data, sampleRate, referenceNote);
            result.loopLengthSamples = fallbackLen;
            result.ok = (result.instrument != nullptr);
            return result;
        }

        // 4) Schleifenanfang auf einen steigenden Nulldurchgang legen, damit
        //    die Schleife phasentreu an sich selbst anschliesst.
        int start = 0;
        for (int i = 1; i < bestLag && i + 1 < analysisLen; ++i)
        {
            if (analysis[i - 1] <= 0.0f && analysis[i] > 0.0f)
            {
                start = i;
                break;
            }
        }

        int cycles = std::max (1, loopCycles);
        if (start + bestLag * cycles > analysisLen)
            cycles = std::max (1, (analysisLen - start) / bestLag);
        const int finalLen = bestLag * cycles;

        std::vector<float> loopData (analysis + start, analysis + start + finalLen);
        result.instrument = detail::buildSampleInstrument (synth, loopData, sampleRate);
        if (result.instrument != nullptr)
            detail::calibrateGain (*result.instrument, loopData, sampleRate, referenceNote);
        result.detectedFrequencyHz = sampleRate / (double) bestLag;
        result.loopLengthSamples   = finalLen;
        result.ok = (result.instrument != nullptr);
        return result;
    }
}
