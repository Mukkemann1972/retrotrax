#pragma once

#include <juce_core/juce_core.h>
#include <vector>

// Kuratierte, frei nutzbare SoundFont-Banks (SF2). Wie bei den ST-Disks ist nur
// der Katalog fest eingebaut - die .sf2-Datei selbst wird erst beim Anwaehlen
// einmalig heruntergeladen und lokal gecacht. Fokus: schwer auffindbare,
// charaktervolle Retro-/Konsolen-Klaenge plus ein paar schlanke GM-Allrounder.
//
// Quelle (stabile Direktlinks): Internet Archive, Item "free-soundfonts-sf2-2019-04".
namespace sf2cat
{

inline const char* baseUrl()
{
    return "https://archive.org/download/free-soundfonts-sf2-2019-04/";
}

struct CatalogEntry
{
    const char* name; // Anzeigename in der Quellenliste
    const char* file; // exakter Dateiname im Archiv
    int         sizeKb;
};

// Bewusst klein gehalten und retro-lastig; die Groesse steht in der Liste, damit
// niemand versehentlich einen 60-MB-Brocken zieht.
inline std::vector<CatalogEntry> banks()
{
    return {
        // --- Chiptune / Konsolen ---
        { "8-Bit / Chiptune",          "8bitsf.SF2",                                6860 },
        { "GXSCC Chip-GM (winzig)",    "GXSCC_gm_033.sf2",                           129 },
        { "Super Nintendo (SNES)",     "Super_Nintendo_Unofficial_update.sf2",      1946 },
        { "Nintendo 64",               "Nintendo_64_ver_2.0.sf2",                  12800 },
        { "Nintendo Wii",              "The_Ultimate_Wii_Soundfont_V1-1.sf2",      18125 },
        { "Sega Mega Drive / Genesis", "The_Ultimate Megadrive_Soundfont[v1.5].sf2", 64717 },
        // --- Vintage-Synths / Klassiker ---
        { "Roland MT-32 (DOS-Klassiker)", "MT32.sf2",                               7782 },
        { "Roland SC-55 (90er-MIDI)",  "Roland_SC-55_v1.1 full pack.sf2",          10138 },
        { "Nokia S30 (Handy-Retro)",   "Nokia_S30.sf2",                             6451 },
        { "Yamaha XG Sound Set",       "Yamaha_XG_Sound_Set.sf2",                   3994 },
        // --- Schlanke GM-Allrounder ---
        { "TimGM6mb (GM, Public Domain)", "TimGM6mb.sf2",                           6144 },
        { "GeneralUser GS (GM, retro)", "GeneralUser GS v1.471.sf2",               32051 },
        { "Florestan Basic GM (klein)", "Florestan_Basic_GM_GS.sf2",               3380 },
        { "Creative 2 MB GM (winzig)", "Creative Labs 2M GM_2gmgsmt.sf2",          2150 },
    };
}

} // namespace sf2cat
