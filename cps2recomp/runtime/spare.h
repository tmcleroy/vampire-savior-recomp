// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of cps2recomp.
//
// Machines kept as save states ("spares") and run beside the port's own, for a game's modes that show or prepare
// more than one game at once (a stage scrolling into the next, the next fight made ready while the last one ends):
// a spare is loaded into the port's machine, run a frame and put away again, within the port's frame.
#pragma once

#include "machine.h"

#include <cstdint>
#include <functional>
#include <vector>

namespace cps2recomp {

// A frame's picture and sound, taken out of the machine to be put together with another's
struct Made {
    std::vector<uint32_t> picture;
    std::vector<int16_t> sound;
};
// The frame the machine has just run: its picture, and the sound made since `soundBefore` samples (which is taken
// out of the machine's)
void Take(Machine &machine, size_t soundBefore, Made &made);
// ... and what is to be seen and heard for the frame put in
void Give(Machine &machine, const Made &made);

// A machine kept as a save state
struct Spare {
    std::vector<uint8_t> state; // (empty: not yet switched on)
    bool ready = false;         // what it was being run for has come (its round is on, say)
    int32_t frames = 0;         // frames run on the way there
    void Clear() {
        state.clear();
        ready = false;
        frames = 0;
    }
};

// What a game's sources keep of their own around a spare's frame (what a save state does not hold: what they want
// of the machine that is running, their picture's switches): `keep` before the spare is loaded, `fresh` with the
// spare loaded and before its frame, `putBack` when the port's machine is back (with its video's switches as they
// were)
struct SpareKeeping {
    std::function<void(Machine &)> keep, fresh, putBack;
};
void SetSpareKeeping(const SpareKeeping &keeping);

// Runs the spare for a frame in the port's machine, which is left as it was (its picture, controls and video's
// switches too). `before` is called with the spare loaded, before its frame; `made`, if given, gets the frame's
// picture and sound; `after`, if given, is called after the frame with the spare still loaded.
void RunSpare(Machine &machine, Spare &spare, const std::function<void(Machine &)> &before, Made *made = nullptr,
              const std::function<void(Machine &)> &after = nullptr);
// Whether a spare has run since this was last cleared (the port clears it before each frame): a frame with a
// spare in it is not run ahead (a game's sources keep more around a spare than its save state holds)
bool SpareRan();
void ClearSpareRan();
// The spare's machine becomes the port's (and the spare is cleared)
void Promote(Machine &machine, Spare &spare);

} // namespace cps2recomp
