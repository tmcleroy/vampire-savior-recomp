// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy
//
// Vampire Savior: cheats and free play. Optional features (the launcher's Features page and the in-game menu
// switch them). The ones that change damage and the clock are in the game's rules as written again in
// fight.cpp; the rest work on the game's memory after every frame.
#include "vsav.h"

#include "board.h"

namespace cps2recomp {

bool AttractMode(); // modes.cpp

namespace {
    using namespace vsav;

    // A round is being fought when both players' health is a sensible amount (the records hold other things
    // outside fights)
    bool Fighting() {
        for (int n = 0; n < 2; n++) {
            if (Ram16(Player(n) + kHealth) > kHealthFull || Ram16(Player(n) + kHealthCeiling) > kHealthFull) {
                return false;
            }
        }
        const uint8_t clock = Ram8(kClock);
        return clock > 0 && clock <= kClockFull;
    }

    void Frame(Machine &) {
        // (not in attract mode: with a coin in, the game waits for Start and shows nothing else)
        if (FeatureOn("free_play") && !AttractMode() && Ram8(kCredits) < 2) {
            SetRam8(kCredits, 2); // (two: a second player can always join)
        }
        if (!Fighting()) {
            return;
        }
        static const char *const kFullGauge[2] = {"full_gauge", "full_gauge_2"};
        for (int n = 0; n < 2; n++) {
            if (FeatureOn(kFullGauge[n])) {
                SetRam8(Player(n) + kStocks, kStocksFull);
            }
        }
    }
} // namespace

void CheatsFrame(Machine &machine) {
    Frame(machine);
}

void RegisterCheats() {
    // The settings memory, read and written directly (board.h). The two routines are Capcom's for every
    // CPS-2 game; these are where this game has them and their variables.
    SettingsMemory memory;
    memory.readWords = HookAt(0x00214C);
    memory.writeWords = HookAt(0x002214);
    memory.buffer = -0x729A;
    memory.count = -0x7294;
    memory.start = -0x7290;
    memory.busy = 0x87;
    AddSettingsMemory(memory);
    Game &game = TheGame();
    game.features.push_back({"free_play", "Game", "Free play", "Start works without a coin.", kFeatureEnhanced});
    game.features.push_back({"no_damage", "Cheats", "No damage", "Player 1's health does not go down."});
    game.features.push_back({"no_damage_2", "Cheats", "No damage, player 2", "Player 2's health does not go down (the opponent's, in a one-player game)."});
    game.features.push_back({"one_hit", "Cheats", "One-hit knockouts", "Player 2 (the opponent, in a one-player game) goes down with the first hit."});
    game.features.push_back({"full_gauge", "Cheats", "Full gauge", "Player 1 always has special gauges to spend."});
    game.features.push_back({"full_gauge_2", "Cheats", "Full gauge, player 2", "Player 2 always has special gauges to spend."});
    game.features.push_back({"infinite_time", "Cheats", "Infinite time", "The round's clock stands still."});
}

} // namespace cps2recomp
