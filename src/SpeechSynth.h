#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>
#include <cmath>

// Eigene kleine Sprachsynthese (Formant-Synthese, komplett unser Code - keine
// fremde Bibliothek): tippt man einen Satz, wird er in eine Robo-Stimme
// verwandelt, im Geist von Speak&Spell/SAM(C64)/Amiga-Narrator, aber mit
// eigenen Regeln und eigenem Klangmotor. Das Ergebnis ist ein ganz normales
// Mono-Sample-Buffer - es spielt danach wie jedes andere Sample im Slot
// (Akai-Filter/12-Bit/Drive/Loop wirken automatisch mit).
namespace SpeechSynth
{
    // Zwei Klang-Charaktere - ein Motor, unterschiedliche Vorgabewerte + Faerbung.
    enum class Character { Sam, Narrator };

    struct Params
    {
        Character character = Character::Sam;
        float speed   = 1.0f;   // 0.5 (langsam) .. 2.0 (schnell)
        float pitchHz = 100.0f; // Grundtonhoehe 60..300 Hz
        float throat  = 0.0f;   // -1 (hell/klein) .. 1 (tief/gross) - verschiebt Formanten
        float mouth   = 0.5f;   // 0 (eng/genuschelt) .. 1 (weit/offen) - F1/Bandbreite
    };

    static Params defaultParams (Character c)
    {
        Params p;
        p.character = c;
        if (c == Character::Sam)
        {
            // SAM: kleiner/heller "Vokaltrakt" (hoehere Formanten -> blecherner,
            // nasaler C64-Chip-Charakter), hoehere Tonlage, etwas engerer Mund.
            p.pitchHz = 115.0f; p.speed = 1.15f; p.throat = -0.35f; p.mouth = 0.35f;
        }
        else
        {
            // Narrator: groesserer/dunklerer "Vokaltrakt" (tiefere Formanten ->
            // wärmerer Amiga-Erzaehler-Charakter), tiefere Tonlage, offenerer Mund.
            p.pitchHz = 75.0f;  p.speed = 0.95f; p.throat = 0.30f;  p.mouth = 0.60f;
        }
        return p;
    }

    // ---------------------------------------------------------------------
    // Phonem-Satz (vereinfachtes Englisch-Set, ausreichend fuer verstaendliche
    // Robo-Aussprache - keine perfekte Linguistik, das gehoert zum Charme).
    enum class Ph
    {
        Sil,
        // Monophthonge (Vokale)
        IY, IH, EH, AE, AA, AH, AO, UH, UW, ER,
        // Diphthonge (Gleiten zwischen zwei Zielklaengen)
        EY, AY, OW, AW, OY,
        // Gleitlaute
        Y, W, L, R,
        // Nasale
        M, N, NG,
        // Stimmhafte Verschlusslaute
        B, D, G,
        // Stimmlose Verschlusslaute
        P, T, K,
        // Stimmhafte Reibelaute + Affrikate
        V, DH, Z, ZH, JH,
        // Stimmlose Reibelaute + Affrikate
        F, TH, S, SH, CH, HH
    };

    enum class PhType { Silence, Vowel, Diphthong, Glide, Nasal, Stop, Fricative };

    struct PhInfo
    {
        PhType type;
        bool   voiced;
        float  f1, f2, f3;     // Formant-Zielfrequenzen (Hz) bei "neutraler" Stimme
        float  bw;             // relative Bandbreite (1 = normal, >1 weicher/breiter)
        float  durMs;          // Grunddauer in ms (vor Speed-Skalierung)
        float  amp;            // relative Lautstaerke 0..1
        Ph     glideTo = Ph::Sil; // nur bei Diphthongen: zweiter Zielklang
    };

