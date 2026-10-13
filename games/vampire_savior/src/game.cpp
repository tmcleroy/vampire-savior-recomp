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
void RegisterMachines(); // machines.cpp
void RegisterBattle();   // battle.cpp
void BattleBeforeFrame(Machine &machine);
void BattleFrame(Machine &machine);
void BattleState(Archive &ar);
void RegisterWidescreen(); // widescreen.cpp
void WidescreenBeforeFrame(Machine &machine);

void RegisterGame() {
    RegisterModes();
    RegisterBattle();
    RegisterMachines();
    RegisterCheats();
    RegisterTraining();
    RegisterCollision();
    RegisterFight();
    RegisterInput();
    RegisterFrames();
    RegisterWidescreen();
    TheGame().beforeFrame = [](Machine &machine) {
        // (first: the modes below may run other machines' frames, which are never wide)
        WidescreenBeforeFrame(machine);
        TrainingBeforeFrame(machine);
        ModesBeforeFrame(machine);
        BattleBeforeFrame(machine);
    };
    // (the training aids see what the frame did before the cheats put things back)
    TheGame().frame = [](Machine &machine) {
        TrainingFrame(machine);
        ModesFrame(machine);
        BattleFrame(machine);
        CheatsFrame(machine);
    };
    TheGame().state = [](Archive &ar) {
        TrainingState(ar);
        ModesState(ar);
        BattleState(ar);
    };
}

} // namespace cps2recomp
