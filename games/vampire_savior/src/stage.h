// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy
//
// Vampire Savior: a stage's scroll screens ("layers"), as the rewrites of the game's stage code share them
// (camera.cpp: the stage's own, which is the camera; planes.cpp: the other two, placed from it).
#pragma once

#include "vsav.h"

namespace cps2recomp {

// The three scroll screens' own parts of the code that places one: which bit of a place says a strip's
// line has been crossed (a strip is two tiles wide), the routines that draw the strip coming into the
// picture, and where the game keeps the scroll registers' values to be (from A5) and how wide they are
struct Layer {
    uint8_t strip;
    uint32_t goingTo, column, row, drawsAcross, drawsUp;
    int32_t registerX, registerY;
    uint16_t registerMask;
};
extern const Layer kLayer8, kLayer16, kLayer32;

// The layer whose record is at A6 is put at the place in D0, across or up, and its shake added; the strip
// of its picture that comes in is drawn into the map when the record says its picture is drawn as it goes
// (01DEEA and 01DF70, 01E450 and 01E4A0, 01E7F6 and 01E876)
void PlaceLayerAcross(M68K &c, const Layer &layer);
void PlaceLayerUp(M68K &c, const Layer &layer);

} // namespace cps2recomp