    static const PhInfo& info (Ph p)
    {
        // F1/F2/F3 grob nach klassischen (oeffentlichen) Formant-Messwerten
        // amerikanischer Vokale (Peterson/Barney 1952) - allgemeines Lehrbuch-
        // wissen der Phonetik, keine fremde Codebasis.
        static const PhInfo table[] = {
            /* Sil */ { PhType::Silence,   false,    0,    0,    0, 1.0f,  70.0f, 0.0f },
            /* IY  */ { PhType::Vowel,     true,   270, 2290, 3010, 1.0f, 130.0f, 0.9f },
            /* IH  */ { PhType::Vowel,     true,   390, 1990, 2550, 1.0f, 110.0f, 0.85f },
            /* EH  */ { PhType::Vowel,     true,   530, 1840, 2480, 1.0f, 120.0f, 0.9f },
            /* AE  */ { PhType::Vowel,     true,   660, 1720, 2410, 1.0f, 140.0f, 0.9f },
            /* AA  */ { PhType::Vowel,     true,   730, 1090, 2440, 1.0f, 140.0f, 0.95f },
            /* AH  */ { PhType::Vowel,     true,   520, 1190, 2390, 1.0f, 110.0f, 0.85f },
            /* AO  */ { PhType::Vowel,     true,   570,  840, 2410, 1.0f, 140.0f, 0.9f },
            /* UH  */ { PhType::Vowel,     true,   440, 1020, 2240, 1.0f, 100.0f, 0.8f },
            /* UW  */ { PhType::Vowel,     true,   300,  870, 2240, 1.0f, 130.0f, 0.85f },
            /* ER  */ { PhType::Vowel,     true,   490, 1350, 1690, 1.0f, 130.0f, 0.85f },
            /* EY  */ { PhType::Diphthong, true,   530, 1840, 2480, 1.0f, 180.0f, 0.9f, Ph::IY },
            /* AY  */ { PhType::Diphthong, true,   730, 1090, 2440, 1.0f, 190.0f, 0.9f, Ph::IY },
            /* OW  */ { PhType::Diphthong, true,   570,  840, 2410, 1.0f, 180.0f, 0.9f, Ph::UW },
            /* AW  */ { PhType::Diphthong, true,   730, 1090, 2440, 1.0f, 190.0f, 0.9f, Ph::UW },
            /* OY  */ { PhType::Diphthong, true,   570,  840, 2410, 1.0f, 190.0f, 0.9f, Ph::IY },
            /* Y   */ { PhType::Glide,     true,   260, 2070, 3020, 1.0f,  70.0f, 0.6f },
            /* W   */ { PhType::Glide,     true,   290,  610, 2150, 1.0f,  70.0f, 0.6f },
            /* L   */ { PhType::Glide,     true,   360, 1300, 2800, 1.0f,  80.0f, 0.7f },
            /* R   */ { PhType::Glide,     true,   310, 1060, 1380, 1.0f,  80.0f, 0.7f },
            /* M   */ { PhType::Nasal,     true,   250, 1000, 2200, 0.6f,  90.0f, 0.55f },
            /* N   */ { PhType::Nasal,     true,   280, 1700, 2600, 0.6f,  90.0f, 0.55f },
            /* NG  */ { PhType::Nasal,     true,   280, 2300, 2800, 0.6f,  90.0f, 0.5f },
            /* B   */ { PhType::Stop,      true,   200,  900, 2100, 1.0f,  70.0f, 0.5f },
            /* D   */ { PhType::Stop,      true,   250, 1700, 2600, 1.0f,  70.0f, 0.5f },
            /* G   */ { PhType::Stop,      true,   250, 2000, 2700, 1.0f,  70.0f, 0.5f },
            /* P   */ { PhType::Stop,      false,  200,  900, 2100, 1.0f,  70.0f, 0.55f },
            /* T   */ { PhType::Stop,      false,  250, 1700, 4000, 1.0f,  70.0f, 0.6f },
            /* K   */ { PhType::Stop,      false,  250, 2000, 3000, 1.0f,  75.0f, 0.55f },
            /* V   */ { PhType::Fricative, true,   300, 1100, 2200, 1.0f, 100.0f, 0.5f },
            /* DH  */ { PhType::Fricative, true,   300, 1400, 2400, 1.0f,  90.0f, 0.45f },
            /* Z   */ { PhType::Fricative, true,   300, 1700, 4300, 1.0f, 110.0f, 0.55f },
            /* ZH  */ { PhType::Fricative, true,   300, 1800, 2800, 1.0f, 110.0f, 0.5f },
            /* JH  */ { PhType::Fricative, true,   250, 1800, 2700, 1.0f, 100.0f, 0.55f },
            /* F   */ { PhType::Fricative, false,  300, 1100, 2200, 1.0f, 110.0f, 0.4f },
            /* TH  */ { PhType::Fricative, false,  300, 1400, 2400, 1.0f, 100.0f, 0.4f },
            /* S   */ { PhType::Fricative, false,  300, 1700, 5500, 1.0f, 130.0f, 0.55f },
            /* SH  */ { PhType::Fricative, false,  300, 1800, 2800, 1.0f, 130.0f, 0.55f },
            /* CH  */ { PhType::Fricative, false,  300, 1800, 2800, 1.0f, 110.0f, 0.5f },
            /* HH  */ { PhType::Fricative, false,  500, 1500, 2500, 1.5f,  70.0f, 0.35f },
        };
        return table[(int) p];
    }

