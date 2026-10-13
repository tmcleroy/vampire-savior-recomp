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
void RegisterModes(); // modes.cpp
void ModesBeforeFrame(Machine &machine);
void ModesFrame(Machine &machine);
void ModesState(Archive &ar);

void RegisterGame() {
    RegisterModes(); // (first: its page is the menu's first)
    RegisterCheats();
    RegisterTraining();
    RegisterCollision();
    RegisterFight();
    RegisterInput();
    RegisterFrames();
    TheGame().beforeFrame = [](Machine &machine) {
        TrainingBeforeFrame(machine);
        ModesBeforeFrame(machine);
    };
    // (the training aids see what the frame did before the cheats put things back)
    TheGame().frame = [](Machine &machine) {
        TrainingFrame(machine);
        ModesFrame(machine);
        CheatsFrame(machine);
    };
    TheGame().state = [](Archive &ar) {
        TrainingState(ar);
        ModesState(ar);
    };
}

} // namespace cps2recomp
