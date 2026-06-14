#pragma once

#include "RetroLookAndFeel.h"
#include "Localisation.h"

// Eingebaute Hilfe als Overlay (wie der Sample-Browser). Links die Themen,
// rechts der erklaerte Text - automatisch in der gewaehlten Sprache.
//
// Die Inhalte stehen gebuendelt in rebuild() (HelpPanel.cpp): ein neues
// Feature bekommt dort einen neuen Eintrag, dann waechst die Hilfe mit.
class HelpPanel : public juce::Component
{
public:
    HelpPanel();

    std::function<void()> onClose;

    // Nach einem Sprachwechsel aufrufen: Themen + Text neu aufbauen.
    void applyLanguage();

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;

private:
    struct Topic
    {
        juce::String title;
        juce::String body;
    };

    struct Model : juce::ListBoxModel
    {
        HelpPanel& owner;
        explicit Model (HelpPanel& o) : owner (o) {}
        int getNumRows() override;
        void paintListBoxItem (int row, juce::Graphics&, int w, int h, bool selected) override;
        void selectedRowsChanged (int row) override;
    };

    void rebuild();
    void showTopic (int index);

    juce::Array<Topic> topics;
    int currentTopic = 0;

    Model model { *this };
    juce::ListBox topicList { "help-topics", &model };
    juce::TextEditor bodyView;
    juce::TextButton closeButton { "SCHLIESSEN" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HelpPanel)
};
