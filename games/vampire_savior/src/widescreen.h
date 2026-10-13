// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy
//
// Vampire Savior: the widescreen picture (widescreen.cpp), for what else draws over the picture.
#pragma once

#include "lift.h"
#include "machine.h"

#include <cstdint>

namespace cps2recomp {

// How many more columns the picture last made has on each side of the board's 384 (0: the board's picture)
int WideSide();
// ... and how far to the right of where the board would draw it something on the stage is in that picture
// (the columns added on the left, and how far the view was kept from the stage's end)
int WideStage();

// Whether fights are played in the wide picture now (the feature is on, and the mode shows one). Then the
// game is changed too: the players are kept inside the wide picture, not the board's, and the camera stops
// 64 short of where it did, so that the wide picture ends where the stage does.
bool WidePlay();
// How many columns the picture fights are played in has on each side of the board's (64, or 0): the picture's
// left edge is that far left of the camera's place, and the camera stops that far short of each of its ends
int WideMargin();
// Which of the stage's planes each entry of a machine's object list rides on, as the game said while it
// made the list (widescreen.cpp). It belongs to a machine and is not in its save state, so whoever keeps
// more than one machine keeps one of these for each.
struct WidePlanes {
    uint8_t rides[0x400] = {}; // 0: not known; 1-3: the scroll screen of 8-, 16-, 32-pixel tiles; 4: none (the panel)
    int last = 0;              // the entry the game last said it of
    int16_t planeX[3] = {}, planeY[3] = {}; // where each plane was when the list was made
    // A record of the stage's that is one of two pieces, by the camera's side of a place (widescreen.cpp):
    // each piece's entries as the game last put them in the list, their places counted from the plane's;
    // and in the list being made, which piece the record is, and where its entries are
    static constexpr int kTwins = 4, kTwinEntries = 32;
    struct Twin {
        uint32_t record = 0; // (0: none)
        uint8_t stateAt = 0; //   the byte of the record that says which piece it is
        uint8_t plane = 0;   //   the plane it rides on (1-3)
        struct Piece {
            int count = 0;
            uint16_t entries[kTwinEntries][4] = {};
        } piece[2];
        int is = -1, at = 0, count = 0;
    } twins[kTwins];
    int filling = -1; // the twin whose entries the game is putting in the list just now
};
// A machine other than the port's is loaded and about to run a frame (modes.h, RunSpare): its picture is to
// be as wide as the port's this frame, whatever it is showing, so that the two can be put together.
// `planes` is that machine's.
void WideForSpare(Machine &machine, WidePlanes &planes);
// ... and the port's own machine is back, with its video's `wide` as it was (`planes` takes the other's)
void WideForPort(Machine &machine, WidePlanes &planes);
// ... or the other machine has become the port's
void WidePromoted(const WidePlanes &planes);
// From the hook on the game's routine that keeps a player inside the picture (battle.cpp has it), when the
// game's routine is to run: the picture it keeps the player in is the wide one
void WideKeepIn(M68K &c);
// From the hook on the camera's own routine (machines.cpp), with the camera's record: its ends put where
// the picture being played in wants them
void WideCameraEnds(uint32_t camera);

} // namespace cps2recomp
