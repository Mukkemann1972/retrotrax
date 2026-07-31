#pragma once
//
// FmPresets.h - fertige FM-Klaenge zum Anklicken.
//
// FM klingt grossartig, ist aber beim Schrauben unzugaenglich: vier Operatoren
// mal sechs Werte, dazu Algorithmus und Rueckkopplung - wer das von Null
// aufbaut, landet meistens bei Krach. Genau deshalb kamen die alten Geraete
// (DX7, Mega Drive, AdLib) mit Werksklaengen, und genauso machen wir es:
// einmal klicken, sofort ein brauchbarer Klang, von dort aus weiterdrehen.
//
// Die Werte folgen dem klassischen Muster:
//   ratio  = Tonhoehe des Operators als Vielfaches der Note. Ganze Zahlen
//            klingen harmonisch (1, 2, 3 ...), krumme metallisch/glockig.
//   level  = wie stark er wirkt. Bei Modulatoren = wie hell/scharf der Klang,
//            bei Traegern = wie laut.
//   ADSR   = eigene Huellkurve. Der Trick der FM: gibt man dem MODULATOR eine
//            kurze Huellkurve, wird der Klang im Anschlag hell und danach
//            weich - genau das macht ein E-Piano aus.

#include "TrackerEngine.h"

namespace FmPresets
{
    struct Preset
    {
        const char* nameDe;
        const char* nameEn;
        int   algo;
        float feedback;
        float ratio[4];
        float level[4];
        float attack[4];
        float decay[4];
        float sustain[4];
        float release[4];
    };

