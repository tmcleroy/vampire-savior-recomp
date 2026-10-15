// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy
//
// Vampire Savior: "Straight to the title" (the Game page). When the board starts (switched on, or reset from
// the menu), everything before the title is run unseen and unheard, as fast as it goes: the board's memory
// test, the version and the warning, the sound notice and the maker's, and the introduction. The picture
// comes back with the title, a second or so later; nothing of the game is changed, only not shown.
//
// The game's step (vsav.h, kMode) is 0 only while the board starts. It then goes 2 (the version), 4 (the
// warning), 6, 0 again, and into the loop of its show: 0C, 0E, 10 (the notices), ..., 18 the introduction,
// whose own step (kGameStep) is 10 when the title comes. With a coin in (free play puts them in) it goes
// from that second 0 to 2 and 4 instead: the title that asks for Start. A step past the introduction ends
// the skipping too. Only the board's start begins it (Machine::frame is 0 again at a reset): the step is 0 for a
// few frames between the screens of a game too. The modes that reset the board to get somewhere of their own
// skip it themselves.
#include "modes.h"

namespace cps2recomp {

bool ModelViewer(); // model.cpp

namespace {
    using namespace vsav;

    constexpr uint16_t kBooted = 0x06, kShow = 0x0C, kIntroduction = 0x18, kTitle = 0x10;
    constexpr int kSkipMost = 6000; // frames after which the skipping is given up

    bool g_skipping = false;
    bool g_booted = false; // the board's own start (the version and the warning) is over
    bool g_fast = false; // this file asked for the game to run unseen
    int g_frames = 0;

    void Fast(bool on) {
        if (on != g_fast) {
            g_fast = on;
            SetFastForward(on);
        }
    }

    // A mode of the menu is on: it starts the board for itself
    bool InAMode() {
        return AttractMode() || StageViewer() || BattleMode() || ModelViewer() || FeatureOn("training_mode");
    }
} // namespace

void TitleFrame(Machine &machine) {
    // (a mode started: its start is its own, and so is running unseen)
    if (InAMode()) {
        g_skipping = g_fast = false;
        return;
    }
    const uint16_t mode = Ram16(kMode);
    // (only the board's first frame: the step is 0 again between screens of a game, and a game must never be
    // run unseen)
    if (machine.frame <= 1 && mode == 0 && !g_skipping) {
        g_skipping = FeatureOn("skip_intro");
        g_booted = false;
        g_frames = 0;
    }
    if (g_skipping) {
        g_booted = g_booted || mode == kBooted;
        const bool title = (mode == kIntroduction && Ram16(kGameStep) >= kTitle) || (g_booted && mode != kBooted && mode != 0 && mode < kShow);
        if (title || mode > kIntroduction || ++g_frames > kSkipMost) {
            g_skipping = false;
        }
    }
    Fast(g_skipping);
}

void RegisterTitle() {
    TheGame().features.push_back({"skip_intro", "Game", "Straight to the title",
                                  "When the game starts, and when it is reset, goes straight to the title: the board's memory test, "
                                  "the notices and the introduction are run unseen, in a second or so.",
                                  0});
}

} // namespace cps2recomp
