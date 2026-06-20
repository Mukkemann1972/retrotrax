#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"
#include "RetroLookAndFeel.h"
#include "Localisation.h"

// Bildschirm-Tastatur: zeigt, welche Computertaste welche Note ist (schwarze/weisse
// Klaviatur), so wie die zwei Tastenreihen im Tracker. Anklicken spielt die Note
// im aktuellen Instrument an - gut zum Ausprobieren, ohne die Belegung auswendig zu
// koennen. Overlay wie die anderen Panels; ESC schliesst.
class KeyboardPanel : public juce::Component,
                      private juce::Timer
{
public:
    explicit KeyboardPanel (RetroTraxProcessor& processor) : proc (processor)
    {
        setWantsKeyboardFocus (true);
        addAndMakeVisible (closeButton);
        closeButton.onClick = [this] { if (onClose) onClose(); };
    }

    std::function<void()> onClose;

    void applyLanguage()
    {
        closeButton.setButtonText (loc::t ("SCHLIESSEN", "CLOSE"));
        titleText = loc::t ("TASTATUR - welche Taste ist welche Note",
                            "KEYBOARD - which key is which note");
        repaint();
    }

    bool keyPressed (const juce::KeyPress& key) override
    {
        if (key.getKeyCode() == juce::KeyPress::escapeKey && onClose != nullptr)
        {
            onClose();
            return true;
        }
        const int off = charToOffset (key.getTextCharacter());
        if (off >= 0)
        {
            playKey (off);
            return true;
        }
        return false;
    }

    // Auch von aussen aufrufbar: Taste optisch+klanglich ausloesen (Buchstabe).
    void triggerChar (juce::juce_wchar c)
    {
        const int off = charToOffset (c);
        if (off >= 0) playKey (off);
    }

    void resized() override
    {
        closeButton.setBounds (getWidth() - 120, 10, 104, 26);

        auto area = getLocalBounds().reduced (24);
        area.removeFromTop (50);
        area.removeFromBottom (10);
        const float ww = area.getWidth() / 17.0f; // 17 weisse Tasten (2+ Oktaven)
        const float bw = ww * 0.62f;
        const float h  = (float) area.getHeight();
        int wi = 0;
        for (int o = 0; o < kKeys; ++o)
        {
            if (isWhite (o))
            {
                keyRect[o] = { area.getX() + wi * ww, (float) area.getY(), ww, h };
                ++wi;
            }
            else
            {
                keyRect[o] = { area.getX() + wi * ww - bw * 0.5f, (float) area.getY(), bw, h * 0.62f };
            }
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        // Schwarze Tasten liegen oben drauf -> zuerst pruefen.
        for (int pass = 0; pass < 2; ++pass)
            for (int o = 0; o < kKeys; ++o)
                if (isWhite (o) == (pass == 1) && keyRect[o].contains (e.position))
                {
                    playKey (o);
                    return;
                }
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (rt::bg.withAlpha (0.95f));
        g.setColour (rt::steel.withAlpha (0.5f));
        g.drawRect (getLocalBounds(), 1);
        g.setColour (rt::cursor);
        g.setFont (rt::mono (16.0f, true));
        g.drawText (titleText, 16, 10, getWidth() - 160, 28, juce::Justification::centredLeft);

        // weisse Tasten zuerst, dann schwarze obendrauf.
        for (int o = 0; o < kKeys; ++o)
            if (isWhite (o))
            {
                g.setColour (o == activeKey ? rt::cursor : juce::Colour (0xfff0f3fa));
                g.fillRect (keyRect[o].reduced (1.0f));
                g.setColour (rt::steel.withAlpha (0.6f));
                g.drawRect (keyRect[o].reduced (1.0f), 1.0f);
                g.setColour (juce::Colour (0xff20242e));
                g.setFont (rt::mono (13.0f, true));
                g.drawText (keyChar (o), keyRect[o].toNearestInt().removeFromBottom (22),
                            juce::Justification::centred);
            }
        for (int o = 0; o < kKeys; ++o)
            if (! isWhite (o))
            {
                g.setColour (o == activeKey ? rt::cursor.darker (0.2f) : juce::Colour (0xff14171d));
                g.fillRect (keyRect[o]);
                g.setColour (rt::cursor.withAlpha (0.8f));
                g.setFont (rt::mono (11.0f, true));
                g.drawText (keyChar (o), keyRect[o].toNearestInt().removeFromBottom (16),
                            juce::Justification::centred);
            }
    }

private:
    static constexpr int kKeys = 29; // Halbtoene 0..28 (wie noteOffsetForChar)

    // Eine Taste spielen + kurz aufleuchten lassen (vom Klick oder per Tastendruck).
    void playKey (int off)
    {
        if (off < 0 || off >= kKeys) return;
        const int note = juce::jlimit (0, 119, proc.currentOctave.load() * 12 + off);
        double srate = proc.getSampleRate(); if (srate <= 0.0) srate = 44100.0;
        proc.engine.audition (note, proc.currentInstrument.load(), (int) (0.7 * srate));
        activeKey = off;
        startTimer (200); // Aufleuchten kurz halten, dann wieder loeschen
        repaint();
    }
    void timerCallback() override { activeKey = -1; stopTimer(); repaint(); }

    // Computertaste -> Halbton-Offset 0..28 (gleiche Belegung wie im Pattern-Grid).
    static int charToOffset (juce::juce_wchar c)
    {
        switch (c)
        {
            case 'y': case 'Y': return 0;  case 's': case 'S': return 1;  case 'x': case 'X': return 2;
            case 'd': case 'D': return 3;  case 'c': case 'C': return 4;  case 'v': case 'V': return 5;
            case 'g': case 'G': return 6;  case 'b': case 'B': return 7;  case 'h': case 'H': return 8;
            case 'n': case 'N': return 9;  case 'j': case 'J': return 10; case 'm': case 'M': return 11;
            case 'q': case 'Q': return 12; case '2': return 13; case 'w': case 'W': return 14;
            case '3': return 15; case 'e': case 'E': return 16; case 'r': case 'R': return 17;
            case '5': return 18; case 't': case 'T': return 19; case '6': return 20;
            case 'z': case 'Z': return 21; case '7': return 22; case 'u': case 'U': return 23;
            case 'i': case 'I': return 24; case '9': return 25; case 'o': case 'O': return 26;
            case '0': return 27; case 'p': case 'P': return 28;
            default: return -1;
        }
    }

    static bool isWhite (int o)
    {
        switch (o % 12) { case 1: case 3: case 6: case 8: case 10: return false; default: return true; }
    }
    static juce::String keyChar (int o)
    {
        static const char* k[kKeys] = { "Y","S","X","D","C","V","G","B","H","N","J","M",
                                        "Q","2","W","3","E","R","5","T","6","Z","7","U",
                                        "I","9","O","0","P" };
        return (o >= 0 && o < kKeys) ? juce::String (k[o]) : juce::String();
    }

    RetroTraxProcessor& proc;
    juce::TextButton closeButton { "SCHLIESSEN" };
    juce::Rectangle<float> keyRect[kKeys];
    juce::String titleText;
    int activeKey = -1; // gerade aufleuchtende Taste

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KeyboardPanel)
};
