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
// A machine other than the port's is loaded and about to run a frame (modes.h, RunSpare): its picture is to
// be as wide as the port's this frame, whatever it is showing, so that the two can be put together
void WideForSpare(Machine &machine);
// ... and the port's own machine is back, with its video's `wide` as it was
void WideForPort(Machine &machine);
// From the camera's own routine (camera.cpp), with the camera's record: its ends put where the picture
// being played in wants them
void WideCameraEnds(uint32_t camera);

} // namespace cps2recomp
