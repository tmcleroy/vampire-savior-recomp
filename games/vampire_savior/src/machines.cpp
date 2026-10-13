// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy
//
// Vampire Savior: what the modes share (modes.h): machines kept as save states and run beside the port's,
// getting a machine to a round, the camera put where a mode wants it, and telling objects apart.
#include "modes.h"

#include "state.h"
#include "widescreen.h"

#include <algorithm>
#include <cstring>

namespace cps2recomp {

namespace {
    using namespace vsav;

    MatchWanted g_wanted;
    CameraWanted g_camera;
    std::vector<uint8_t> g_mine; // the port's machine, while a spare runs in it

    // 01E36E, 01E3F2. The camera works out where it wants to be, across and up, from where the players are.
    // When a mode wants it somewhere, it goes there instead, no faster than the game lets it go (the stage's
    // tiles are drawn in as it moves, a column or a row at a time).
    bool CameraAcross(M68K &c) {
        WideCameraEnds(c.a[6] & 0xFFFFFF);
        if (g_camera.x < 0) {
            return false;
        }
        const uint32_t camera = c.a[6] & 0xFFFFFF;
        const int now = static_cast<int16_t>(Ram16(camera + kCameraWantsX)), most = std::max<int>(Ram16(camera + kCameraMostX), 1);
        const int to = now + std::clamp(g_camera.x - now, -most, most);
        SetRam16(camera + kCameraWantsX, static_cast<uint16_t>(to));
        SetLow16(c.d[0], static_cast<uint16_t>(to));
        c.pc = kCameraAcrossSet;
        return true;
    }
    bool CameraUp(M68K &c) {
        if (g_camera.y < 0) {
            return false;
        }
        const uint32_t camera = c.a[6] & 0xFFFFFF;
        const int now = static_cast<int16_t>(Ram16(camera + kCameraWantsY)), most = std::max<int>(Ram16(camera + kCameraMostY), 1);
        const int to = now + std::clamp(g_camera.y - now, -most, most);
        SetRam16(camera + kCameraWantsY, static_cast<uint16_t>(to));
        SetLow16(c.d[0], static_cast<uint16_t>(to));
        c.pc = kCameraUpSet;
        return true;
    }
} // namespace

MatchWanted &Wanted() {
    return g_wanted;
}
CameraWanted &CameraAt() {
    return g_camera;
}

bool RoundFought() {
    return Ram16(kMode) == 2 && Ram16(kGameStep) == kGameMatch && Ram16(kMatchStep) == kMatchRound && Ram16(kRoundStep) == kRoundFought;
}

bool ToRound(Machine &machine, int32_t &frames, bool choose) {
    if (RoundFought()) {
        return true;
    }
    if (Ram8(kCredits) < 2) {
        SetRam8(kCredits, 2);
    }
    const bool press = frames++ % 16 < 6;
    const bool both = Ram8(kPlayersIn) == 3;
    const uint16_t one = machine.players[0];
    machine.players[0] = machine.players[1] = !press ? 0 : both ? kPunch1 : kStart;
    if (choose && both) {
        // (player 1's own controls, but for Start and the coin, which would do other things here)
        machine.players[0] = one & static_cast<uint16_t>(~(kStart | kCoin));
    }
    return false;
}

void PlaySound(uint16_t code) {
    const uint16_t at = Ram16(kSoundQueueEnd) & 0xFF0;
    SetRam32(kSoundQueue + at, code);
    SetRam32(kSoundQueue + at + 4, 0);
    SetRam32(kSoundQueue + at + 8, 0);
    SetRam16(kSoundQueueEnd, static_cast<uint16_t>((at + 0x10) & 0xFF0));
}

bool IsShadow(uint32_t tile, uint16_t attributes) {
    return (attributes & 0x1F) == 22 && tile >= 0xF8E0 && tile < 0xF920;
}
bool NotTheStage(uint32_t tile, uint16_t attributes) {
    return (attributes & 0x1F) < 22 || IsShadow(tile, attributes);
}
bool ThePanel(uint32_t tile, uint16_t attributes) {
    // (the players' faces beside the bars are in the players' own colours: told by the panel's tiles)
    return (attributes & 0x1F) < 5 || (tile >= 0x13800 && tile < 0x14400);
}
bool NotTheFighters(uint32_t tile, uint16_t attributes) {
    return ThePanel(tile, attributes) || ((attributes & 0x1F) >= 22 && !IsShadow(tile, attributes));
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

void RunSpare(Machine &machine, Spare &spare, const std::function<void(Machine &)> &before, Made *made,
              const std::function<void(Machine &)> &after) {
    // What a save state does not hold is kept by hand: the picture, the controls, the video's switches, what
    // is wanted
    const uint16_t held[2] = {machine.players[0], machine.players[1]};
    const auto hide = machine.video.hideObject;
    const unsigned layers = machine.video.layerMask;
    const bool mark = machine.video.markObjects;
    const MatchWanted wanted = g_wanted;
    const CameraWanted camera = g_camera;
    const size_t sound = machine.audio.size();
    const Video::Wide wide = machine.video.wide;
    const size_t pixels = static_cast<size_t>(Video::kMaxWidth) * Video::kHeight; // (all of it, whatever its width)
    static std::vector<uint32_t> picture;
    picture.assign(machine.Picture(), machine.Picture() + pixels);

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
    g_wanted = MatchWanted();
    g_camera = CameraWanted();
    WideForSpare(machine);
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
    WideForPort(machine);
    g_wanted = wanted;
    g_camera = camera;
}

void Promote(Machine &machine, Spare &spare) {
    if (!spare.state.empty()) {
        Archive ar(spare.state, false);
        machine.Serialize(ar);
    }
    spare.Clear();
}

void RegisterMachines() {
    AddHook(0x01E36E, "CameraAcross", CameraAcross);
    AddHook(0x01E3F2, "CameraUp", CameraUp);
}

} // namespace cps2recomp
