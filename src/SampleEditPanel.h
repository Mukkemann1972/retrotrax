#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"
#include "RetroLookAndFeel.h"
#include "Localisation.h"

// Fairlight-Sample-Werkzeug: ein kleiner Wellenform-Editor fuer das Sample im
// aktuellen Slot. Bereich markieren und TRIMMEN, NORMALISIEREN, UMKEHREN, mit
// der Maus die Wellenform zeichnen (FREIHAND - das Lichtgriffel-Gefuehl des
// Fairlight CMI) und vor allem: das Sample IN KIT schneiden (16 Scheiben auf die
// Drum-Pads). Bearbeitet wird eine Arbeitskopie; UEBERNEHMEN sichert sie zurueck
// in den Slot (als WAV, bleibt im Song erhalten). Overlay wie das AKAI-Panel.
class SampleEditPanel : public juce::Component
{
public:
    explicit SampleEditPanel (RetroTraxProcessor& processor);

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    bool keyPressed (const juce::KeyPress&) override;

    void applyLanguage();
    void refresh(); // Arbeitskopie aus dem aktuellen Slot holen (oder leeren Puffer anlegen)

    std::function<void()> onClose;

private:
    void setHint (const juce::String& de, const juce::String& en);
    void drawAt (const juce::MouseEvent&);     // Freihand: Wellenwert unter der Maus setzen
    void selectAt (const juce::MouseEvent&);   // Bereich markieren (Ziehen)
    int  xToIndex (int x) const;               // Pixel-X -> Sample-Index
    float yToValue (int y) const;              // Pixel-Y -> Amplitude (-1..1)

    RetroTraxProcessor& proc;

    juce::AudioBuffer<float> work;  // Arbeitskopie
    double rate = 8363.0;
    int    slot = 0;

    juce::Rectangle<int> waveRect;
    double selStart = 0.0, selEnd = 0.0; // Auswahl als Bruchteile 0..1
    bool   hasSel = false;
    bool   freehand = false;
    int    lastDrawIdx = -1;

    juce::Label titleLabel;

    // Time-Stretch: Faktor 0.5..2.0, DEHNEN wendet ihn auf die Arbeitskopie an.
    juce::Label      stretchLabel;
    juce::Slider     stretchSlider;
    juce::TextButton stretchButton { "DEHNEN" };

    juce::TextButton trimButton   { "TRIMMEN" };
    juce::TextButton normButton   { "NORMAL." };
    juce::TextButton revButton    { "UMKEHREN" };
    juce::TextButton drawButton   { "FREIHAND" };
    juce::TextButton chopButton   { "IN KIT (16)" };
    juce::TextButton previewButton{ "VORHOEREN" };
    juce::TextButton applyButton  { "UEBERNEHMEN" };
    juce::TextButton closeButton  { "SCHLIESSEN" };
    juce::Label hintLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SampleEditPanel)
};
