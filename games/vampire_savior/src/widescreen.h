// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy
//
// Vampire Savior: the widescreen picture (widescreen.cpp), for what else draws over the picture.
#pragma once

namespace cps2recomp {

// How many more columns the picture last made has on each side of the board's 384 (0: the board's picture)
int WideSide();
// ... and how far to the right of where the board would draw it something on the stage is in that picture
// (the columns added on the left, and how far the view was kept from the stage's end)
int WideStage();

} // namespace cps2recomp
