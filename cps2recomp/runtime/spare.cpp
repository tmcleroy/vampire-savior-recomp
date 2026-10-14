// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of cps2recomp.
//
// Spare machines (spare.h).
#include "spare.h"

#include "state.h"

#include <algorithm>
#include <cstring>

namespace cps2recomp {

namespace {
    SpareKeeping g_keeping;
    std::vector<uint8_t> g_mine; // the port's machine, while a spare runs in it
} // namespace

void SetSpareKeeping(const SpareKeeping &keeping) {
    g_keeping = keeping;
}

void Take(Machine &machine, size_t soundBefore, Made &made) {
    const size_t pixels = static_cast<size_t>(machine.PictureWidth()) * Video::kHeight;
    made.picture.assign(machine.Picture(), machine.Picture() + pixels);
    soundBefore = std::min(soundBefore, machine.audio.size());
    made.sound.assign(machine.audio.begin() + static_cast<std::ptrdiff_t>(soundBefore), machine.audio.end());
    machine.audio.resize(soundBefore);
}

void Give(Machine &machine, const Made &made) {
    memcpy(machine.Picture(), made.picture.data(), made.picture.size() * sizeof(uint32_t));
    machine.audio.insert(machine.audio.end(), made.sound.begin(), made.sound.end());
}

namespace {
    bool g_spareRan = false;
}
bool SpareRan() {
    return g_spareRan;
}
void ClearSpareRan() {
    g_spareRan = false;
}

void RunSpare(Machine &machine, Spare &spare, const std::function<void(Machine &)> &before, Made *made,
              const std::function<void(Machine &)> &after) {
    g_spareRan = true;
    // What a save state does not hold is kept by hand: the picture, the controls, the video's switches
    const uint16_t held[2] = {machine.players[0], machine.players[1]};
    const auto hide = machine.video.hideObject;
    const unsigned layers = machine.video.layerMask;
    const bool mark = machine.video.markObjects;
    const Video::Wide wide = machine.video.wide;
    const size_t sound = machine.audio.size();
    const size_t pixels = static_cast<size_t>(Video::kMaxWidth) * Video::kHeight; // (all of it, whatever its width)
    static std::vector<uint32_t> picture;
    picture.assign(machine.Picture(), machine.Picture() + pixels);
    if (g_keeping.keep) {
        g_keeping.keep(machine);
    }

    g_mine.clear();
    {
        Archive ar(g_mine, true);
        machine.Serialize(ar);
    }
    if (spare.state.empty()) {
        machine.Reset();
        spare.ready = false;
        spare.frames = 0;
    } else {
        Archive ar(spare.state, false);
        machine.Serialize(ar);
    }
    machine.players[0] = machine.players[1] = 0;
    machine.video.hideObject = nullptr;
    machine.video.layerMask = 0xF;
    machine.video.markObjects = false;
    if (g_keeping.fresh) {
        g_keeping.fresh(machine);
    }
    before(machine);
    machine.RunFrame();
    if (after) {
        after(machine);
    }
    if (made) {
        Take(machine, sound, *made);
    }
    machine.audio.resize(sound);
    spare.state.clear();
    {
        Archive ar(spare.state, true);
        machine.Serialize(ar);
    }
    {
        Archive ar(g_mine, false);
        machine.Serialize(ar);
    }
    memcpy(machine.Picture(), picture.data(), pixels * sizeof(uint32_t));
    machine.players[0] = held[0];
    machine.players[1] = held[1];
    machine.video.hideObject = hide;
    machine.video.layerMask = layers;
    machine.video.markObjects = mark;
    machine.video.wide = wide;
    if (g_keeping.putBack) {
        g_keeping.putBack(machine);
    }
}

void Promote(Machine &machine, Spare &spare) {
    if (!spare.state.empty()) {
        Archive ar(spare.state, false);
        machine.Serialize(ar);
    }
    spare.Clear();
}

} // namespace cps2recomp
