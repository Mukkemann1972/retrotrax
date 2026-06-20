#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <cmath>
#include "PluginProcessor.h"
#include "RetroLookAndFeel.h"
#include "Localisation.h"

// Spektrum-Anzeige: die "tanzenden Frequenzbalken" aus den alten Trackern/Demos.
// Reine Optik - additiv und abschaltbar; greift nur den Ringpuffer im Processor
// ab (kein Eingriff in den Klang). Die Magnitude pro Band kommt aus einem
// schlanken Goertzel-Filter (selbstgebaut, keine zusaetzliche FFT-Abhaengigkeit).
class SpectrumPanel : public juce::Component,
                      private juce::Timer
{
public:
    explicit SpectrumPanel (RetroTraxProcessor& processor) : proc (processor)
    {
        setWantsKeyboardFocus (true);
        addAndMakeVisible (closeButton);
        closeButton.onClick = [this] { if (onClose) onClose(); };
    }

    std::function<void()> onClose; // Editor blendet die Anzeige dann wieder aus

    bool keyPressed (const juce::KeyPress& key) override
    {
        if (key.getKeyCode() == juce::KeyPress::escapeKey && onClose != nullptr)
        {
            onClose();
            return true;
        }
        return false;
    }

    void applyLanguage()
    {
        closeButton.setButtonText (loc::t ("SCHLIESSEN", "CLOSE"));
    }

    // Timer nur laufen lassen, solange die Anzeige sichtbar ist (spart Last).
    void visibilityChanged() override
    {
        if (isVisible()) startTimerHz (30);
        else             stopTimer();
    }

    void resized() override
    {
        closeButton.setBounds (getWidth() - 120, 10, 104, 26);
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds();
        g.setColour (rt::bg.withAlpha (0.93f));
        g.fillRect (r);
        g.setColour (rt::steel.withAlpha (0.5f));
        g.drawRect (r, 1);

        g.setColour (rt::cursor);
        g.setFont (rt::mono (16.0f, true));
        g.drawText (loc::t ("SPEKTRUM", "SPECTRUM"), 14, 8, 280, 28,
                    juce::Justification::centredLeft);

        auto area = r.reduced (14, 0);
        area.removeFromTop (44);
        area.removeFromBottom (16);
        // Untere ~90 px: VU-Pegel PRO SPUR (wie die Kanal-Meter alter Tracker).
        auto trackStrip = area.removeFromBottom (94);
        area.removeFromBottom (10);
        drawTrackMeters (g, trackStrip);
        const int   n   = kBands;
        const float gap = 3.0f;
        const float bw  = (area.getWidth() - gap * (n - 1)) / (float) n;
        const float baseY = (float) area.getBottom();
        const float maxH  = (float) area.getHeight();

        for (int i = 0; i < n; ++i)
        {
            const float t = juce::jlimit (0.0f, 1.0f, bars[i]);
            const float h = t * maxH;
            const float x = area.getX() + i * (bw + gap);
            // Farbverlauf gruen -> gelb -> rot je nach Hoehe (klassischer Analyzer).
            const juce::Colour col = juce::Colour::fromHSV (juce::jmap (t, 0.38f, 0.0f),
                                                            0.85f, 0.95f, 1.0f);
            g.setColour (col);
            g.fillRect (x, baseY - h, bw, h);
            // Peak-Kappe, faellt langsam nach.
            const float ph = juce::jlimit (0.0f, 1.0f, peaks[i]) * maxH;
            g.setColour (juce::Colours::white.withAlpha (0.8f));
            g.fillRect (x, baseY - ph - 2.0f, bw, 2.0f);
        }
    }

private:
    static constexpr int kBands = 32;
    static constexpr int kFFT   = 1024; // Analysefenster (Zweierpotenz)

    // 16 kleine VU-Balken, einer pro Spur - "Spektrum auf die Spuren verteilt".
    void drawTrackMeters (juce::Graphics& g, juce::Rectangle<int> area)
    {
        g.setColour (rt::textDim);
        g.setFont (rt::mono (11.0f, true));
        g.drawText (loc::t ("PEGEL PRO SPUR", "LEVEL PER TRACK"),
                    area.removeFromTop (14), juce::Justification::centredLeft);
        const int n = TrackerEngine::kTracks;
        const float gap = 3.0f;
        const float bw  = (area.getWidth() - gap * (n - 1)) / (float) n;
        const float baseY = (float) area.getBottom() - 12.0f; // Platz fuer die Nummer
        const float maxH  = (float) area.getHeight() - 14.0f;
        for (int t = 0; t < n; ++t)
        {
            const float lvl = juce::jlimit (0.0f, 1.0f, trk[t]);
            const float x = area.getX() + t * (bw + gap);
            g.setColour (rt::rowBeat);
            g.fillRect (x, baseY - maxH, bw, maxH); // Hintergrund-Schiene
            g.setColour (rt::instColour (t).withAlpha (0.5f + 0.5f * lvl));
            g.fillRect (x, baseY - lvl * maxH, bw, lvl * maxH);
            g.setColour (rt::textDim);
            g.setFont (rt::mono (9.0f, false));
            g.drawText (juce::String (t + 1), (int) x, (int) baseY + 1, (int) bw, 11,
                        juce::Justification::centred);
        }
    }

    void timerCallback() override
    {
        const float sr = (float) juce::jmax (8000.0, proc.getSampleRate());

        // Juengste kFFT Samples aus dem Ringpuffer holen, Hann-Fenster gegen Leakage.
        float buf[kFFT];
        const int end = proc.scopePos.load (std::memory_order_relaxed);
        for (int i = 0; i < kFFT; ++i)
        {
            const int idx = (end - kFFT + i) & (RetroTraxProcessor::kScopeSize - 1);
            const float w = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi
                                                    * (float) i / (float) (kFFT - 1));
            buf[i] = proc.scope[idx] * w;
        }

        // Log-verteilte Bandmitten 45 Hz .. ~16 kHz.
        const float fLo = 45.0f;
        const float fHi = juce::jmin (16000.0f, sr * 0.45f);
        for (int b = 0; b < kBands; ++b)
        {
            const float frac = b / (float) (kBands - 1);
            const float freq = fLo * std::pow (fHi / fLo, frac);
            const float mag  = goertzel (buf, kFFT, freq, sr);
            // in dB und auf 0..1 abbilden (Rauschboden ~ -78 dB).
            const float db = 20.0f * std::log10 (mag + 1.0e-7f);
            float level = juce::jlimit (0.0f, 1.0f, (db + 78.0f) / 78.0f);
            // hohe Baender klingen leiser -> leicht anheben fuer ein angenehmes Bild.
            level = juce::jlimit (0.0f, 1.0f, level * (0.75f + 0.5f * frac));

            // Balken springt schnell hoch, faellt langsam (lebendige Optik).
            bars[b]  = juce::jmax (level, bars[b] * 0.78f);
            peaks[b] = juce::jmax (bars[b], peaks[b] - 0.02f);
        }
        // VU pro Spur: schnell hoch, langsam runter (lebendige Meter).
        for (int t = 0; t < TrackerEngine::kTracks; ++t)
        {
            const float lvl = juce::jlimit (0.0f, 1.0f, proc.engine.trackLevel[t].load (std::memory_order_relaxed) * 1.4f);
            trk[t] = juce::jmax (lvl, trk[t] * 0.80f);
        }
        repaint();
    }

    // Goertzel: Magnitude einer einzelnen Frequenz ueber N Samples. Schlank und
    // genau genug fuer eine Balkenanzeige, ohne eine FFT-Bibliothek einzubinden.
    static float goertzel (const float* x, int N, float freq, float sr)
    {
        const float k     = freq * (float) N / sr;
        const float w     = juce::MathConstants<float>::twoPi * k / (float) N;
        const float coeff = 2.0f * std::cos (w);
        float s1 = 0.0f, s2 = 0.0f;
        for (int i = 0; i < N; ++i)
        {
            const float s0 = x[i] + coeff * s1 - s2;
            s2 = s1;
            s1 = s0;
        }
        const float power = s1 * s1 + s2 * s2 - coeff * s1 * s2;
        return std::sqrt (juce::jmax (0.0f, power)) / ((float) N * 0.5f);
    }

    RetroTraxProcessor& proc;
    juce::TextButton closeButton { "SCHLIESSEN" };
    float bars[kBands]  = {};
    float peaks[kBands] = {};
    float trk[TrackerEngine::kTracks] = {}; // geglaettete VU-Pegel pro Spur

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SpectrumPanel)
};
