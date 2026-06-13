#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "PatternGrid.h"
#include "RetroLookAndFeel.h"
#include "SampleDiskBrowser.h"

class RetroTraxEditor : public juce::AudioProcessorEditor
{
public:
    explicit RetroTraxEditor (RetroTraxProcessor&);
    ~RetroTraxEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void refreshInstrumentBox();
    void updateTransportButtons();
    void loadSampleClicked();

    RetroTraxProcessor& proc;
    RetroLookAndFeel lnf;

    juce::TextButton playButton { "PLAY" };
    juce::TextButton stopButton { "STOP" };
    juce::TextButton loadButton { "SAMPLE LADEN" };
    juce::TextButton stDisksButton { "SAMPLES" };
    juce::Slider bpmSlider;
    juce::ComboBox instrumentBox;
    juce::ComboBox octaveBox;
    juce::Label instLabel { {}, "INSTR" };
    juce::Label octLabel { {}, "OKTAVE" };
    juce::Label hintLabel;

    // Kleines Farbquadrat: zeigt die Farbe des gerade gewaehlten Instruments
    struct ColourDot : juce::Component
    {
        juce::Colour colour;
        void paint (juce::Graphics& g) override
        {
            const auto r = getLocalBounds().toFloat().reduced (2.0f);
            g.setColour (colour);
            g.fillRoundedRectangle (r, 4.0f);
            g.setColour (juce::Colour (0xff0e1118));
            g.drawRoundedRectangle (r, 4.0f, 1.2f);
        }
    };
    ColourDot instDot;

    PatternGrid grid;
    SampleDiskBrowser diskBrowser;
    std::unique_ptr<juce::FileChooser> chooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RetroTraxEditor)
};
