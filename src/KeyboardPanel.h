#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"
#include "RetroLookAndFeel.h"
#include "Localisation.h"

// Bildschirm-Tastatur: zeigt, welche Computertaste welche Note ist (schwarze/weisse
// Klaviatur), so wie die zwei Tastenreihen im Tracker. Anklicken spielt die Note
// im aktuellen Instrument an - gut zum Ausprobieren, ohne die Belegung auswendig zu
// koennen. Overlay wie die anderen Panels; ESC schliesst.
class KeyboardPanel : public juce::Component
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
        return false;
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
                    const int note = juce::jlimit (0, 119, proc.currentOctave.load() * 12 + o);
                    double srate = proc.getSampleRate(); if (srate <= 0.0) srate = 44100.0;
                    proc.engine.audition (note, proc.currentInstrument.load(), (int) (1.0 * srate));
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
                g.setColour (juce::Colour (0xfff0f3fa));
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
                g.setColour (juce::Colour (0xff14171d));
                g.fillRect (keyRect[o]);
                g.setColour (rt::cursor.withAlpha (0.8f));
                g.setFont (rt::mono (11.0f, true));
                g.drawText (keyChar (o), keyRect[o].toNearestInt().removeFromBottom (16),
                            juce::Justification::centred);
            }
    }

private:
    static constexpr int kKeys = 29; // Halbtoene 0..28 (wie noteOffsetForChar)

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

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KeyboardPanel)
};
