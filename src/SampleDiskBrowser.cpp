#include "SampleDiskBrowser.h"
#include "STDiskIndex.h"

// Archive.org liefert einzelne Dateien direkt aus dem ZIP aus; der Pfad
// innerhalb des ZIPs wird dabei %2F-kodiert an die ZIP-URL angehaengt.
static const char* const kArchiveBase =
    "https://archive.org/download/AmigaSTXX_originals_plus_conversions/ST-XX.zip/";

SampleDiskBrowser::SampleDiskBrowser (RetroTraxProcessor& p) : proc (p)
{
    for (int i = 0; i < stdisk::kNumDisks; ++i)
    {
        juce::StringArray parts = juce::StringArray::fromTokens (stdisk::kIndex[i], "|", {});
        if (parts.size() < 2)
            continue;
        diskNames.add (parts[0]);
        parts.remove (0);
        diskSamples.add (parts);
    }

    for (auto* lb : { &diskList, &sampleList })
    {
        lb->setColour (juce::ListBox::backgroundColourId, rt::rowBeat);
        lb->setColour (juce::ListBox::outlineColourId, rt::steel.withAlpha (0.5f));
        lb->setOutlineThickness (1);
        lb->setRowHeight (20);
        addAndMakeVisible (*lb);
    }

    addAndMakeVisible (loadButton);
    addAndMakeVisible (closeButton);
    addAndMakeVisible (statusLabel);

    loadButton.onClick  = [this] { loadSelected(); };
    closeButton.onClick = [this] { if (onClose) onClose(); };

    setWantsKeyboardFocus (true);

    statusLabel.setFont (rt::mono (13.0f));
    setStatus ("Diskette und Sample waehlen, dann IN SLOT LADEN (oder Doppelklick). ESC schliesst.");

    diskList.updateContent();
    diskList.selectRow (0);
}

SampleDiskBrowser::~SampleDiskBrowser()
{
    task.reset(); // laufenden Download abbrechen, bevor der Listener stirbt
}

// ---- Listen-Modelle -------------------------------------------------------

int SampleDiskBrowser::Model::getNumRows()
{
    return isDiskList ? owner.diskNames.size()
                      : owner.diskSamples[owner.currentDisk].size();
}

void SampleDiskBrowser::Model::paintListBoxItem (int row, juce::Graphics& g,
                                                 int w, int h, bool selected)
{
    if (selected)
        g.fillAll (rt::playBar);

    juce::String text;
    if (isDiskList)
    {
        text = owner.diskNames[row];
    }
    else
    {
        const auto name = owner.diskSamples[owner.currentDisk][row];
        text = name;
        if (owner.cacheFileFor (owner.currentDisk, name).existsAsFile())
            text << "  *"; // schon lokal vorhanden -> laedt sofort
    }

    g.setFont (rt::mono (13.0f));
    g.setColour (selected ? juce::Colours::white : rt::text);
    g.drawText (text, 8, 0, w - 12, h, juce::Justification::centredLeft);
}

void SampleDiskBrowser::Model::selectedRowsChanged (int row)
{
    if (isDiskList && row >= 0)
        owner.diskSelected (row);
}

void SampleDiskBrowser::Model::listBoxItemDoubleClicked (int row, const juce::MouseEvent&)
{
    if (! isDiskList)
    {
        owner.sampleList.selectRow (row);
        owner.loadSelected();
    }
}

// ---- Logik ----------------------------------------------------------------

void SampleDiskBrowser::diskSelected (int index)
{
    currentDisk = juce::jlimit (0, diskNames.size() - 1, index);
    sampleList.deselectAllRows();
    sampleList.updateContent();
    sampleList.scrollToEnsureRowIsOnscreen (0);
    sampleList.repaint();
}

juce::File SampleDiskBrowser::cacheFileFor (int diskIdx, const juce::String& sampleName) const
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
        .getChildFile ("MukkemannRetroTrax").getChildFile ("ST-Disks")
        .getChildFile (diskNames[diskIdx]).getChildFile (sampleName + ".aiff");
}

juce::URL SampleDiskBrowser::urlFor (int diskIdx, const juce::String& sampleName) const
{
    const auto esc = [] (const juce::String& s)
    {
        return juce::URL::addEscapeChars (s, false);
    };
    return juce::URL (juce::String (kArchiveBase)
                      + "ST-XX%2F" + esc (diskNames[diskIdx])
                      + "%2F" + esc (sampleName) + ".aiff");
}

