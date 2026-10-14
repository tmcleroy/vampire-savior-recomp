// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy
//
// Vampire Savior: frame data (frames.cpp; the counting is the toolkit's fight module, fight.h).
#pragma once

#include "fight.h"
#include "vsav.h"

#include <vector>

namespace cps2recomp {

// What a player is doing at a step of the game
using Doing = fight::Doing;

void RegisterFrames();
// A hit has landed on a player, or been guarded
void FramesContact(int victim, bool guarded);
Doing FramesDoing(int player);
void FramesReset();
void FramesState(Archive &ar);
// What to show: the numbers as lines of text, the meter as boxes
void FramesHud(bool numbers, bool meter, std::vector<HudLine> &lines, std::vector<HudBox> &boxes);

} // namespace cps2recomp