    // ---------------------------------------------------------------------
    // Text -> Phoneme (eigene, bewusst einfache Buchstabe-zu-Laut-Regeln;
    // keine perfekte Aussprache, aber verstaendlich und mit Robo-Charme).

    static bool isVowelLetter (juce::juce_wchar c)
    {
        return c == 'A' || c == 'E' || c == 'I' || c == 'O' || c == 'U';
    }

    static void appendWord (const juce::String& wordIn, std::vector<Ph>& out)
    {
        juce::String w = wordIn.toUpperCase();
        const int n = w.length();
        if (n == 0)
            return;

        // Ganze Zahlen 0-9 als Wort aussprechen (kommt in Songnamen oft vor).
        static const char* digitWords[10] = { "ZERO","ONE","TWO","THREE","FOUR",
                                               "FIVE","SIX","SEVEN","EIGHT","NINE" };
        if (n == 1 && w[0] >= '0' && w[0] <= '9')
        {
            appendWord (digitWords[w[0] - '0'], out);
            return;
        }

        // "Magic E": Wort endet auf Konsonant+E -> der Vokal davor wird "lang"
        // gesprochen, das E selbst bleibt stumm (make, bike, home, cute ...).
        bool magicE = false;
        if (n >= 3 && w[n - 1] == 'E' && ! isVowelLetter (w[n - 2]) && isVowelLetter (w[n - 3]))
            magicE = true;

        static const juce::StringArray voicedThe { "THE","THIS","THAT","THESE","THOSE",
            "THEY","THEM","THEN","THAN","THERE","THOUGH","THUS","THEIR","THY" };
        const bool thVoiced = voicedThe.contains (w);

        int i = 0;
        const int end = magicE ? n - 1 : n; // stummes Schluss-E ueberspringen
        while (i < end)
        {
            const juce::juce_wchar c  = w[i];
            const juce::juce_wchar c1 = (i + 1 < n) ? w[i + 1] : 0;
            const juce::juce_wchar c2 = (i + 2 < n) ? w[i + 2] : 0;
            const juce::juce_wchar c3 = (i + 3 < n) ? w[i + 3] : 0;
            const bool wordStart = (i == 0);
            const bool wordEnd   = (i + 1 >= end);

            // -- Mehrbuchstaben-Regeln zuerst (laengster Treffer gewinnt) --
            if (c == 'T' && c1 == 'I' && c2 == 'O' && c3 == 'N') { out.push_back (Ph::SH); out.push_back (Ph::AH); out.push_back (Ph::N); i += 4; continue; }
            if (c == 'S' && c1 == 'I' && c2 == 'O' && c3 == 'N') { out.push_back (Ph::ZH); out.push_back (Ph::AH); out.push_back (Ph::N); i += 4; continue; }
            if (wordStart && c == 'W' && c1 == 'R') { out.push_back (Ph::R); i += 2; continue; }
            if (wordStart && c == 'K' && c1 == 'N') { out.push_back (Ph::N); i += 2; continue; }
            if (wordEnd   && c == 'M' && c1 == 'B') { out.push_back (Ph::M); i += 2; continue; }
            if (c == 'I' && c1 == 'N' && c2 == 'G' && wordEnd) { out.push_back (Ph::IH); out.push_back (Ph::NG); i += 3; continue; }
            if (c == 'C' && c1 == 'K') { out.push_back (Ph::K); i += 2; continue; }
            if (c == 'S' && c1 == 'H') { out.push_back (Ph::SH); i += 2; continue; }
            if (c == 'C' && c1 == 'H') { out.push_back (Ph::CH); i += 2; continue; }
            if (c == 'P' && c1 == 'H') { out.push_back (Ph::F); i += 2; continue; }
            if (c == 'W' && c1 == 'H') { out.push_back (Ph::W); i += 2; continue; }
            if (c == 'Q' && c1 == 'U') { out.push_back (Ph::K); out.push_back (Ph::W); i += 2; continue; }
            if (c == 'N' && c1 == 'G') { out.push_back (Ph::NG); i += 2; continue; }
            if (c == 'T' && c1 == 'H') { out.push_back (thVoiced ? Ph::DH : Ph::TH); i += 2; continue; }

            // -- Vokal-Digraphen --
            if (c == 'E' && c1 == 'E') { out.push_back (Ph::IY); i += 2; continue; }
            if (c == 'E' && c1 == 'A') { out.push_back (Ph::IY); i += 2; continue; }
            if (c == 'O' && c1 == 'O') { out.push_back (Ph::UW); i += 2; continue; }
            if (c == 'O' && c1 == 'A') { out.push_back (Ph::OW); i += 2; continue; }
            if ((c == 'A' && c1 == 'I') || (c == 'A' && c1 == 'Y') || (c == 'E' && c1 == 'I') || (c == 'E' && c1 == 'Y'))
                { out.push_back (Ph::EY); i += 2; continue; }
            if (c == 'I' && c1 == 'E') { out.push_back (Ph::IY); i += 2; continue; }
            if ((c == 'O' && c1 == 'Y') || (c == 'O' && c1 == 'I')) { out.push_back (Ph::OY); i += 2; continue; }
            if (c == 'O' && c1 == 'U') { out.push_back (Ph::AW); i += 2; continue; }
            if (c == 'O' && c1 == 'W') { out.push_back (wordEnd ? Ph::OW : Ph::AW); i += 2; continue; }
            if ((c == 'A' && c1 == 'U') || (c == 'A' && c1 == 'W')) { out.push_back (Ph::AO); i += 2; continue; }
            if ((c == 'U' && c1 == 'E') || (c == 'U' && c1 == 'I')) { out.push_back (Ph::UW); i += 2; continue; }
            if (c == 'I' && c1 == 'G' && c2 == 'H') { out.push_back (Ph::AY); i += 3; continue; }

            // -- Doppelte Konsonanten wie im Englischen ueblich zusammenfassen --
            if (c == c1 && ! isVowelLetter (c) && c != 'Y') { i += 1; continue; }

            // -- Einzelbuchstaben --
            switch (c)
            {
                case 'A': out.push_back (magicE && wordEnd ? Ph::EY : Ph::AE); break;
                case 'E': out.push_back (magicE && wordEnd ? Ph::IY : Ph::EH); break;
                case 'I': out.push_back (magicE && wordEnd ? Ph::AY : Ph::IH); break;
                case 'O': out.push_back (magicE && wordEnd ? Ph::OW : Ph::AA); break;
                case 'U': out.push_back (magicE && wordEnd ? Ph::UW : Ph::AH); break;
                case 'Y': out.push_back (wordStart ? Ph::Y : (wordEnd ? Ph::IY : Ph::IH)); break;
                case 'B': out.push_back (Ph::B); break;
                case 'C': out.push_back ((c1 == 'E' || c1 == 'I' || c1 == 'Y') ? Ph::S : Ph::K); break;
                case 'D': out.push_back (Ph::D); break;
                case 'F': out.push_back (Ph::F); break;
                case 'G': out.push_back ((c1 == 'E' || c1 == 'I' || c1 == 'Y') ? Ph::JH : Ph::G); break;
                case 'H': out.push_back (Ph::HH); break;
                case 'J': out.push_back (Ph::JH); break;
                case 'K': out.push_back (Ph::K); break;
                case 'L': out.push_back (Ph::L); break;
                case 'M': out.push_back (Ph::M); break;
                case 'N': out.push_back (Ph::N); break;
                case 'P': out.push_back (Ph::P); break;
                case 'Q': out.push_back (Ph::K); break;
                case 'R': out.push_back (Ph::R); break;
                case 'S':
                {
                    // Mehrzahl-S nach stimmhaftem Laut klingt eher wie Z (dogs, cars ...).
                    const bool afterVoiced = i > 0 && (isVowelLetter (w[i - 1])
                        || w[i - 1] == 'L' || w[i - 1] == 'M' || w[i - 1] == 'N'
                        || w[i - 1] == 'R' || w[i - 1] == 'V' || w[i - 1] == 'D'
                        || w[i - 1] == 'G' || w[i - 1] == 'B');
                    out.push_back (wordEnd && afterVoiced ? Ph::Z : Ph::S);
                    break;
                }
                case 'T': out.push_back (Ph::T); break;
                case 'V': out.push_back (Ph::V); break;
                case 'W': out.push_back (Ph::W); break;
                case 'X': out.push_back (Ph::K); out.push_back (Ph::S); break;
                case 'Z': out.push_back (Ph::Z); break;
                default: break; // Unbekanntes/Zahlen/Sonderzeichen -> ueberspringen
            }
            ++i;
        }
    }

