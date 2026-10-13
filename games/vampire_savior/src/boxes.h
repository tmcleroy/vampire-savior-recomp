// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy
//
// Vampire Savior: the boxes the game tests for hits and for the players pushing each other.
#pragma once

#include "machine.h"
#include "vsav.h"

#include <vector>

namespace vsav {

// A box as the ROM has it: its middle, measured from where the object stands (x towards the way it faces, y
// upwards), and half its width and height
struct Box {
    int16_t x, y, halfWidth, halfHeight;
};
Box ReadBox(cps2recomp::Machine &machine, uint32_t address);

enum class BoxKind { Head, Body, Legs, Push, Attack };
// A box in use, placed in the stage: its middle and half sizes
struct PlacedBox {
    BoxKind kind;
    int x, y, halfWidth, halfHeight;
    bool thrown; // it belongs to something thrown, not to a player
};
// The boxes of the object whose record is at `object`, as they are now
void ObjectBoxes(cps2recomp::Machine &machine, uint32_t object, bool thrown, std::vector<PlacedBox> &boxes);
// Of every player record in use and everything thrown
std::vector<PlacedBox> AllBoxes(cps2recomp::Machine &machine);

} // namespace vsav
