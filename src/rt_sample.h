#pragma once
//
// rt_sample.h - eingebettete Sample-Daten (<D>) dekodieren, JUCE-frei.
//
// Phase 2b des Replayers: Sample-Instrumente klingen auch AUSSERHALB des
// Plugins. Das Plugin schreibt Sample-Daten als 16-Bit-PCM (interleaved) ->
// zlib-komprimiert (JUCEs GZIPCompressorOutputStream mit windowBits=0 = zlib/
// RFC1950, erstes Byte 0x78) -> Base64 in den <D>-Textknoten. Hier der
// umgekehrte Weg: Base64 -> zlib-inflate (System-zlib) -> float-Samples,
// exakt gespiegelt zu decodeSamples() in PluginProcessor.cpp.

#include "TrackerEngine.h"

#include <cstdint>
#include <string>
#include <vector>
#include <zlib.h>

namespace rtsample
{
    // Base64 -> Bytes. Nicht-Base64-Zeichen (Zeilenumbrueche/Leerraum aus der
    // XML-Formatierung) werden uebersprungen; '=' beendet die Auffuellung.
    inline std::vector<uint8_t> base64Decode (const std::string& in)
    {
        auto val = [] (unsigned char c) -> int
        {
            if (c >= 'A' && c <= 'Z') return c - 'A';
            if (c >= 'a' && c <= 'z') return c - 'a' + 26;
            if (c >= '0' && c <= '9') return c - '0' + 52;
            if (c == '+') return 62;
            if (c == '/') return 63;
            return -1;
        };
        std::vector<uint8_t> out;
        out.reserve (in.size() * 3 / 4 + 3);
        int acc = 0, bits = 0;
        for (unsigned char c : in)
        {
            if (c == '=') break;
            int v = val (c);
            if (v < 0) continue; // Leerraum/Zeilenumbruch ueberspringen
            acc = (acc << 6) | v;
            bits += 6;
            if (bits >= 8)
            {
                bits -= 8;
                out.push_back ((uint8_t) ((acc >> bits) & 0xFF));
            }
        }
        return out;
    }

    // zlib-Datenstrom -> Rohbytes bekannter Ziel-Groesse. true bei Erfolg.
    inline bool zlibInflate (const uint8_t* src, size_t srcLen,
                             std::vector<uint8_t>& out, size_t expectedLen)
    {
        out.resize (expectedLen);
        uLongf destLen = (uLongf) expectedLen;
        const int r = uncompress (out.data(), &destLen, src, (uLong) srcLen);
        return r == Z_OK && destLen == (uLongf) expectedLen;
    }

    // <D>-Text (Base64) -> AudioBuffer. Spiegelt exakt decodeSamples() im
    // Plugin: interleaved int16 [i*ch + c] -> float / 32768.
    inline bool decodeSampleData (const std::string& b64, int ch, int n,
                                  juce::AudioBuffer<float>& out)
    {
        if (ch < 1 || n < 1 || b64.empty())
            return false;
        std::vector<uint8_t> zbytes = base64Decode (b64);
        if (zbytes.empty())
            return false;
        const size_t pcmBytes = (size_t) ch * (size_t) n * 2;
        std::vector<uint8_t> pcm;
        if (! zlibInflate (zbytes.data(), zbytes.size(), pcm, pcmBytes))
            return false;
        const auto* s = (const int16_t*) pcm.data();
        out.setSize (ch, n);
        for (int i = 0; i < n; ++i)
            for (int c = 0; c < ch; ++c)
                out.getWritePointer (c)[i] = (float) s[i * ch + c] / 32768.0f;
        return true;
    }
}
