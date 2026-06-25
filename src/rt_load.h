#pragma once
//
// rt_load.h - .retrotrax laden + Song zu WAV rendern, JUCE-frei.
//
// Phase 2a: liest bpm/swing/order, Synth-Instrumente und die Pattern-Zellen <C>
// in die TrackerEngine. Sample-Instrumente (eingebettete <D>-Base64/GZIP-Daten)
// folgen in Phase 2b. Rendert den Song einmal komplett (bis die Reihenfolge
// einmal umlaeuft) und schreibt 16-bit-Stereo-WAV.

#include "TrackerEngine.h"
#include "rt_xml.h"

#include <cstdint>
#include <cstdio>
#include <sstream>
#include <string>
#include <vector>

namespace rtload
{
    // --- .retrotrax-XML -> Engine ------------------------------------------
    // Gibt die Zahl geladener Instrumente zurueck (>=0) oder -1 bei falscher Wurzel.
    inline int loadRetrotrax (const std::string& xmlText, TrackerEngine& engine)
    {
        using Inst = TrackerEngine::Instrument;
        rtxml::Node root = rtxml::parse (xmlText);
        if (root.tag != "RETROTRAX")
            return -1;

        engine.bpm.store   ((float) root.attrDouble ("bpm", 125.0));
        engine.swing.store ((float) root.attrDouble ("swing", 0.0));

        const std::string ord = root.attr ("order");
        if (! ord.empty())
        {
            std::istringstream iss (ord);
            int v = 0, n = 0;
            while (iss >> v && n < TrackerEngine::kMaxOrder)
                engine.order[n++] = v;
            engine.orderLen = n > 0 ? n : 1;
        }

        int loaded = 0;
        for (const auto& e : root.children)
        {
            if (e.tag == "INSTRUMENT")
            {
                const int slot = e.attrInt ("slot", -1);
                if (slot < 0) continue;

                auto inst = std::make_unique<Inst>();
                inst->name = e.attr ("name").c_str();

                if (e.attr ("kind") == "synth")
                {
                    inst->kind       = Inst::Kind::Synth;
                    inst->engine     = (Inst::Engine) e.attrInt ("eng", (int) inst->engine);
                    inst->wave       = (Inst::Wave) e.attrInt ("wave", (int) inst->wave);
                    inst->pulseWidth = (float) e.attrDouble ("pw",  inst->pulseWidth);
                    inst->attack     = (float) e.attrDouble ("a",   inst->attack);
                    inst->decay      = (float) e.attrDouble ("d",   inst->decay);
                    inst->sustain    = (float) e.attrDouble ("s",   inst->sustain);
                    inst->release    = (float) e.attrDouble ("rel", inst->release);
                    inst->filter     = (Inst::Filter) e.attrInt ("flt", (int) inst->filter);
                    inst->cutoff     = (float) e.attrDouble ("cut", inst->cutoff);
                    inst->resonance  = (float) e.attrDouble ("res", inst->resonance);
                    inst->ringMod    = e.attrInt ("ring", inst->ringMod ? 1 : 0) != 0;
                    inst->sync       = e.attrInt ("sync", inst->sync ? 1 : 0) != 0;
                    inst->modTune    = (float) e.attrDouble ("mtune", inst->modTune);
                    inst->pwmRate    = (float) e.attrDouble ("pwmr", inst->pwmRate);
                    inst->pwmDepth   = (float) e.attrDouble ("pwmd", inst->pwmDepth);
                    inst->unison     = e.attrInt ("uni", inst->unison);
                    inst->detune     = (float) e.attrDouble ("det", inst->detune);
                    inst->chord      = e.attrInt ("chord", inst->chord);
                    engine.setInstrument (slot, std::move (inst));
                    ++loaded;
                }
                else
                {
                    // Sample-Instrument: Daten stecken im <D>-Kind (Base64+GZIP).
                    // -> Phase 2b. Vorerst ueberspringen (kein Klang fuer diesen Slot).
                }
            }
            else if (e.tag == "C")
            {
                const int p = e.attrInt ("p", 0);
                const int r = e.attrInt ("r", 0);
                const int t = e.attrInt ("t", 0);
                if (p < 0 || p >= TrackerEngine::kMaxPatterns) continue;
                if (r < 0 || r >= TrackerEngine::kRows)        continue;
                if (t < 0 || t >= TrackerEngine::kTracks)      continue;
                auto& c = engine.patterns[p][r][t];
                c.note        = e.attrInt ("n", -1);
                c.instrument  = e.attrInt ("i", -1);
                c.volume      = e.attrInt ("v", -1);
                c.effect      = e.attrInt ("fx", -1);
                c.effectParam = e.attrInt ("fp", 0);
            }
        }
        return loaded;
    }

