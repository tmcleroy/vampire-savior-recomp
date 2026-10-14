// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy
//
// Vampire Savior: what the game's own sources add to the port (see PROGRESS.md).
#include "game.h"
#include "modes.h"

namespace cps2recomp {

void RegisterCheats();   // cheats.cpp
void CheatsFrame(Machine &machine);
void RegisterCollision(); // collision.cpp
void RegisterFight();     // fight.cpp
void RegisterInput();     // input.cpp
void RegisterChoosing();  // choosing.cpp
void RegisterCamera();    // camera.cpp
void RegisterPlanes();    // planes.cpp
void RegisterRows();      // rows.cpp
void RegisterPicture();   // picture.cpp
void RegisterEdges();     // edges.cpp
void RegisterPieces();    // pieces.cpp
void RegisterFrames();    // frames.cpp
void RegisterTraining(); // training.cpp
void TrainingBeforeFrame(Machine &machine);
void TrainingFrame(Machine &machine);
void TrainingState(Archive &ar);
void RegisterModes(); // modes.cpp
void ModesBeforeFrame(Machine &machine);
void ModesFrame(Machine &machine);
void ModesState(Archive &ar);
void RegisterModel(); // model.cpp
void ModelBeforeFrame(Machine &machine);
void ModelFrame(Machine &machine);
void ModelState(Archive &ar);
bool ModelViewer();
void RegisterMachines(); // machines.cpp
void RegisterBattle();   // battle.cpp
void BattleBeforeFrame(Machine &machine);
void BattleFrame(Machine &machine);
void BattleState(Archive &ar);
void RegisterWidescreen(); // widescreen.cpp
void WidescreenBeforeFrame(Machine &machine);
void RegisterPalettes(); // palettes.cpp
void PalettesBeforeFrame(Machine &machine);

void RegisterGame() {
    RegisterModes();
    RegisterBattle();
    RegisterModel();
    RegisterMachines();
    RegisterCheats();
    RegisterTraining();
    RegisterCollision();
    RegisterFight();
    RegisterInput();
    RegisterChoosing();
    RegisterCamera();
    RegisterPlanes();
    RegisterRows();
    RegisterPicture();
    RegisterEdges();
    RegisterPieces();
    RegisterFrames();
    RegisterWidescreen();
    // The training aids work only in a training session (the dummy's are the toolkit's: fight::RegisterDummy)
    TrainingAids({"damage_readout", "frame_data", "frame_log", "frame_meter", "health_refill", "hit_boxes", "input_display"});
    TheGame().inTraining = [] { return FeatureOn("training_mode"); };
    RegisterPalettes();
    TheGame().beforeFrame = [](Machine &machine) {
        PalettesBeforeFrame(machine);
        // (first: the modes below may run other machines' frames, which are never wide)
        WidescreenBeforeFrame(machine);
        TrainingBeforeFrame(machine);
        ModesBeforeFrame(machine);
        BattleBeforeFrame(machine);
        ModelBeforeFrame(machine);
    };
    // (the training aids see what the frame did before the cheats put things back)
    TheGame().frame = [](Machine &machine) {
        TrainingFrame(machine);
        ModesFrame(machine);
        BattleFrame(machine);
        ModelFrame(machine);
        CheatsFrame(machine);
    };
    // (run-ahead: not in the modes, whose workings a save state does not wholly hold)
    TheGame().noRunAhead = [] { return AttractMode() || StageViewer() || BattleMode() || ModelViewer(); };
    TheGame().state = [](Archive &ar) {
        TrainingState(ar);
        ModesState(ar);
        BattleState(ar);
        ModelState(ar);
    };
}

} // namespace cps2recomp
