#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "BinaryData.h"

// Start-Splash: zeigt beim Programmstart kurz das RetroTrax-Logo (fest eingebettet),
// haelt ein paar Sekunden und blendet dann weg - oder sofort per Klick/Taste.
// Liegt ganz oben ueber dem Editor und gibt danach den Fokus zurueck.
class SplashOverlay : public juce::Component,
                      private juce::Timer
{
public:
    SplashOverlay()
    {
        logo = juce::ImageCache::getFromMemory (BinaryData::logo_png, BinaryData::logo_pngSize);
        setWantsKeyboardFocus (true);
        setInterceptsMouseClicks (true, true);
        startTimerHz (30);
    }

    std::function<void()> onDone; // Editor blendet aus + holt den Fokus zurueck

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colour (0xff05070c).withAlpha (alpha)); // fast schwarz
        if (logo.isValid())
        {
            auto area = getLocalBounds().toFloat().reduced (getWidth() * 0.07f, getHeight() * 0.10f);
            const float iw = (float) logo.getWidth(), ih = (float) logo.getHeight();
            if (iw > 0 && ih > 0)
            {
                const float scale = juce::jmin (area.getWidth() / iw, area.getHeight() / ih);
                const float w = iw * scale, h = ih * scale;
                const juce::Rectangle<float> dst (area.getCentreX() - w * 0.5f,
                                                  area.getCentreY() - h * 0.5f, w, h);
                g.setOpacity (alpha);
                g.drawImage (logo, dst);
                g.setOpacity (1.0f);
            }
        }
        g.setColour (juce::Colours::white.withAlpha (0.45f * alpha));
        g.setFont (juce::Font (juce::FontOptions (14.0f)));
        g.drawText (juce::String::fromUTF8 ("Klick oder Taste \xE2\x80\x93 los geht's"),
                    getLocalBounds().removeFromBottom (34), juce::Justification::centred);
    }

    void mouseDown (const juce::MouseEvent&) override { dismiss(); }
    bool keyPressed (const juce::KeyPress&) override  { dismiss(); return true; }

private:
    void timerCallback() override
    {
        if (holdTicks > 0) { --holdTicks; return; } // erst halten ...
        alpha -= 0.06f;                              // ... dann sanft ausblenden
        if (alpha <= 0.0f) { alpha = 0.0f; dismiss(); return; }
        repaint();
    }

    void dismiss()
    {
        stopTimer();
        setVisible (false);
        if (onDone) onDone();
    }

    juce::Image logo;
    float alpha = 1.0f;
    int   holdTicks = 70; // ~2,3 s bei 30 Hz halten, dann ausblenden

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SplashOverlay)
};
