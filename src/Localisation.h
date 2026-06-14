#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

// Zweisprachigkeit (Deutsch/Englisch) an EINER Stelle.
//
// Jeder sichtbare Text wird als loc::t ("deutsch", "english") geschrieben —
// so steht jede Uebersetzung direkt am Verwendungsort und kann nicht
// vergessen werden. Die gewaehlte Sprache wird gespeichert; beim allerersten
// Start richtet sie sich nach der Systemsprache (Deutsch sonst Englisch).
namespace loc
{
    enum class Lang { DE, EN };

    inline Lang& currentRef() { static Lang l = Lang::DE; return l; }
    inline Lang  current()    { return currentRef(); }

    inline juce::File settingsFile()
    {
        return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
            .getChildFile ("MukkemannRetroTrax").getChildFile ("sprache.txt");
    }

    // Einmal beim Start aufrufen: gespeicherte Sprache laden, sonst Systemsprache.
    inline void load()
    {
        const auto f = settingsFile();
        if (f.existsAsFile())
        {
            currentRef() = f.loadFileAsString().trim().equalsIgnoreCase ("EN") ? Lang::EN : Lang::DE;
        }
        else
        {
            const auto sys = juce::SystemStats::getUserLanguage(); // z.B. "de", "en"
            currentRef() = sys.startsWithIgnoreCase ("de") ? Lang::DE : Lang::EN;
        }
    }

    inline void setCurrent (Lang l)
    {
        currentRef() = l;
        const auto f = settingsFile();
        f.getParentDirectory().createDirectory();
        f.replaceWithText (l == Lang::EN ? "EN" : "DE");
    }

    inline void toggle()
    {
        setCurrent (current() == Lang::DE ? Lang::EN : Lang::DE);
    }

    // Kuerzel der AKTUELLEN Sprache (fuer den Umschalt-Knopf).
    inline juce::String code() { return current() == Lang::DE ? "DE" : "EN"; }

    inline juce::String t (const juce::String& de, const juce::String& en)
    {
        return current() == Lang::DE ? de : en;
    }
}