    // Wandelt einen ganzen Satz in eine Phonem-Folge inkl. Pausen. Satzzeichen
    // werden zu (unterschiedlich langen) Pausen, alles andere zu Woertern.
    static std::vector<Ph> textToPhonemes (const juce::String& text)
    {
        std::vector<Ph> phs;
        juce::String word;
        auto flushWord = [&]
        {
            if (word.isNotEmpty())
            {
                if (! phs.empty())
                    phs.push_back (Ph::Sil); // kurze Wortluecke
                appendWord (word, phs);
                word.clear();
            }
        };
        for (int i = 0; i < text.length(); ++i)
        {
            const auto c = text[i];
            if (juce::CharacterFunctions::isLetterOrDigit (c))
                word += juce::String::charToString (c);
            else
            {
                flushWord();
                if (c == '.' || c == '!' || c == '?')
                {
                    phs.push_back (Ph::Sil);
                    phs.push_back (Ph::Sil); // laengere Satzpause
                }
                else if (c == ',' || c == ';' || c == ':')
                    phs.push_back (Ph::Sil);
            }
        }
        flushWord();
        return phs;
    }

    // ---------------------------------------------------------------------
    // Formant-Resonator: ein simples resonantes Bandpass-Filter (2-Pol),
    // per Mittenfrequenz + Bandbreite gestimmt (Regalia/Klatt-Bauart).
    struct Resonator
    {
        double a1 = 0.0, a2 = 0.0, gain = 1.0;
        double z1 = 0.0, z2 = 0.0;

