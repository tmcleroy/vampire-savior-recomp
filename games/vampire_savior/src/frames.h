// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy
//
// Vampire Savior: frame data (frames.cpp).
#pragma once

#include "vsav.h"

#include <vector>

namespace cps2recomp {

// What a player is doing at a step of the game
enum class Doing : uint8_t {
    Free,      // can do anything
    Startup,   // in a move, before it can hit
    Active,    // in a move that can hit now
    Recovery,  // in a move, after it could hit
    HitStun,   // reeling from a hit
    GuardStun, // reeling from a guarded hit
    Air,       // jumping (and not attacking)
    Held,      // being thrown, and the other things done to a player
};

void RegisterFrames();
// A hit has landed on a player, or been guarded
void FramesContact(int victim, bool guarded);
Doing FramesDoing(int player);
void FramesReset();
void FramesState(Archive &ar);
// What to show: the numbers as lines of text, the meter as boxes
void FramesHud(bool numbers, bool meter, std::vector<HudLine> &lines, std::vector<HudBox> &boxes);

} // namespace cps2recomp
