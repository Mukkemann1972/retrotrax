// TFMX audio decoder library by Michael Schwendt

// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License along
// with this program; if not, see <https://www.gnu.org/licenses/>.

#ifndef DECODER_H
#define DECODER_H

#include <cstdint>
#include <string>
#include <vector>

#include "MyTypes.h"

namespace tfmxaudiodecoder {

class PaulaVoice;

class Decoder {
  public:
    Decoder();
    virtual ~Decoder() {};

    virtual void setPath(std::string pathArg);

    virtual bool init(void*, udword, int) {
        return false;
    }
    virtual bool detect(void*, udword) {
        return false;
    }
    virtual void restart() {}
    virtual void setPaulaVoice(ubyte, PaulaVoice*) {}
    virtual int run() {
        return 20;
    }
    virtual void seek(sdword);

    virtual ubyte getVoices() {
        return 0;
    }
    virtual int getSongs() {
        return 0;
    }

    // Pattern display API - per-track unrolled pattern data
    virtual bool buildPatternDisplay() {
        return false;
    }
    virtual int getPatternDisplayNumTracks() {
        return 0;
    }
    virtual uint32_t getPatternDisplayTrackRows(int track) {
        (void)track;
        return 0;
    }
    virtual bool getPatternDisplayRow(int track, uint32_t row, uint8_t* type, uint8_t* note, uint8_t* macro,
                                      uint8_t* volume, int8_t* detune, uint8_t* channel, uint16_t* wait,
                                      uint32_t* tick) {
        (void)track;
        (void)row;
        if (type)
            *type = 0;
        if (note)
            *note = 0;
        if (macro)
            *macro = 0;
        if (volume)
            *volume = 0;
        if (detune)
            *detune = 0;
        if (channel)
            *channel = 0;
        if (wait)
            *wait = 0;
        if (tick)
            *tick = 0;
        return false;
    }
    virtual uint32_t getPatternTickCounter() {
        return 0;
    }
    virtual int findPatternDisplayRowForTick(int track, uint32_t tick) {
        (void)track;
        (void)tick;
        return -1;
    }

    const std::string getFormatName() {
        return formatName;
    }
    const std::string getFormatID() {
        return formatID;
    }
    const char* getInfoString(const std::string which);
    udword getDuration() {
        return duration;
    }
    bool getSongEndFlag() {
        return songEnd;
    }
    void setLoopMode(bool x) {
        loopMode = x;
    }
    void setSongEndFlag(bool x) {
        songEnd = x;
    }
    udword getRate() {
        return rate;
    }

  protected:
    void setRate(udword); // [Hz] *256
    void setBPM(uword);

    static const std::string UNKNOWN_FORMAT_ID;

    std::string formatID;
    std::string formatName;
    std::string author, title, game, name;

    std::string path;

    bool songEnd, loopMode;
    udword duration;
    udword rate;
    udword tickFP, tickFPadd;
};

} // namespace tfmxaudiodecoder

#endif // DECODER_H
