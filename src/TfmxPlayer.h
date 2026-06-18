#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>
#include <cstdint>

// =============================================================================
//  TFMX (Chris Huelsbeck, Amiga) - eigener, offen nachgebauter Replayer.
//
//  TFMX ("The Final Musicsystem eXtended") ist KEIN Pattern-Raster wie MOD/XM,
//  sondern eine laufende Makro-Engine: der Sequencer (Tracksteps -> Patterns)
//  und der Synthesizer (Makros pro Stimme) laufen getrennt. Faithful TFMX heisst
//  deshalb Replayer (wie unser reSIDfp-Chip ein Renderer ist), nicht Import.
//
//  Aufbau zweier Dateien:
//    .mdat  - Big-Endian Song-/Pattern-/Makro-Daten (Header + Tabellen)
//    .smpl  - roher 8-Bit-signed Sample-Speicher (Paula-Samples)
//
//  Header (.mdat), gegen echte Dateien (Apidya, Turrican 2) verifiziert:
//    0x000  Magic-Text, beginnt mit "TFMX"
//    0x100  Subsong-Start-Tabelle (32 x uint16)
//    0x140  Subsong-End-Tabelle   (32 x uint16)
//    0x180  Subsong-Tempo-Tabelle (32 x uint16)
//    0x1D0  Zeiger auf Trackstep-Tabelle (uint32)
//    0x1D4  Zeiger auf Pattern-Zeiger-Tabelle (uint32)
//    0x1D8  Zeiger auf Makro-Zeiger-Tabelle (uint32)
//  Die Pattern-/Makro-DATEN stehen vorn im File, die Zeiger-Tabellen am Ende:
//    #Patterns = (macroTbl - patternTbl) / 4
//    #Makros   = (Dateiende - macroTbl) / 4
//    #Tracksteps = (erster Pattern-Zeiger - trackTbl) / 16   (16 Bytes je Step)
//
//  STUFE 1 (jetzt): Format lesen + Diagnose. Audio (Paula + Makro-Engine) folgt
//  in Stufe 2/3 - render() ist hier bewusst noch stumm.
// =============================================================================
class TfmxPlayer
{
public:
    struct Info
    {
        bool ok = false;
        juce::String title;
        int subsongs   = 0;
        int patterns   = 0;
        int macros     = 0;
        int tracksteps = 0;
        int sampleBytes = 0;
        juce::String message; // Fehlertext, falls ok == false
    };

    TfmxPlayer() = default;

