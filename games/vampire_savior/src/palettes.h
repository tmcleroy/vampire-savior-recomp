// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy
//
// Vampire Savior: the characters' colours as palettes.cpp keeps them, for the colour editor. A colour is the
// board's 12 bits (4 each of red, green, blue: 0x0RGB); `line` is one of a set's four palettes (vsav.h).
#pragma once

#include <cstdint>

namespace cps2recomp {

// The character's name as files have it ("" for a number with no colours of its own)
const char *ColourName(int number);
// Where the character's set is in the ROM (what the game keeps at kColourSet)
uint32_t ColourSetAddress(int number, int set);
// A colour as it is now (with the files' changes and the editor's), and as the game has it
uint16_t Colour(int number, int set, int line, int n);
uint16_t GameColour(int number, int set, int line, int n);
void SetColour(int number, int set, int line, int n, uint16_t colour);
// Writes the character's colours that are not the game's to its file in the palettes folder (removes the file
// when they all are); false if there is no folder to write to
bool SaveColours(int number);

} // namespace cps2recomp