    // --- WAV schreiben (16-bit PCM, interleaved) ---------------------------
    inline void writeLE (std::vector<uint8_t>& v, uint32_t val, int bytes)
    {
        for (int i = 0; i < bytes; ++i) v.push_back ((uint8_t) ((val >> (8 * i)) & 0xFF));
    }

    inline bool writeWav (const std::string& path, const std::vector<int16_t>& interleaved,
                          int channels, int sampleRate)
    {
        const uint32_t dataBytes = (uint32_t) (interleaved.size() * sizeof (int16_t));
        std::vector<uint8_t> hdr;
        hdr.insert (hdr.end(), { 'R','I','F','F' });
        writeLE (hdr, 36 + dataBytes, 4);
        hdr.insert (hdr.end(), { 'W','A','V','E', 'f','m','t',' ' });
        writeLE (hdr, 16, 4);                 // fmt chunk size
        writeLE (hdr, 1, 2);                  // PCM
        writeLE (hdr, (uint32_t) channels, 2);
        writeLE (hdr, (uint32_t) sampleRate, 4);
        writeLE (hdr, (uint32_t) (sampleRate * channels * 2), 4); // byte rate
        writeLE (hdr, (uint32_t) (channels * 2), 2);              // block align
        writeLE (hdr, 16, 2);                 // bits per sample
        hdr.insert (hdr.end(), { 'd','a','t','a' });
        writeLE (hdr, dataBytes, 4);

        FILE* f = std::fopen (path.c_str(), "wb");
        if (! f) return false;
        std::fwrite (hdr.data(), 1, hdr.size(), f);
        std::fwrite (interleaved.data(), sizeof (int16_t), interleaved.size(), f);
        std::fclose (f);
        return true;
    }

    // Song einmal komplett rendern (bis die Reihenfolge umlaeuft) -> WAV.
    // Gibt die Zahl gerenderter Frames zurueck.
    inline long renderSongToWav (TrackerEngine& engine, const std::string& path,
                                 double sampleRate, double maxSeconds = 600.0)
    {
        engine.prepare (sampleRate);
        engine.songMode.store (true);
        engine.play();

        const int block = 512, ch = 2;
        juce::AudioBuffer<float> buf (ch, block);
        std::vector<int16_t> pcm;
        const long maxFrames = (long) (maxSeconds * sampleRate);
        long frames = 0;

        // play() startet bei Zeile 63, damit der erste Tick auf Zeile 0 landet.
        // Bei orderLen==1 zaehlt dieser Start-Uebergang schon als ein Umlauf.
        // Darum nach dem 1. Block den Loop-Stand als Basislinie merken und erst
        // stoppen, wenn er DARUEBER steigt -> genau ein voller Durchlauf, robust
        // fuer ein wie fuer viele Patterns.
        long startLoops = -1;
        while (frames < maxFrames)
        {
            buf.clear();
            engine.process (buf);
            for (int i = 0; i < block; ++i)
                for (int c = 0; c < ch; ++c)
                {
                    float s = buf.getReadPointer (c)[i];
                    if (s >  1.0f) s =  1.0f;
                    if (s < -1.0f) s = -1.0f;
                    pcm.push_back ((int16_t) (s * 32767.0f));
                }
            frames += block;

            const long loops = engine.songLoopCount.load();
            if (startLoops < 0) startLoops = loops;     // Basislinie nach 1. Block
            else if (loops > startLoops) break;          // Reihenfolge wirklich umgelaufen
        }
        writeWav (path, pcm, ch, (int) sampleRate);
        return frames;
    }
}