    inline const Preset* table (int& count)
    {
        static const Preset presets[] =
        {
            // --- E-PIANO: DER FM-Klang schlechthin (DX7 "Rhodes") ------------
            // Algorithmus 4 = zwei getrennte Paare. Jeder Modulator hat eine
            // schnell abfallende Huellkurve -> im Anschlag glockig-hell, danach
            // rund und weich. Das Verhaeltnis 14:1 im zweiten Paar gibt den
            // typischen metallischen Anschlag obendrauf.
            { "E-PIANO", "E-PIANO", 4, 0.0f,
              { 1.0f, 1.0f, 14.0f, 1.0f },
              { 0.62f, 1.0f, 0.18f, 0.55f },
              { 0.000f, 0.000f, 0.000f, 0.000f },
              { 0.42f, 1.60f, 0.12f, 1.40f },
              { 0.00f, 0.30f, 0.00f, 0.25f },
              { 0.28f, 0.60f, 0.10f, 0.55f } },

            // --- GLOCKE: krumme Verhaeltnisse = unharmonisch = metallisch ----
            { "GLOCKE", "BELL", 4, 0.0f,
              { 3.5f, 1.0f, 7.0f, 2.0f },
              { 0.75f, 1.0f, 0.45f, 0.45f },
              { 0.000f, 0.000f, 0.000f, 0.000f },
              { 1.20f, 2.60f, 0.90f, 2.20f },
              { 0.00f, 0.00f, 0.00f, 0.00f },
              { 0.80f, 1.60f, 0.70f, 1.40f } },

            // --- BASS: Mega-Drive-Bass, kurz und knackig mit Rueckkopplung ---
            { "BASS", "BASS", 0, 0.55f,
              { 1.0f, 2.0f, 1.0f, 1.0f },
              { 0.55f, 0.65f, 0.85f, 1.0f },
              { 0.000f, 0.000f, 0.000f, 0.002f },
              { 0.18f, 0.20f, 0.22f, 0.55f },
              { 0.10f, 0.15f, 0.30f, 0.55f },
              { 0.10f, 0.12f, 0.15f, 0.22f } },

            // --- BRASS: langsamer Anstieg im Modulator = "blaest sich auf" ---
            { "BLAeSER", "BRASS", 2, 0.30f,
              { 1.0f, 1.0f, 1.0f, 1.0f },
              { 0.60f, 0.0f, 0.70f, 1.0f },
              { 0.090f, 0.000f, 0.060f, 0.030f },
              { 0.40f, 0.20f, 0.40f, 0.30f },
              { 0.70f, 0.00f, 0.75f, 0.85f },
              { 0.22f, 0.10f, 0.22f, 0.30f } },

            // --- LEAD: durchsetzungsfaehig, viel Rueckkopplung = saegig ------
            { "LEAD", "LEAD", 1, 0.70f,
              { 1.0f, 2.0f, 1.0f, 1.0f },
              { 0.50f, 0.45f, 0.75f, 1.0f },
              { 0.002f, 0.002f, 0.002f, 0.004f },
              { 0.30f, 0.30f, 0.30f, 0.40f },
              { 0.60f, 0.60f, 0.70f, 0.85f },
              { 0.15f, 0.15f, 0.18f, 0.25f } },

            // --- ORGEL: Algorithmus 7, alle parallel = rein additiv ----------
            // Vier Sinusse auf Oktav-/Quint-Abstaenden - genau wie die Zugriegel
            // einer Orgel. Keine Modulation, nur Zusammenklang.
            { "ORGEL", "ORGAN", 7, 0.0f,
              { 1.0f, 2.0f, 3.0f, 4.0f },
              { 1.0f, 0.55f, 0.35f, 0.25f },
              { 0.004f, 0.004f, 0.004f, 0.004f },
              { 0.10f, 0.10f, 0.10f, 0.10f },
              { 1.00f, 1.00f, 1.00f, 1.00f },
              { 0.08f, 0.08f, 0.08f, 0.08f } },

            // --- HOLZ/MARIMBA: kurzer Anschlag, schnell weg -------------------
            { "MARIMBA", "MARIMBA", 4, 0.0f,
              { 1.0f, 1.0f, 4.0f, 1.0f },
              { 0.70f, 1.0f, 0.30f, 0.40f },
              { 0.000f, 0.000f, 0.000f, 0.000f },
              { 0.10f, 0.55f, 0.06f, 0.45f },
              { 0.00f, 0.00f, 0.00f, 0.00f },
              { 0.10f, 0.30f, 0.06f, 0.25f } },

            // --- PIEPS/BLIP: der harte Chiptune-Ton fuer Effekte -------------
            { "BLIP", "BLIP", 0, 0.85f,
              { 1.0f, 3.0f, 2.0f, 1.0f },
              { 0.80f, 0.70f, 0.80f, 1.0f },
              { 0.000f, 0.000f, 0.000f, 0.000f },
              { 0.05f, 0.05f, 0.05f, 0.07f },
              { 0.00f, 0.00f, 0.00f, 0.00f },
              { 0.04f, 0.04f, 0.04f, 0.05f } },
        };
        count = (int) (sizeof (presets) / sizeof (presets[0]));
        return presets;
    }

    // Preset auf ein Instrument legen (schaltet den Klangmotor gleich mit um).
    inline void apply (TrackerEngine::Instrument& inst, int index)
    {
        int n = 0;
        const Preset* p = table (n);
        if (index < 0 || index >= n)
            return;
        const Preset& s = p[index];

        inst.kind      = TrackerEngine::Instrument::Kind::Synth;
        inst.engine    = TrackerEngine::Instrument::Engine::Fm;
        inst.fmAlgo    = s.algo;
        inst.fmFeedback = s.feedback;
        for (int o = 0; o < TrackerEngine::Instrument::kFmOps; ++o)
        {
            inst.fmRatio[o]   = s.ratio[o];
            inst.fmLevel[o]   = s.level[o];
            inst.fmAttack[o]  = s.attack[o];
            inst.fmDecay[o]   = s.decay[o];
            inst.fmSustain[o] = s.sustain[o];
            inst.fmRelease[o] = s.release[o];
        }
    }
}
