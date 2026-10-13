// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy
//
// Vampire Savior: what the game's own sources add to the port (see PROGRESS.md).
#include "game.h"

namespace cps2recomp {

void RegisterCheats();   // cheats.cpp
void CheatsFrame(Machine &machine);
void RegisterCollision(); // collision.cpp
void RegisterFight();     // fight.cpp
void RegisterInput();     // input.cpp
void RegisterFrames();    // frames.cpp
void RegisterTraining(); // training.cpp
void TrainingBeforeFrame(Machine &machine);
void TrainingFrame(Machine &machine);
void TrainingState(Archive &ar);

void RegisterGame() {
    RegisterCheats();
    RegisterTraining();
    RegisterCollision();
    RegisterFight();
    RegisterInput();
    RegisterFrames();
    TheGame().beforeFrame = TrainingBeforeFrame;
    // (the training aids see what the frame did before the cheats put things back)
    TheGame().frame = [](Machine &machine) {
        TrainingFrame(machine);
        CheatsFrame(machine);
    };
    TheGame().state = TrainingState;
}

} // namespace cps2recomp
