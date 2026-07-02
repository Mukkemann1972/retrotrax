#pragma once
//
// rt_tfmx.h - TFMX-Modul (Chris Huelsbeck) JUCE-frei zu WAV rendern (Phase 1b).
//
// Nutzt denselben TfmxPlayer wie das Plugin (duenner Wrapper um den vendorten
// GPL-Decoder), jetzt aber ueber den JUCE-Shim -> laeuft im schlanken Replayer.
// TFMX ist eine laufende Makro-Engine ohne Ende, darum rendern wir eine feste
// Spieldauer (Sekunden) statt "bis die Reihenfolge umlaeuft".

#include "TfmxPlayer.h"
#include "rt_load.h" // rtload::writeWav

#include <string>
#include <vector>

namespace rttfmx
{
    inline bool fileExists (const std::string& p)
    {
        struct stat st;
        return ! p.empty() && ::stat (p.c_str(), &st) == 0 && S_ISREG (st.st_mode);
    }

    // Findet die Sample-Datei (.smpl/.sam) neben dem mdat - gleiche Konventionen
    // wie findTfmxSmpl im Plugin (Modland "mdat."<->"smpl.", Endungen).
    inline std::string findSmpl (const std::string& mdatPath)
    {
        const auto slash = mdatPath.find_last_of ("/\\");
        const std::string dir  = (slash == std::string::npos) ? std::string() : mdatPath.substr (0, slash + 1);
        const std::string name = (slash == std::string::npos) ? mdatPath : mdatPath.substr (slash + 1);

        auto lower = [] (std::string s) { for (auto& c : s) c = (char) std::tolower ((unsigned char) c); return s; };
        const std::string ln = lower (name);

        // Modland: "mdat.xxx" -> "smpl.xxx"
        if (ln.rfind ("mdat.", 0) == 0)
        {
            std::string cand = dir + "smpl." + name.substr (5);
            if (fileExists (cand)) return cand;
        }
        // Endungen: ".mdat"->".smpl", ".tfmx"/".tfx"/".tfm"->".sam"/".smpl"
        const auto dot = name.find_last_of ('.');
        if (dot != std::string::npos && dot != 0)
        {
            const std::string base = dir + name.substr (0, dot);
            const std::string ext  = lower (name.substr (dot));
            if (ext == ".mdat" && fileExists (base + ".smpl")) return base + ".smpl";
            if (ext == ".tfmx" || ext == ".tfx" || ext == ".tfm")
            {
                if (fileExists (base + ".sam"))  return base + ".sam";
                if (fileExists (base + ".smpl")) return base + ".smpl";
            }
        }
        // Fallback: "mdat" im Namen durch "smpl" ersetzen.
        auto pos = ln.find ("mdat");
        if (pos != std::string::npos)
        {
            std::string cand = name;
            cand.replace (pos, 4, "smpl");
            cand = dir + cand;
            if (fileExists (cand)) return cand;
        }
        return {};
    }

    // Rendert 'seconds' Sekunden TFMX-Wiedergabe in einen interleaved Stereo-
    // Float-Puffer (L,R,L,R). Gemeinsame Basis fuer WAV (CLI) UND Web/WASM.
    // Rueckgabe: gerenderte Frames, oder <0 bei Fehler.
    inline long renderTfmxToBuffer (const std::string& mdatPath, std::vector<float>& interleaved,
                                    double sampleRate, double seconds,
                                    TfmxPlayer::Info* infoOut = nullptr)
    {
        TfmxPlayer player;
        const std::string smplPath = findSmpl (mdatPath);
        if (! player.load (juce::File (mdatPath), juce::File (smplPath)))
        {
            if (infoOut != nullptr) *infoOut = player.info();
            return -1; // keine gueltige TFMX-Datei
        }
        if (infoOut != nullptr) *infoOut = player.info();
        if (! player.isPlayable())
            return -2; // Diagnose ok, aber Decoder konnte nicht initialisieren

        player.prepare (sampleRate);
        player.restart();

        const int block = 512, ch = 2;
        juce::AudioBuffer<float> buf (ch, block);
        interleaved.clear();
        const long maxFrames = (long) (seconds * sampleRate);
        long frames = 0;

        while (frames < maxFrames)
        {
            buf.clear();
            player.render (buf, 0, block);
            const float* L = buf.getReadPointer (0);
            const float* R = buf.getReadPointer (1);
            for (int i = 0; i < block; ++i)
            {
                interleaved.push_back (L[i]);
                interleaved.push_back (R[i]);
            }
            frames += block;
        }
        return frames;
    }

    // Rendert TFMX -> 16-bit-Stereo-WAV (fuer die CLI).
    inline long renderTfmxToWav (const std::string& mdatPath, const std::string& outPath,
                                 double sampleRate, double seconds,
                                 TfmxPlayer::Info* infoOut = nullptr)
    {
        std::vector<float> interleaved;
        const long frames = renderTfmxToBuffer (mdatPath, interleaved, sampleRate, seconds, infoOut);
        if (frames < 0) return frames;
        std::vector<int16_t> pcm;
        pcm.reserve (interleaved.size());
        for (float s : interleaved)
        {
            if (s >  1.0f) s =  1.0f;
            if (s < -1.0f) s = -1.0f;
            pcm.push_back ((int16_t) (s * 32767.0f));
        }
        rtload::writeWav (outPath, pcm, 2, (int) sampleRate);
        return frames;
    }
}
