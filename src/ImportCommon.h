#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>

// Gemeinsame, neutrale Zwischenstruktur fuer Modul-Importer (S3M, IT, ...): die
// Parser fuellen sie nur, das Einsetzen in die Engine macht der Processor
// (applyImportedSong). Gleicher pragmatischer Geist wie ModImport/XmImport:
// pro Slot ein Sample, nur Effekte mit gleicher Tracker-Nummerierung, Patterns
// auf 64 Zeilen / 16 Spuren gekuerzt.
namespace ImportCommon
{
    struct Sample
    {
        juce::String name;
        juce::AudioBuffer<float> data; // mono, -1..1
        double sourceRate = 8363.0;    // Abspielrate bei Note 60 (C-5)
    };

    struct Cell { int note = -1, instrument = -1, volume = -1, effect = -1, effectParam = 0; };

    struct Song
    {
        bool ok = false;
        juce::String title;
        int channels = 4;
        int numPatterns = 0;
        int songLength = 1;
        std::vector<int> order;                               // Pattern-Indizes
        std::vector<Sample> samples;                          // ein Eintrag je Slot
        std::vector<std::vector<std::vector<Cell>>> patterns; // [pattern][row][channel]
        juce::String message;
    };

    constexpr int kNoteOff = -2; // gleich wie TrackerEngine::kNoteOff

    inline juce::uint32 le16 (const juce::uint8* p) { return (juce::uint32) (p[0] | (p[1] << 8)); }
    inline juce::uint32 le32 (const juce::uint8* p)
    {
        return (juce::uint32) (p[0] | (p[1] << 8) | (p[2] << 16) | ((juce::uint32) p[3] << 24));
    }

    // Nur Effekte uebernehmen, die der Tracker mit gleicher Nummer kennt
    // (0=Arpeggio, 1/2=Slides, 3=Porta, 4=Vibrato, A=Vol-Slide, F=Speed/Tempo).
    inline void mapTrackerEffect (int fx, int fxp, Cell& cell)
    {
        if      (fx == 0x0 && fxp != 0)   { cell.effect = 0x0; cell.effectParam = fxp; }
        else if (fx == 0x1 || fx == 0x2 || fx == 0x3 || fx == 0x4
              || fx == 0xA || fx == 0xF)  { cell.effect = fx;  cell.effectParam = fxp; }
    }
}
