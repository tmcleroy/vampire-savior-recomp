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

// The records of the scroll screens an object can ride on, by the byte kIsRiding of its record (0 none, then
// 4, 8, 12 for the three). An object that rides none is looked up at 000030, in the ROM, as the game does:
// its place is its script's business.
constexpr uint32_t kIsRiding = 0x0C;
constexpr uint32_t kLayerRecords[4] = {0x000030, vsav::kBackLayer, vsav::kCameraRecord, vsav::kFarLayer};

// The layer whose record is at A6 is put at the place in D0, across or up, and its shake added; the strip
// of its picture that comes in is drawn into the map when the record says its picture is drawn as it goes
// (01DEEA and 01DF70, 01E450 and 01E4A0, 01E7F6 and 01E876)
void PlaceLayerAcross(M68K &c, const Layer &layer);
void PlaceLayerUp(M68K &c, const Layer &layer);

// A wide picture and what goes by more slowly than the camera. The board paints a plane, and a row of the
// stage's own, as far as its picture shows them with the camera at its ends. A wide picture's camera stops
// 64 short of those ends and shows 64 more, so anything slower than the camera would be seen beyond its
// painting there (a black band at the stage's end). What is slower is therefore that much slower again:
// how far it lags behind the camera is made greater in the proportion of the board's camera's travel to
// the wide one's (three to two on most stages), so that at the wide camera's ends the wide picture's edges
// are where the board's picture's edges are at its ends. Given a lag (any units), the lag for the picture
// being played in: the same on the board.
int64_t WideLag(int64_t lag);

// How far right of the board's picture's left edge the object is, where it is drawn (the place the list's
// making gives it: its scroll screen's, and its row's when it rides one). False for an object that rides
// no scroll screen. Nothing of the CPU's is changed.
bool AcrossInPicture(M68K &c, uint32_t object, int &across);

} // namespace cps2recomp
