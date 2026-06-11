#pragma once

#include "PluginProcessor.h"
#include "RetroLookAndFeel.h"

// Browser fuer die klassischen Amiga-Sample-Disketten ST-01 bis ST-XX.
// Der Katalog (Disk- und Sample-Namen) ist fest eingebaut (STDiskIndex.h),
// die Audiodaten werden erst beim Laden einzeln von archive.org geholt
// (Public Domain) und lokal gecacht — beim zweiten Mal geht es offline.
class SampleDiskBrowser : public juce::Component,
                          private juce::URL::DownloadTaskListener
{
public:
    explicit SampleDiskBrowser (RetroTraxProcessor&);
    ~SampleDiskBrowser() override;

    // Wird nach erfolgreichem Laden gerufen (Sample-Name, Slot-Index).
    std::function<void (const juce::String&, int)> onSampleLoaded;
    std::function<void()> onClose;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    // Ein ListBoxModel pro Liste; beide malen im ProTracker-Stil.
    struct Model : juce::ListBoxModel
    {
        SampleDiskBrowser& owner;
        bool isDiskList;
        Model (SampleDiskBrowser& o, bool disks) : owner (o), isDiskList (disks) {}

        int getNumRows() override;
        void paintListBoxItem (int row, juce::Graphics&, int w, int h, bool selected) override;
        void selectedRowsChanged (int row) override;
        void listBoxItemDoubleClicked (int row, const juce::MouseEvent&) override;
    };

    void diskSelected (int index);
    void loadSelected();
    void finished (juce::URL::DownloadTask*, bool success) override;
    void finishLoad (const juce::File& file, bool success);
    void setStatus (const juce::String& text, bool warn = false);

    juce::File cacheFileFor (int diskIdx, const juce::String& sampleName) const;
    juce::URL  urlFor (int diskIdx, const juce::String& sampleName) const;

    RetroTraxProcessor& proc;

    juce::StringArray diskNames;
    juce::Array<juce::StringArray> diskSamples;
    int currentDisk = 0;

    Model diskModel   { *this, true };
    Model sampleModel { *this, false };
    juce::ListBox diskList   { "disks",   &diskModel };
    juce::ListBox sampleList { "samples", &sampleModel };

    juce::TextButton loadButton  { "IN SLOT LADEN" };
    juce::TextButton closeButton { "SCHLIESSEN" };
    juce::Label statusLabel;

    std::unique_ptr<juce::URL::DownloadTask> task;
    juce::String pendingSample;
    int pendingDisk = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SampleDiskBrowser)
};
