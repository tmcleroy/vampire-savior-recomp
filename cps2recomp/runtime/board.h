// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of cps2recomp.
//
// The board's services at the level of Capcom's own system routines: where every CPS-2 game has the same
// routine to talk to a part of the board, a game's sources can name its address and have the port do the
// routine's work directly, with no talking to the part.
#pragma once

#include "lift.h"

#include <cstdint>

namespace cps2recomp {

class Eeprom;
void SetEeprom(Eeprom *eeprom); // (the port says where the settings memory is)

// The settings memory. The game reads and writes it some words at a time, with two routines that work the
// memory chip's lines bit by bit. Both take what to do from variables (distances from A5): a long with the
// address of the game's copy of the settings, a word with the place in it to start (in bytes: so the first
// word of the memory read or written is that halved), a word with the number of words less one, and a byte
// that is set while the lines are in use. The write returns nothing (0) in D0 when it has worked.
struct SettingsMemory {
    uint32_t readWords, writeWords;     // the two routines
    int32_t buffer, start, count, busy; // the variables
    // The time the game's routines take, in the 68000's cycles: a call, and each word (the check measures it)
    // (as measured for calls of 64 words and of 2: the routines are the same code in every game)
    uint32_t readTime = 115, readWordTime = 8302, writeTime = 4151, writeWordTime = 15712;
};
void AddSettingsMemory(const SettingsMemory &memory);

} // namespace cps2recomp
