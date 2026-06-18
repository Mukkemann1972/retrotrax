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

#include "TFMXDecoder.h"

#include <functional> // For std::function (recursive lambda)

namespace tfmxaudiodecoder {

udword TFMXDecoder::getPattOffset(ubyte pt) {
    // With this TFMX format it's always an array of offsets to patterns.
    return offsets.header + readBEudword(pBuf, offsets.patterns + (pt << 2));
}

void TFMXDecoder::processPattern(Track& tr) {
#if defined(DEBUG_RUN)
    cout << "  processPattern() at 0x" << tohex(tr.pattern.offset) << endl;
#endif
    int evalMaxLoops = RECURSE_LIMIT; // NB! Around 8 would suffice.
    do {
        tr.pattern.evalNext = false;
#if defined(DEBUG_RUN)
        cout << "  " << hexW(tr.pattern.step) << ":";
#endif
        // Offset to current step position within pattern.
        udword p = tr.pattern.offset + (tr.pattern.step << 2);
        // Fetch pattern entry, four bytes aka 'aabbcdee'.
        cmd.aa = pBuf[p];
        cmd.bb = pBuf[p + 1];
        cmd.cd = pBuf[p + 2];
        cmd.ee = pBuf[p + 3];
#if defined(DEBUG_RUN)
        cout << "  " << hexB(cmd.aa) << hexB(cmd.bb) << hexB(cmd.cd) << hexB(cmd.ee) << "  ";
#endif
        ubyte aaBak = cmd.aa;
        if (cmd.aa < 0xf0) {                       // >= 0xf0 pattern state command
            if (cmd.aa < 0xc0 && cmd.aa >= 0x7f) { // note + wait (instead of detune)
                tr.pattern.wait = cmd.ee;
                cmd.ee = 0;
            }
            cmd.aa += tr.TR;
            if (aaBak < 0xc0) {
                cmd.aa &= 0x3f;
            }
            if (tr.on) {
                noteCmd();
            }
            if (aaBak >= 0xc0 || aaBak < 0x7f) {
                tr.pattern.step++;
                tr.pattern.evalNext = true;
            } else {
                tr.pattern.step++;
            }
#if defined(DEBUG_RUN)
            cout << endl;
#endif
        } else { // cmd.aa >= 0xf0   pattern state command
            ubyte command = cmd.aa & 0xf;
            patternCmdUsed[command] = true;
            (this->*PattCmdFuncs[command])(tr);
        }
    } while (tr.pattern.evalNext && --evalMaxLoops > 0);
}

void TFMXDecoder::pattCmd_NOP(Track& tr) {
#if defined(DEBUG_RUN)
    cout << "NOP >>>>>>>>" << endl;
#endif
    tr.pattern.step++;
    tr.pattern.evalNext = true;
}

void TFMXDecoder::pattCmd_End(Track& tr) {
#if defined(DEBUG_RUN)
    cout << "End >>>>>>>>--Next track  step--" << endl;
#endif
    tr.PT = 0xff;
    if (sequencer.step.current == sequencer.step.last) {
        songEnd = true;
        triggerRestart = true;
        return;
    } else {
        sequencer.step.current++;
    }
    processTrackStep();
    sequencer.step.next = true;
}

void TFMXDecoder::pattCmd_Loop(Track& tr) {
#if defined(DEBUG_RUN)
    cout << "Loop>>>>>>>>[count     / step.w]" << endl;
#endif
    if (tr.pattern.loops == 0) { // end of loop
        tr.pattern.loops = -1;
        tr.pattern.step++;
        tr.pattern.evalNext = true;
        return;
    } else if (tr.pattern.loops == -1) { // init permitted
        tr.pattern.loops = cmd.bb - 1;

        // This would be an infinite loop that potentially affects
        // song-end detection, if all tracks loop endlessly.
        // So, let's evaluate that elsewhere.
        if (tr.pattern.loops == -1) { // infinite loop
            tr.pattern.infiniteLoop = true;
        }
    } else {
        tr.pattern.loops--;
    }
    tr.pattern.step = makeWord(cmd.cd, cmd.ee);
    tr.pattern.evalNext = true;
}

void TFMXDecoder::pattCmd_Goto(Track& tr) {
#if defined(DEBUG_RUN)
    cout << "Cont>>>>>>>>[patternno./ step.w]" << endl;
#endif
    tr.PT = cmd.bb;
    tr.pattern.offset = getPattOffset(tr.PT);
    tr.pattern.step = makeWord(cmd.cd, cmd.ee);
    tr.pattern.evalNext = true;
}

void TFMXDecoder::pattCmd_Wait(Track& tr) {
#if defined(DEBUG_RUN)
    cout << "Wait>>>>>>>>[count 00-FF--------" << endl;
#endif
    tr.pattern.wait = cmd.bb;
    tr.pattern.step++;
}

void TFMXDecoder::pattCmd_Stop(Track& tr) {
#if defined(DEBUG_RUN)
    cout << "Stop>>>>>>>>--Stop this pattern-" << endl;
#endif
    tr.PT = 0xff;
}

void TFMXDecoder::pattCmd_Note(Track& tr) {
#if defined(DEBUG_RUN)
    cout << "Note>>>" << endl;
#endif
    noteCmd();
    tr.pattern.step++;
    tr.pattern.evalNext = true;
}

void TFMXDecoder::pattCmd_SaveAndGoto(Track& tr) {
#if defined(DEBUG_RUN)
    cout << "GsPt>>>>>>>>[patternno./ step.w]" << endl;
#endif
    tr.pattern.offsetSaved = tr.pattern.offset;
    tr.pattern.stepSaved = tr.pattern.step;
    pattCmd_Goto(tr);
}

void TFMXDecoder::pattCmd_ReturnFromGoto(Track& tr) {
#if defined(DEBUG_RUN)
    cout << "RoPt>>>>>>>>-Return old pattern-" << endl;
#endif
    tr.pattern.offset = tr.pattern.offsetSaved;
    tr.pattern.step = tr.pattern.stepSaved;
    tr.pattern.step++;
    tr.pattern.evalNext = true;
}

void TFMXDecoder::pattCmd_Fade(Track& tr) {
    fadeInit(cmd.ee, cmd.bb);
    tr.pattern.step++;
    tr.pattern.evalNext = true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Pattern display generation - walks patterns and unrolls loops for visualization
//
// This uses tick-based simulation to match the original decoder behavior:
// - Each tick, all tracks are processed together
// - If a track's wait counter is 0, it processes the next command
// - If a track's wait counter is > 0, it decrements
// - When ANY track hits END, it immediately triggers step transition for ALL tracks

bool TFMXDecoder::buildPatternDisplay(int stepFirst, int stepLast, PatternDisplayData& out) {
    if (!admin.initialized) {
        return false;
    }

    // Initialize output structure
    out.numTracks = sequencer.tracks;
    for (int t = 0; t < PatternDisplayData::MAX_TRACKS; t++) {
        out.tracks[t].rows.clear();
        out.tracks[t].rows.reserve(1024);
        out.tracks[t].totalTicks = 0;
    }

    // State for pattern walking (per track) - mirrors original Track struct
    struct WalkState {
        udword patternOffset;
        uword step;
        sbyte loops;
        sbyte transpose;
        udword savedOffset;
        uword savedStep;
        bool active;         // PT < 0x90 (track is playing)
        ubyte wait;          // Wait counter (like tr.pattern.wait in original)
        bool inInfiniteLoop; // True if in LOOP with bb=0
    };

    uint32_t globalTick = 0;
    bool stepSeen[TRACK_STEPS_MAX] = { false };

    // Track state persists across sequencer steps (for PT 0x80-0x8F "continue" mode)
    WalkState trackState[TRACKS_MAX] = {};

    // Current sequence step (like sequencer.step.current in original)
    int seqStep = stepFirst;

    // Helper to load track states for current seqStep (like processTrackStep)
    // Use std::function to allow recursive calls
    std::function<bool()> loadTrackStates = [&]() -> bool {
        if (seqStep < 0 || seqStep >= TRACK_STEPS_MAX || seqStep > stepLast) {
            return false; // End of song
        }

        // Loop detection at sequencer level
        if (stepSeen[seqStep]) {
            return false; // Loop detected
        }
        stepSeen[seqStep] = true;

        udword stepOffset = offsets.trackTable + (seqStep << 4);

        // Check for track command (0xEFFE prefix)
        if (readBEuword(pBuf, stepOffset) == 0xEFFE) {
            uword command = readBEuword(pBuf, stepOffset + 2);
            if (command == 0) { // STOP
                return false;
            }
            // Skip track commands like LOOP, SPEED, etc. and advance to next step
            seqStep++;
            return loadTrackStates(); // Recursively try next step
        }

        // Update walk state for each track based on PT value (like processTrackStep)
        for (int t = 0; t < sequencer.tracks && t < TRACKS_MAX; t++) {
            ubyte pt = pBuf[stepOffset + t * 2];
            sbyte tr = (sbyte)pBuf[stepOffset + t * 2 + 1];

            if (pt < 0x80) {
                // New pattern - reset state
                trackState[t].active = true;
                trackState[t].patternOffset = getPattOffset(pt);
                trackState[t].step = 0;
                trackState[t].loops = -1;
                trackState[t].transpose = tr;
                trackState[t].savedOffset = 0;
                trackState[t].savedStep = 0;
                trackState[t].wait = 0;
                trackState[t].inInfiniteLoop = false;
            } else if (pt < 0x90) {
                // Continue pattern - keep existing state
            } else {
                // Track not used
                trackState[t].active = false;
            }
        }
        return true;
    };

    // Load initial track states (like restart() calling processTrackStep())
    if (!loadTrackStates()) {
        return false;
    }

    // Main tick loop (like repeated calls to run())
    bool songEnded = false;
    int maxTicks = 100000; // Safety limit

    while (!songEnded && maxTicks-- > 0) {
        // Like original: do { step.next = false; for each track... } while (step.next)
        bool stepNext;
        do {
            stepNext = false;

            // Process all tracks for this tick (like original processPTTR loop)
            for (int t = 0; t < sequencer.tracks && t < TRACKS_MAX; t++) {
                WalkState& ws = trackState[t];

                // Skip inactive tracks (PT >= 0x90)
                if (!ws.active) {
                    continue;
                }

                // Like original processPTTR: if wait == 0, process; else wait--
                if (ws.wait == 0) {
                    // Process commands until we hit a yielding command
                    // (Like original processPattern with evalNext loop)
                    int evalLimit = 64; // Prevent infinite loops in command chains
                    bool evalNext = true;

                    while (evalNext && evalLimit-- > 0) {
                        evalNext = false;

                        udword p = ws.patternOffset + (ws.step << 2);
                        if (p + 4 > input.len) {
                            ws.active = false;
                            break;
                        }

                        ubyte aa = pBuf[p];
                        ubyte bb = pBuf[p + 1];
                        ubyte cd = pBuf[p + 2];
                        ubyte ee = pBuf[p + 3];

                        if (aa >= 0xF0) {
                            // Pattern command
                            ubyte cmdType = aa & 0x0F;

                            switch (cmdType) {
                                case 0x0: // END - triggers immediate step transition
                                {
                                    ws.active = false;

                                    // Like original pattCmd_End: check if at last step
                                    if (seqStep >= stepLast) {
                                        songEnded = true;
                                    } else {
                                        // Advance to next step and reload track states
                                        seqStep++;
                                        if (!loadTrackStates()) {
                                            songEnded = true;
                                        } else {
                                            stepNext = true; // Reprocess this tick with new states
                                        }
                                    }
                                    break;
                                }

                                case 0x1: // LOOP
                                    if (ws.loops == 0) {
                                        // Loop exhausted
                                        ws.loops = -1;
                                        ws.step++;
                                        evalNext = true;
                                    } else if (ws.loops == -1) {
                                        // First time - initialize
                                        ws.loops = bb - 1;
                                        if (ws.loops < 0) {
                                            // Infinite loop (bb=0)
                                            ws.inInfiniteLoop = true;
                                        }
                                        ws.step = makeWord(cd, ee);
                                        evalNext = true;
                                    } else {
                                        ws.loops--;
                                        ws.step = makeWord(cd, ee);
                                        evalNext = true;
                                    }
                                    break;

                                case 0x2: // GOTO (to another pattern)
                                    ws.patternOffset = getPattOffset(bb);
                                    ws.step = makeWord(cd, ee);
                                    ws.loops = -1;
                                    evalNext = true;
                                    break;

                                case 0x3: // WAIT - sets wait counter (yields)
                                {
                                    PatternDisplayRow row = {};
                                    row.type = 1; // command
                                    row.note = 0xF3;
                                    row.macro = bb;
                                    row.channel = (ubyte)t;
                                    row.wait = bb;
                                    row.tick = globalTick;
                                    out.tracks[t].rows.push_back(row);

                                    ws.wait = bb; // Set wait counter (will decrement next tick)
                                    ws.step++;
                                    // Don't set evalNext - wait yields
                                    break;
                                }

                                case 0x4: // STOP
                                    ws.active = false;
                                    break;

                                case 0x8: // SaveAndGoto
                                    ws.savedOffset = ws.patternOffset;
                                    ws.savedStep = ws.step;
                                    ws.patternOffset = getPattOffset(bb);
                                    ws.step = makeWord(cd, ee);
                                    ws.loops = -1;
                                    evalNext = true;
                                    break;

                                case 0x9: // Return
                                    ws.patternOffset = ws.savedOffset;
                                    ws.step = ws.savedStep + 1;
                                    evalNext = true;
                                    break;

                                case 0xA: // Fade
                                    ws.step++;
                                    evalNext = true;
                                    break;

                                default: // NOP (0xF), Note (0x6/0x7), etc
                                    ws.step++;
                                    evalNext = true;
                                    break;
                            }
                        } else {
                            // Note command
                            PatternDisplayRow row = {};
                            row.type = 0; // note
                            // Map logical channel to Paula voice for consistent coloring with scopes
                            row.channel = channelToVoiceMap[cd & 0x07];
                            row.volume = (cd >> 4) & 0x0F;
                            row.tick = globalTick;

                            ubyte aaBak = aa;
                            if (aa < 0xC0 && aa >= 0x7F) {
                                // Note with embedded wait (aa 0x7F-0xBF)
                                // Like original: tr.pattern.wait = ee; cmd.ee = 0;
                                ws.wait = ee;
                            }

                            // Apply transpose
                            aa += ws.transpose;
                            if (aaBak < 0xC0) {
                                aa &= 0x3F;
                            }

                            row.note = aa;
                            row.macro = bb;
                            row.detune = (aaBak < 0x7F || aaBak >= 0xC0) ? (int8_t)ee : 0;
                            row.wait = (aaBak >= 0x7F && aaBak < 0xC0) ? ee : 0;

                            out.tracks[t].rows.push_back(row);
                            ws.step++;

                            // Note with embedded wait yields; others chain
                            if (aaBak >= 0xC0 || aaBak < 0x7F) {
                                evalNext = true;
                            }
                            // else: note with wait, don't set evalNext
                        }
                    }
                } else {
                    // wait > 0: decrement and skip (like original processPTTR)
                    ws.wait--;
                }

                // If stepNext or songEnded, break out of track loop immediately
                // (Like original: if (sequencer.step.next) break;)
                if (stepNext || songEnded) {
                    break;
                }
            }
        } while (stepNext); // Reprocess same tick if step transition occurred

        // If song ended, exit the tick loop
        if (songEnded) {
            break;
        }

        // Check if all active tracks are in infinite loops (song end condition)
        int countActive = 0;
        int countInfinite = 0;
        for (int t = 0; t < sequencer.tracks && t < TRACKS_MAX; t++) {
            if (trackState[t].active) {
                countActive++;
                if (trackState[t].inInfiniteLoop) {
                    countInfinite++;
                }
            }
        }
        if (countActive > 0 && countActive == countInfinite) {
            // All active tracks are in infinite loops - end of song
            songEnded = true;
            break;
        }

        // Also stop if no tracks are active
        if (countActive == 0) {
            songEnded = true;
            break;
        }

        // Advance to next tick
        globalTick++;
    }

    // Set total ticks for each track
    for (int t = 0; t < sequencer.tracks && t < TRACKS_MAX; t++) {
        if (!out.tracks[t].rows.empty()) {
            // Total ticks is the tick of the last row plus any remaining wait
            const auto& lastRow = out.tracks[t].rows.back();
            out.tracks[t].totalTicks = lastRow.tick + lastRow.wait;
        }
    }

    // Check if we have any data
    bool hasData = false;
    for (int t = 0; t < out.numTracks; t++) {
        if (!out.tracks[t].rows.empty()) {
            hasData = true;
            break;
        }
    }
    return hasData;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool TFMXDecoder::buildPatternDisplayForCurrentSong(PatternDisplayData& out) {
    if (!admin.initialized || vSongs.empty()) {
        return false;
    }

    uword songOffset = vSongs[admin.startSong] << 1;
    int stepFirst = readBEuword(pBuf, offsets.header + 0x100 + songOffset);
    int stepLast = readBEuword(pBuf, offsets.header + 0x140 + songOffset);

    return buildPatternDisplay(stepFirst, stepLast, out);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int TFMXDecoder::findRowForTick(const TrackDisplayData& trackData, uint32_t targetTick) {
    if (trackData.rows.empty()) {
        return -1;
    }

    // Binary search for the last row with tick <= targetTick
    int left = 0;
    int right = (int)trackData.rows.size() - 1;
    int result = -1;

    while (left <= right) {
        int mid = left + (right - left) / 2;
        if (trackData.rows[mid].tick <= targetTick) {
            result = mid;
            left = mid + 1;
        } else {
            right = mid - 1;
        }
    }

    return result;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Pattern display API implementations (virtual overrides for Decoder base class)

bool TFMXDecoder::buildPatternDisplay() {
    if (patternDisplayBuilt) {
        return true;
    }
    patternDisplayBuilt = buildPatternDisplayForCurrentSong(cachedPatternDisplay);
    return patternDisplayBuilt;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int TFMXDecoder::getPatternDisplayNumTracks() {
    if (!patternDisplayBuilt) {
        return 0;
    }
    return cachedPatternDisplay.numTracks;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

uint32_t TFMXDecoder::getPatternDisplayTrackRows(int track) {
    if (!patternDisplayBuilt || track < 0 || track >= cachedPatternDisplay.numTracks) {
        return 0;
    }
    return static_cast<uint32_t>(cachedPatternDisplay.tracks[track].rows.size());
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// TODO: Return a struct instead. Also, do we really need a virtual interface? We only have one implementation....

bool TFMXDecoder::getPatternDisplayRow(int track, uint32_t row, uint8_t* type, uint8_t* note, uint8_t* macro,
                                       uint8_t* volume, int8_t* detune, uint8_t* channel, uint16_t* wait,
                                       uint32_t* tick) {
    if (!patternDisplayBuilt || track < 0 || track >= cachedPatternDisplay.numTracks) {
        return false;
    }

    const TrackDisplayData& trackData = cachedPatternDisplay.tracks[track];
    if (row >= trackData.rows.size()) {
        return false;
    }

    const PatternDisplayRow& r = trackData.rows[row];
    if (type)
        *type = r.type;
    if (note)
        *note = r.note;
    if (macro)
        *macro = r.macro;
    if (volume)
        *volume = r.volume;
    if (detune)
        *detune = r.detune;
    if (channel)
        *channel = r.channel;
    if (wait)
        *wait = r.wait;
    if (tick)
        *tick = r.tick;

    return true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int TFMXDecoder::findPatternDisplayRowForTick(int track, uint32_t targetTick) {
    if (!patternDisplayBuilt || track < 0 || track >= cachedPatternDisplay.numTracks) {
        return -1;
    }
    return findRowForTick(cachedPatternDisplay.tracks[track], targetTick);
}

} // namespace tfmxaudiodecoder