void SampleDiskBrowser::loadSelected()
{
    if (task != nullptr)
        return; // es laeuft schon ein Download

    const int row = sampleList.getSelectedRow();
    if (row < 0)
    {
        setStatus ("Bitte erst ein Sample auswaehlen.", true);
        return;
    }

    pendingDisk   = currentDisk;
    pendingSample = diskSamples[currentDisk].getReference (row);

    const auto target = cacheFileFor (pendingDisk, pendingSample);
    if (target.existsAsFile())
    {
        finishLoad (target, true);
        return;
    }

    target.getParentDirectory().createDirectory();
    setStatus ("Lade " + pendingSample + " von archive.org ...");
    loadButton.setEnabled (false);

    task = urlFor (pendingDisk, pendingSample)
               .downloadToFile (target, juce::URL::DownloadTaskOptions().withListener (this));

    if (task == nullptr)
    {
        loadButton.setEnabled (true);
        setStatus ("Download konnte nicht gestartet werden (Internet?).", true);
    }
}

void SampleDiskBrowser::finished (juce::URL::DownloadTask* t, bool success)
{
    // Kommt vom Download-Thread -> auf den Message-Thread wechseln.
    const auto file = t->getTargetLocation();
    const bool ok   = success && ! t->hadError();

    juce::MessageManager::callAsync (
        [sp = juce::Component::SafePointer<SampleDiskBrowser> (this), file, ok]
        {
            if (sp != nullptr)
            {
                sp->task.reset();
                sp->loadButton.setEnabled (true);
                sp->finishLoad (file, ok);
            }
        });
}

void SampleDiskBrowser::finishLoad (const juce::File& file, bool success)
{
    if (! success || file.getSize() < 64)
    {
        file.deleteFile(); // halbe Downloads nicht im Cache lassen
        setStatus ("Download fehlgeschlagen — Internetverbindung pruefen.", true);
        return;
    }

    const int slot = proc.currentInstrument.load();
    if (! proc.loadInstrument (slot, file))
    {
        setStatus ("Konnte \"" + pendingSample + "\" nicht laden.", true);
        return;
    }

    proc.engine.audition (60, slot); // gleich anspielen (C-5 = Originaltonhoehe)
    sampleList.repaint();            // Cache-Sternchen aktualisieren
    setStatus (diskNames[pendingDisk] + "/" + pendingSample
               + " in Slot " + juce::String::formatted ("%02d", slot + 1) + " geladen.");

    if (onSampleLoaded)
        onSampleLoaded (pendingSample, slot);
}

void SampleDiskBrowser::setStatus (const juce::String& text, bool warn)
{
    statusLabel.setColour (juce::Label::textColourId, warn ? rt::cursor : rt::textDim);
    statusLabel.setText (text, juce::dontSendNotification);
}

bool SampleDiskBrowser::keyPressed (const juce::KeyPress& key)
{
    if (key.getKeyCode() == juce::KeyPress::escapeKey && onClose != nullptr)
    {
        onClose();
        return true;
    }
    return false;
}

// ---- Optik ----------------------------------------------------------------

void SampleDiskBrowser::paint (juce::Graphics& g)
{
    g.fillAll (rt::bg);
    g.setColour (rt::steel.withAlpha (0.7f));
    g.drawRect (getLocalBounds(), 1);

    g.setFont (rt::mono (15.0f, true));
    g.setColour (rt::cursor);
    g.drawText ("ST-XX SAMPLE-DISKS", 12, 6, getWidth() - 24, 20, juce::Justification::centredLeft);

    g.setFont (rt::mono (12.0f));
    g.setColour (rt::textDim);
    g.drawText ("Original-Amiga-Sounds, Public Domain (archive.org) — * = schon heruntergeladen",
                12, 26, getWidth() - 24, 16, juce::Justification::centredLeft);
}

void SampleDiskBrowser::resized()
{
    auto area = getLocalBounds().reduced (10);
    area.removeFromTop (38); // Titel (nur paint)

    auto bottom = area.removeFromBottom (30);
    closeButton.setBounds (bottom.removeFromRight (110).reduced (0, 2));
    bottom.removeFromRight (8);
    loadButton.setBounds (bottom.removeFromRight (140).reduced (0, 2));
    statusLabel.setBounds (bottom);

    area.removeFromBottom (6);
    diskList.setBounds (area.removeFromLeft (110));
    area.removeFromLeft (8);
    sampleList.setBounds (area);
}
