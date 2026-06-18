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

#include "DecoderProxy.h"
#include "Chris/TFMXDecoder.h"
#include "Jochen/HippelDecoder.h"
#include <fstream>

namespace tfmxaudiodecoder {

class PaulaMixer;

DecoderProxy::DecoderProxy() {
    pDecoder = new Decoder;
    pMixer = 0;

    endShortsMode = false;
    endShortsDuration = 10 * 1000;

    formatID = pDecoder->getFormatID().c_str();
    formatName = pDecoder->getFormatName().c_str();
}

DecoderProxy::~DecoderProxy() {
    delete pDecoder;
}

udword DecoderProxy::getDuration() {
    return pDecoder->getDuration();
}

bool DecoderProxy::getSongEndFlag() {
    return pDecoder->getSongEndFlag();
}

void DecoderProxy::endShorts(int flag, int maxSecs) {
    endShortsMode = (flag == 1);
    endShortsDuration = maxSecs * 1000;
}

void DecoderProxy::setLoopMode(int flag) {
    pDecoder->setLoopMode(flag == 1);
}

const char* DecoderProxy::getFormatID() {
    return formatID.c_str();
}

const char* DecoderProxy::getFormatName() {
    return formatName.c_str();
}

const char* DecoderProxy::getInfoString(const std::string which) {
    return pDecoder->getInfoString(which);
}

void DecoderProxy::setPath(const char* pathArg) {
    path = pathArg ? pathArg : "";
}

int DecoderProxy::getSongs() {
    return pDecoder->getSongs();
}

int DecoderProxy::getVoices() {
    return pDecoder->getVoices();
}

int DecoderProxy::run() {
    return pDecoder->run();
}

// --------------------------------------------------------------------------

bool DecoderProxy::maybeOurs(void* data, udword length) {
    Decoder* pd; // we use a separate decoder instance within here
    bool maybe = false;

    pd = new TFMXDecoder; // For Huelsbeck's TFMX.
    maybe = pd->detect(data, length);
    if (maybe) {
        goto foundSomething;
    }

    delete pd;
    pd = new HippelDecoder; // For Hippel's TFMX and derivatives like FC.
    maybe = pd->detect(data, length);
    if (maybe) {
        goto foundSomething;
    }

    delete pd;
    pd = new Decoder; // dummy
foundSomething:
    formatID = pd->getFormatID().c_str();
    formatName = pd->getFormatName().c_str();
    delete pd;
    return maybe;
}

// --------------------------------------------------------------------------

bool DecoderProxy::init(void* data, udword length, int songNumber) {
    currentSong = songNumber;
    delete pDecoder;

    pDecoder = new TFMXDecoder;
    if (initDecoder(data, length, songNumber)) {
        return true;
    }
    delete pDecoder;

    pDecoder = new HippelDecoder;
    if (initDecoder(data, length, songNumber)) {
        return true;
    }
    delete pDecoder;

    pDecoder = new Decoder;
    return false;
}

bool DecoderProxy::reinit(int songNumber) {
    if (songNumber >= 0) {
        currentSong = songNumber;
    }
    return initDecoder(0, 0, currentSong);
}

bool DecoderProxy::initDecoder(void* data, udword length, int songNumber) {
    pDecoder->setPath(path);
    if (!pDecoder->init(data, length, songNumber)) {
        return false;
    }
    mixerInit();

    udword duration = pDecoder->getDuration();
    if (endShortsMode && duration && (duration < endShortsDuration)) {
        pDecoder->setSongEndFlag(true);
    }

    formatID = pDecoder->getFormatID().c_str();
    formatName = pDecoder->getFormatName().c_str();
    return true;
}

// --------------------------------------------------------------------------

// The interface to a cheap Paula simulator/mixer.
void DecoderProxy::setMixer(LamePaulaMixer* mixer) {
    pMixer = mixer;
}

// Create needed number of voices and replace the dummies.
void DecoderProxy::mixerInit() {
    if (!pMixer) {
        return;
    }
    pMixer->init(pDecoder);
}

void DecoderProxy::mixerFillBuffer(void* buffer, udword bufferLen) {
    pMixer->fillBuffer(buffer, bufferLen, pDecoder);
}

bool DecoderProxy::seek(sdword ms) {
    pDecoder->seek(ms);
    if (pMixer) {
        pMixer->drain();
    }
    return true;
}

bool DecoderProxy::load(const char* path, int songNum) {
    bool good = false;
    setPath(path);
    std::ifstream fIn(path, std::ios::in | std::ios::binary | std::ios::ate);
    if (fIn.is_open()) {
        uint32_t fLen = fIn.tellg();
        fIn.seekg(0, std::ios::beg);
        char* buf = new char[fLen];
        fIn.read(buf, fLen);
        if (fIn.bad()) {
            delete[] buf;
            return false;
        }
        fIn.close();
        good = init(buf, fLen, songNum);
        delete[] buf;
    }
    return good;
}

// --------------------------------------------------------------------------
// Pattern display API

bool DecoderProxy::buildPatternDisplay() {
    if (pDecoder) {
        return pDecoder->buildPatternDisplay();
    }
    return false;
}

int DecoderProxy::getPatternDisplayNumTracks() {
    if (pDecoder) {
        return pDecoder->getPatternDisplayNumTracks();
    }
    return 0;
}

uint32_t DecoderProxy::getPatternDisplayTrackRows(int track) {
    if (pDecoder) {
        return pDecoder->getPatternDisplayTrackRows(track);
    }
    return 0;
}

bool DecoderProxy::getPatternDisplayRow(int track, uint32_t row, uint8_t* type, uint8_t* note, uint8_t* macro,
                                        uint8_t* volume, int8_t* detune, uint8_t* channel, uint16_t* wait,
                                        uint32_t* tick) {
    if (pDecoder) {
        return pDecoder->getPatternDisplayRow(track, row, type, note, macro, volume, detune, channel, wait, tick);
    }
    return false;
}

uint32_t DecoderProxy::getPatternTickCounter() {
    if (pDecoder) {
        return pDecoder->getPatternTickCounter();
    }
    return 0;
}

int DecoderProxy::findPatternDisplayRowForTick(int track, uint32_t tick) {
    if (pDecoder) {
        return pDecoder->findPatternDisplayRowForTick(track, tick);
    }
    return -1;
}

} // namespace tfmxaudiodecoder
