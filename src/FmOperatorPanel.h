#pragma once

#include <array>
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"
#include "RetroLookAndFeel.h"
#include "Localisation.h"

// Voller Editor fuer die vier FM-Operatoren (Ratio/Level/ADSR je Operator) +
// Algorithmus/Rueckkopplung. Ergaenzt die schlanken Makro-Regler im SynthPanel
// (dort reichen Werksklaenge + drei Regler fuer den Alltag) - wer wirklich an
// den 26 Einzelwerten schrauben will, oeffnet dieses Panel ueber den
// "OPERATOREN"-Knopf im SynthPanel, wenn FM als Klangmotor gewaehlt ist.
// Liegt wie alle anderen Overlays direkt ueber dem Pattern-Grid.
class FmOperatorPanel : public juce::Component
{
public:
    explicit FmOperatorPanel (RetroTraxProcessor& processor);

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;

    void applyLanguage();
    void refresh(); // Regler aus dem aktuellen Slot fuellen (Slot wird hier gemerkt)

    std::function<void()> onClose;
    std::function<void()> onChanged; // fuer den Instrument-Namen im Hauptfenster

private:
    static constexpr int kOps = TrackerEngine::Instrument::kFmOps; // 4 Operatoren

    void writeAlgoFeedback();          // Algorithmus + Rueckkopplung in den Slot schreiben
    void writeOperator (int op);       // Ratio/Level/ADSR eines Operators schreiben
    void updateAlgoText();             // Beschreibungstext + TRAEGER/MODULATOR-Tags
    void previewNote();                // ein C-5 mit automatischem Note-Aus

    RetroTraxProcessor& proc;
    int slot = 0;

    juce::Label titleLabel, slotLabel;

    juce::Label  algoLabel, fbLabel;
    juce::Slider algoSlider, fbSlider;
    juce::Label  algoTextLabel; // erklaert live, was der gewaehlte Algorithmus macht

    // Je Operator: Kopf (Nummer + TRAEGER/MODULATOR) + sechs Regler.
    struct OperatorUi
    {
        juce::Label headerLabel, tagLabel;
        juce::Label ratioLabel, levelLabel, aLabel, dLabel, sLabel, rLabel;
        juce::Slider ratioSlider, levelSlider, aSlider, dSlider, sSlider, rSlider;
    };
    std::array<OperatorUi, kOps> ops;

    juce::Label      hintLabel;
    juce::TextButton testButton  { "TEST" };
    juce::TextButton closeButton { "SCHLIESSEN" };

    bool loading = false; // true, waehrend refresh() die Regler setzt (keine Callbacks)

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FmOperatorPanel)
};