        void set (double freqHz, double bwHz, double sampleRate)
        {
            const double r = std::exp (-juce::MathConstants<double>::pi * bwHz / sampleRate);
            const double theta = 2.0 * juce::MathConstants<double>::pi * freqHz / sampleRate;
            a1 = 2.0 * r * std::cos (theta);
            a2 = -r * r;
            gain = (1.0 - r); // grobe Normierung, damit die Resonanz nicht explodiert
        }

        float process (float x)
        {
            const double y = gain * x + a1 * z1 + a2 * z2;
            z2 = z1;
            z1 = y;
            return (float) y;
        }
    };

    // Einfacher, reproduzierbarer Rauschgenerator (LCG) - kein juce::Random noetig.
    struct Noise
    {
        uint32_t state = 12345;
        float next()
        {
            state = state * 1664525u + 1013904223u;
            return ((float) (state >> 8) / (float) 0x00FFFFFF) * 2.0f - 1.0f;
        }
    };

    // Rendert einen Text zu einer Mono-PCM-Bufferung. Liefert false bei leerem Text.
    static bool render (const juce::String& text, const Params& params, double sampleRate,
                        juce::AudioBuffer<float>& out)
    {
        auto phs = textToPhonemes (text);
        if (phs.empty())
            return false;

        const bool sam = params.character == Character::Sam;

        // Vorschau-Laenge grob abschaetzen, dann am Ende auf die echte Laenge kappen.
        double estMs = 200.0;
        for (auto p : phs) estMs += info (p).durMs;
        estMs = estMs / juce::jmax (0.3f, params.speed) + 200.0;
        out.setSize (1, (int) (estMs * 0.001 * sampleRate) + (int) sampleRate);
        out.clear();

        const float throatScale = 1.0f - params.throat * 0.28f;         // tief <-> hell
        const float f1Scale     = 0.72f + params.mouth * 0.56f;         // eng <-> weit

        Resonator r1, r2, r3;
        Noise noise;
        double phase = 0.0;      // Glottis-Phase 0..1 fuer stimmhafte Anregung
        double jitterPhase = 0.0;
        int writePos = 0;
        const float xfadeMs = 6.0f;
        const int xfadeSamples = (int) (xfadeMs * 0.001 * sampleRate);
        const double totalEstSamples = juce::jmax (1.0, estMs * 0.001 * sampleRate);

        // Formanten-Nachlauf (Amiga-Vorbild: "formant positions move fairly
        // smoothly as we speak") - ohne das springen f1/f2/f3 an JEDER
        // Phonem-Grenze hart auf den naechsten Zielwert, das klingt kantig/
        // schwerer verstaendlich. Ein sanftes Nachziehen (~12ms) glaettet
        // Uebergaenge zwischen Lauten, ohne Konsonanten zu verwaschen.
        double f1S = 0.0, f2S = 0.0, f3S = 0.0;
        bool formantSmoothInit = false;
        const double formantSmoothCoeff = 1.0 - std::exp (-1.0 / (0.012 * sampleRate));

        for (size_t idx = 0; idx < phs.size(); ++idx)
        {
            const Ph ph = phs[idx];
            const PhInfo& pi = info (ph);
            const bool isDiph = pi.type == PhType::Diphthong;
            const PhInfo& pi2 = isDiph ? info (pi.glideTo) : pi;

            double durMs = pi.durMs / juce::jmax (0.3f, params.speed);
            if (ph == Ph::Sil)
                durMs *= 1.0; // Pausen skalieren mit derselben Sprechgeschwindigkeit
            const int segLen = juce::jmax (1, (int) (durMs * 0.001 * sampleRate));

            if (writePos + segLen + xfadeSamples > out.getNumSamples())
                out.setSize (1, writePos + segLen + xfadeSamples + (int) sampleRate, true);
            auto* d = out.getWritePointer (0);

            for (int n = 0; n < segLen; ++n)
            {
                const float tNorm = (float) n / (float) juce::jmax (1, segLen - 1);
                float f1 = juce::jmap (tNorm, pi.f1, pi2.f1) * throatScale * f1Scale;
                float f2 = juce::jmap (tNorm, pi.f2, pi2.f2) * throatScale;
                float f3 = juce::jmap (tNorm, pi.f3, pi2.f3) * throatScale;
                f1 = juce::jlimit (80.0f, 1200.0f, f1);
                f2 = juce::jlimit (400.0f, 3200.0f, f2);
                f3 = juce::jlimit (1200.0f, 6000.0f, f3);

                if (! formantSmoothInit) { f1S = f1; f2S = f2; f3S = f3; formantSmoothInit = true; }
                else
                {
                    f1S += (f1 - f1S) * formantSmoothCoeff;
                    f2S += (f2 - f2S) * formantSmoothCoeff;
                    f3S += (f3 - f3S) * formantSmoothCoeff;
                }

                const float bwBase = sam ? 55.0f : 135.0f; // SAM eng/scharf-blechern, Narrator deutlich weicher/runder
                r1.set ((float) f1S, bwBase * pi.bw, sampleRate);
                r2.set ((float) f2S, (bwBase + 40.0f) * pi.bw, sampleRate);
                r3.set ((float) f3S, (bwBase + 80.0f) * pi.bw, sampleRate);

                float exc = 0.0f;
                if (pi.type != PhType::Silence && pi.voiced)
                {
                    // Stimmhafte Anregung: Pulszug an der Grundtonhoehe. SAM = schmaler,
                    // obertonreicher Puls (schrill), Narrator = runderer, weicherer Puls.
                    jitterPhase += 1.0;
                    const float jitter = sam ? (float) std::sin (jitterPhase * 0.017) * 0.02f
                                              : (float) std::sin (jitterPhase * 0.009) * 0.006f;
                    // Sanfte Tonhoehen-Kontur statt Dauerton (Amiga-Vorbild: Pitch
                    // faellt/steigt natuerlich mit der Satzposition statt monoton
                    // zu bleiben) - leichte Deklination ueber die ganze Phrase.
                    const double declineFrac = juce::jlimit (0.0, 1.0,
                        (double) (writePos + n) / totalEstSamples);
                    const float declinePitch = 1.0f - 0.12f * (float) declineFrac;
                    const double f0 = juce::jmax (40.0f, params.pitchHz * declinePitch * (1.0f + jitter));
                    phase += f0 / sampleRate;
                    if (phase >= 1.0) phase -= 1.0;
                    if (sam)
                        exc = (phase < 0.15) ? 1.0f : -0.15f; // schmaler Puls -> viele Obertoene
                    else
                        exc = (float) std::sin (phase * juce::MathConstants<double>::twoPi) * 0.6f
                            + (float) std::sin (phase * juce::MathConstants<double>::twoPi * 2.0) * 0.25f;
                }
                if (pi.type == PhType::Fricative || pi.type == PhType::Stop || ! pi.voiced)
                {
                    const float burst = (pi.type == PhType::Stop)
                        ? (tNorm < 0.5f ? 0.0f : 1.0f) // Verschluss dann Loesungs-Burst
                        : 1.0f;
                    exc += noise.next() * burst * (pi.voiced ? 0.35f : 0.8f);
                }

                float y = r1.process (exc) * 0.5f + r2.process (exc) * 0.3f + r3.process (exc) * 0.2f;
                y *= pi.amp;

                // Kurzes Fade an Segmentgrenzen -> keine Knackser beim Aneinanderreihen.
                if (n < xfadeSamples)      y *= (float) n / (float) xfadeSamples;
                if (n > segLen - xfadeSamples) y *= (float) (segLen - n) / (float) xfadeSamples;

                d[writePos + n] += y;
            }
            writePos += segLen;
        }

        out.setSize (1, juce::jmax (1, writePos), true);

        // Charakter-Faerbung am Schluss: SAM etwas blechern/gebittet, Narrator
        // dumpfer/weicher (wie die 22 kHz-Ausgabe des echten Amiga-narrator.device).
        auto* d = out.getWritePointer (0);
        const int N = out.getNumSamples();
        if (sam)
        {
            const float levels = 1024.0f; // ~10-Bit-Crunch
            for (int i = 0; i < N; ++i)
                d[i] = std::round (d[i] * levels) / levels;
        }
        else
        {
            float z = 0.0f;
            const float a = 0.55f; // simple Einpol-Tiefpass ~5 kHz bei 22 kHz SR
            for (int i = 0; i < N; ++i) { z += a * (d[i] - z); d[i] = z; }
        }

        // Pegel normalisieren.
        float peak = 0.001f;
        for (int i = 0; i < N; ++i) peak = juce::jmax (peak, std::abs (d[i]));
        const float norm = 0.85f / peak;
        for (int i = 0; i < N; ++i) d[i] *= norm;

        return true;
    }
}
