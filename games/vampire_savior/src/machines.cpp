// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy
//
// Vampire Savior: what the modes share (modes.h): getting a machine to a round, the camera put where a mode wants
// it, and telling objects apart (the machines kept as save states and run beside the port's are the toolkit's:
// runtime/spare.h).
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
    int g_noShadow = -1; // the player whose shadow is not drawn (-1: nobody)

    // The object list counts across in 1024 columns, and a player's shadow is drawn this far on from where the
    // player stands in the picture (found from the list: Morrigan standing at 360 in the picture, her shadow's
    // pieces from 384 to 464). A player far off the picture can have its shadow come round into it.
    constexpr int kShadowColumns = 0x400, kShadowOn = 64;
    constexpr uint16_t kOffPicture = 0x300; // (a column the picture never shows, wide or not)
    int ShadowColumn(int player) {
        return (static_cast<int16_t>(Ram16(Player(player) + kX)) - static_cast<int16_t>(Ram16(kCameraX)) + kShadowOn) & (kShadowColumns - 1);
    }
    int Apart(int a, int b) {
        const int d = (a - b) & (kShadowColumns - 1);
        return std::min(d, kShadowColumns - d);
    }
    // When the board takes the object list (Video::objectsTaken): the pieces of shadow nearer the player whose
    // shadow is not drawn than the other player are put where the picture never shows
    void DropShadow(uint16_t *list, int &last) {
        if (g_noShadow < 0) {
            return;
        }
        const int mine = ShadowColumn(g_noShadow), other = ShadowColumn(1 - g_noShadow);
        for (int i = 0; i <= last; i += 4) {
            const uint32_t tile = list[i + 2] + ((list[i + 1] & 0x6000u) << 3);
            const uint16_t attributes = list[i + 3];
            if (!IsShadow(tile, attributes)) {
                continue;
            }
            const int across = ((attributes >> 8) & 0x0F) + 1;
            const int middle = (list[i] + across * 8) & (kShadowColumns - 1);
            if (Apart(middle, mine) < Apart(middle, other)) {
                list[i] = static_cast<uint16_t>((list[i] & ~(kShadowColumns - 1)) | kOffPicture); // (its level against the scroll screens kept)
            }
        }
    }
} // namespace

void NoShadow(Machine &machine, int player) {
    g_noShadow = player;
    machine.video.objectsTaken = player >= 0 ? DropShadow : nullptr;
}

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

void RegisterMachines() {
    // Around a spare's frame (runtime/spare.h): what is wanted of the machine that is running kept and fresh for it,
    // and its picture as wide as the port's (widescreen.h)
    static MatchWanted wanted;
    static CameraWanted camera;
    static int noShadow;
    SetSpareKeeping({[](Machine &) { wanted = g_wanted; camera = g_camera; noShadow = g_noShadow; },
                     [](Machine &machine) {
                         g_wanted = MatchWanted();
                         g_camera = CameraWanted();
                         NoShadow(machine, -1);
                         WideForSpare(machine);
                     },
                     [](Machine &machine) {
                         WideForPort(machine);
                         g_wanted = wanted;
                         g_camera = camera;
                         NoShadow(machine, noShadow);
                     }});
}

} // namespace cps2recomp