    // Liest mdat + smpl ein und fuellt info(). Big-Endian. Gibt false + Fehlertext
    // in info().message zurueck, wenn die Datei nicht wie ein TFMX-Modul aussieht.
    bool load (const juce::File& mdatFile, const juce::File& smplFile)
    {
        info_ = Info(); // zuruecksetzen

        juce::MemoryBlock mb;
        if (! mdatFile.loadFileAsData (mb) || mb.getSize() < 0x200)
        {
            info_.message = "mdat-Datei zu klein oder nicht lesbar.";
            return false;
        }
        mdat_.assign ((const uint8_t*) mb.getData(),
                      (const uint8_t*) mb.getData() + mb.getSize());

        // Magic: beginnt mit "TFMX" (z.B. "TFMX-SONG ", "TFMX_SONG", "TFMX 1.5").
        if (! (mdat_[0] == 'T' && mdat_[1] == 'F' && mdat_[2] == 'M' && mdat_[3] == 'X'))
        {
            info_.message = "Kein TFMX-Modul (Kennung \"TFMX\" fehlt am Anfang).";
            return false;
        }

        // Sample-Bank (.smpl) - reines 8-Bit-PCM, Groesse = Anzahl Bytes.
        juce::MemoryBlock sb;
        if (smplFile.existsAsFile() && smplFile.loadFileAsData (sb))
            smpl_.assign ((const uint8_t*) sb.getData(),
                          (const uint8_t*) sb.getData() + sb.getSize());
        info_.sampleBytes = (int) smpl_.size();

        // Tabellen-Zeiger (Big-Endian, absolute Datei-Offsets).
        trackTbl_ = be32 (0x1D0);
        patTbl_   = be32 (0x1D4);
        macTbl_   = be32 (0x1D8);

        const uint32_t fileSize = (uint32_t) mdat_.size();
        if (patTbl_ < 0x200 || macTbl_ <= patTbl_ || macTbl_ > fileSize)
        {
            info_.message = "TFMX-Tabellen-Zeiger unplausibel (evtl. exotische Variante).";
            return false;
        }

        info_.patterns = (int) ((macTbl_ - patTbl_) / 4);
        info_.macros   = (int) ((fileSize - macTbl_) / 4);

        // Tracksteps: vom Trackstep-Zeiger bis zum ersten Pattern-Daten-Offset.
        const uint32_t firstPatData = be32At (patTbl_);
        if (firstPatData > trackTbl_ && firstPatData <= fileSize && trackTbl_ >= 0x200)
            info_.tracksteps = (int) ((firstPatData - trackTbl_) / 16);

        // Subsongs: aktive Eintraege in der Start/End-Tabelle zaehlen.
        for (int i = 0; i < 32; ++i)
        {
            songStart_[i] = be16 (0x100 + i * 2);
            songEnd_[i]   = be16 (0x140 + i * 2);
            songTempo_[i] = be16 (0x180 + i * 2);
        }
        info_.subsongs = 0;
        for (int i = 0; i < 32; ++i)
        {
            // Terminator-/Leer-Eintraege ueberspringen; gueltig = Ende >= Start,
            // im kleinen Trackstep-Indexbereich, nicht der typische 0x1FF/0xFFFF-Marker.
            if (songStart_[i] >= 0x1FF) break;
            if (songEnd_[i] >= songStart_[i] && ! (songStart_[i] == 0 && songEnd_[i] == 0 && i > 0))
                ++info_.subsongs;
        }
        if (info_.subsongs == 0) info_.subsongs = 1; // mindestens ein Song

        info_.title = mdatTitleFromFile (mdatFile);
        info_.ok = true;
        return true;
    }

    const Info& info() const { return info_; }
    bool isLoaded() const    { return info_.ok; }

    void prepare (double sampleRate) { sampleRate_ = sampleRate; }

    // STUFE 1: noch keine Wiedergabe. Fuellt mit Stille, damit der Audiopfad sicher
    // bleibt. Paula-Mixer + Makro-Engine kommen in Stufe 2.
    void render (juce::AudioBuffer<float>& buffer, int startSample, int numSamples)
    {
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            buffer.clear (ch, startSample, numSamples);
    }

private:
    // "mdat.apidya (title)" -> "apidya (title)"; sonst Dateiname ohne Endung.
    static juce::String mdatTitleFromFile (const juce::File& f)
    {
        auto name = f.getFileName();
        if (name.startsWithIgnoreCase ("mdat."))
            return name.substring (5);
        if (f.getFileExtension().equalsIgnoreCase (".mdat"))
            return f.getFileNameWithoutExtension();
        return name;
    }

    uint16_t be16 (size_t off) const
    {
        if (off + 1 >= mdat_.size()) return 0;
        return (uint16_t) ((mdat_[off] << 8) | mdat_[off + 1]);
    }
    uint32_t be32 (size_t off) const { return be32At ((uint32_t) off); }
    uint32_t be32At (uint32_t off) const
    {
        if ((size_t) off + 3 >= mdat_.size()) return 0;
        return ((uint32_t) mdat_[off]     << 24) | ((uint32_t) mdat_[off + 1] << 16)
             | ((uint32_t) mdat_[off + 2] <<  8) |  (uint32_t) mdat_[off + 3];
    }

    Info info_;
    std::vector<uint8_t> mdat_, smpl_;
    double sampleRate_ = 44100.0;

    uint32_t trackTbl_ = 0, patTbl_ = 0, macTbl_ = 0;
    uint16_t songStart_[32] {}, songEnd_[32] {}, songTempo_[32] {};
};
