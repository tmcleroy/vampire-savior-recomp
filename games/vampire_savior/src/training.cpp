// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy
//
// Vampire Savior: training aids. Optional features in the categories "Training" and "Dummy": a readout of the
// damage hits and combos do, a display of what the players press, health that comes back, and a second
// player that stands, crouches, jumps, guards, or does again what was recorded. Hits are counted where the
// game takes their damage off (cheats.cpp has the hook); the dummy is worked by changing what the game is
// told player 2 presses; the rest looks at the game's memory after every frame. The counting and the dummy's rules are
// the toolkit's fight module (fight.h), shared with the other CPS-2 fighting games; this file tells it the game.
#include "boxes.h"
#include "fight.h"
#include "frames.h"
#include "machine.h"
#include "modes.h"
#include "state.h"
#include "widescreen.h"
#include "vsav.h"

#include <algorithm>
#include <cstdio>
#include <string>

namespace cps2recomp {

void BootToChoice(Machine &machine);
bool AttackInRange(M68K &c, uint32_t player); // fight.cpp
bool GuardStanding(M68K &c, uint32_t player);

namespace {
    using namespace vsav;

    constexpr int kRefillAfter = 90; // frames without a hit after which health comes back

    constexpr uint16_t kSticks = kLeft | kRight | kUp | kDown;
    constexpr uint16_t kButtons = kPunch1 | kPunch2 | kPunch3 | kKick1 | kKick2 | kKick3;
    fight::InputHistory g_inputs;
    fight::Combos g_combos;
    fight::Dummy g_dummy;
    bool g_fighting = false;
    // Starts the aids afresh (a round starting, or started over), but for what the dummy was told to do
    void Afresh(bool fighting) {
        g_fighting = fighting;
        g_inputs.Clear();
        g_combos.Clear();
        g_dummy.Afresh();
        FramesReset();
    }
    // Getting to a training session from switching on: coins, then both Start buttons, until both players
    // are choosing their characters (the game runs unseen meanwhile)
    bool g_boot = false;
    int g_bootFrames = 0;
    constexpr int kBootMost = 4000; // frames after which it gives up
    // The machine as it was when the round's fighting began, for starting over
    std::vector<uint8_t> g_roundStart;
    bool g_roundFought = false;
    constexpr uint16_t kCornerApart = 120; // how far apart the players are put in a corner
    std::vector<HudBox> g_nextBoxes;   // the hit boxes for the next picture
    constexpr int kPictureFloor = 239; // the picture's line that height 0 in the stage is on, with the camera at the bottom

    bool Fighting() {
        for (int n = 0; n < 2; n++) {
            if (Ram16(Player(n) + kHealthCeiling) > kHealthFull) {
                return false;
            }
        }
        const uint8_t clock = Ram8(kClock);
        return clock > 0 && clock <= kClockFull;
    }

    // Player 2 is to the right of player 1
    bool DummyOnRight() {
        return Ram16(Player(1) + kX) >= Ram16(Player(0) + kX);
    }
    // --- a training session ---

    // The menu's "Training mode": the board switched off and on, and the way to the character choice worked
    void Start(Machine &machine) {
        EndModes();
        EndBattle();
        SetFeature("training_mode", true);
        BootToChoice(machine);
    }

    // What the controls do on the way there
    void Boot(Machine &machine) {
        uint16_t &one = machine.players[0], &two = machine.players[1];
        const bool press = g_bootFrames % 16 < 6;
        one = two = 0;
        if (Ram8(kPlayersIn) == 3 || ++g_bootFrames > kBootMost) {
            g_boot = false;
            SetFastForward(false);
        } else if (Ram8(kCredits) < 2 && !FeatureOn("free_play")) {
            one = press ? kCoin : 0;
        } else {
            one = two = press ? kStart : 0;
        }
    }

    bool RoundFought() {
        return Ram16(kMode) == 2 && Ram16(kGameStep) == kGameMatch && Ram16(kMatchStep) == kMatchRound && Ram16(kRoundStep) == kRoundFought;
    }

    // "Start over": the round as it began, with the players put where the player wants them. The stick held
    // as it is asked for chooses the place (and it is remembered): left or right for a corner, down for the
    // sides swapped, up for the middle again.
    void StartOver(Machine &machine) {
        if (g_roundStart.empty() || !FeatureOn("training_mode")) {
            return;
        }
        int place = FeatureValue("restart_at"); // 0 middle, 1 left corner, 2 right corner; +3: sides swapped
        const uint16_t stick = machine.players[0] & kSticks;
        if (stick) {
            int where = place % 3;
            bool swapped = place >= 3;
            if (stick & kUp) {
                where = 0;
                swapped = false;
            }
            if (stick & kLeft) {
                where = 1;
            } else if (stick & kRight) {
                where = 2;
            }
            if (stick & kDown) {
                swapped = !swapped;
            }
            place = where + (swapped ? 3 : 0);
            SetFeatureValue("restart_at", place);
        }
        Archive ar(g_roundStart, false);
        machine.Serialize(ar);
        uint16_t x[2] = {kStartX[0], kStartX[1]}, camera = Ram16(kCameraX);
        if (place % 3 == 1) {
            x[0] = kStageLeft;
            x[1] = kStageLeft + kCornerApart;
            camera = static_cast<uint16_t>(kCameraLeft + WideMargin()); // (the camera's end in the picture played in)
        } else if (place % 3 == 2) {
            x[0] = kStageRight - kCornerApart;
            x[1] = kStageRight;
            camera = static_cast<uint16_t>(kCameraRight - WideMargin());
        }
        if (place >= 3) {
            std::swap(x[0], x[1]);
        }
        for (int n = 0; n < 2; n++) {
            SetRam32(Player(n) + kX, static_cast<uint32_t>(x[n]) << 16);
        }
        for (const uint32_t copy : kCameraCopies) {
            SetRam16(copy, camera);
        }
        // (and the training aids start afresh; a recording being replayed from its beginning)
        Afresh(true);
    }

    // The game is choosing the stage for a match (vsav.h, kChooseStage). In a training session with a stage
    // asked for, that one is used and the game's routine is left out.
    bool ChooseStage(M68K &c) {
        // The characters a mode wants, in place of the ones chosen (two alike: the second in its other colours),
        // in their hidden forms if asked for (which have two sets of colours)
        const MatchWanted &wanted = Wanted();
        for (int n = 0; n < 2; n++) {
            if (wanted.characters[n] >= 0) {
                const int character = HiddenForm(n, wanted.characters[n]);
                SetRam8(Player(n) + kCharacter, static_cast<uint8_t>(character));
                SetRam8(Player(n) + kCharacterAgain, static_cast<uint8_t>(character));
            }
        }
        for (int n = 0; n < 2; n++) {
            if (wanted.colours[n] >= 0) {
                const bool hidden = wanted.characters[n] >= 0 && HiddenForm(n, wanted.characters[n]) != wanted.characters[n];
                SetRam8(Player(n) + kColours, static_cast<uint8_t>(hidden ? wanted.colours[n] & 1 : wanted.colours[n]));
            }
        }
        if (wanted.characters[0] >= 0 && Ram8(Player(0) + kCharacter) == Ram8(Player(1) + kCharacter) &&
            Ram8(Player(0) + kColours) == Ram8(Player(1) + kColours)) {
            SetRam8(Player(1) + kColours, Ram8(Player(1) + kColours) ^ 1);
        }
        // (0: the game's choice)
        const int stage = wanted.stage ? wanted.stage : FeatureOn("training_mode") ? FeatureValue("training_stage") : 0;
        if (stage < 1 || stage > kStages || Ram8(kPlayersThen) != 3) {
            return false;
        }
        SetRam16(kStage, static_cast<uint16_t>((stage - 1) * 2));
        Return(c);
        return true;
    }

    bool Choosing() {
        return Ram16(kMode) == 2 && Ram16(kGameStep) == kGameChoosing && Ram8(kPlayersIn) == 3;
    }

    void Dummy(Machine &machine);

    bool g_choosingTwo = false; // player 1's controls are choosing player 2's character

    void BeforeFrame(Machine &machine) {
        if (g_boot) {
            Boot(machine);
            return;
        }
        if (FeatureOn("training_mode") && Choosing() && Ram8(Player(0) + kChoosingStep) >= kChosen &&
            Ram8(Player(1) + kChoosingStep) < kChosen) {
            // Player 1 has chosen: its controls choose for player 2 (training with one set of controls). What
            // was held as it chose is not passed on until let go, or the button that chose for player 1
            // would choose for player 2 at once.
            static uint16_t heldOver = 0;
            const uint16_t held = machine.players[0] & (kSticks | kButtons);
            heldOver = g_choosingTwo ? heldOver & held : held;
            g_choosingTwo = true;
            machine.players[1] |= held & ~heldOver;
            machine.players[0] &= kStart | kCoin;
            return;
        }
        g_choosingTwo = false;
        Dummy(machine);
    }

    // --- the dummy: before a frame, what the game is told player 2 presses (fight.h) ---

    void Dummy(Machine &machine) {
        fight::DummyView view;
        view.fighting = Fighting();
        view.doing = FramesDoing(1);
        view.onRight = DummyOnRight();
        view.sinceHit = g_combos.Since(1);
        if (view.fighting && FeatureValue("dummy_guard")) {
            view.attackInRange = AttackInRange(machine.cpu, Player(1));
            view.guardStanding = view.attackInRange && GuardStanding(machine.cpu, Player(1));
        }
        g_dummy.BeforeFrame(machine.players[0], machine.players[1], view);
    }

    // --- after a frame ---

    // What the game read off the panel for a player this frame (vsav.h, kControls), as Button bits
    uint16_t PanelHeld(int player) {
        const uint16_t word = Ram16(0xFF8000 + static_cast<uint32_t>(kControls[player]));
        uint16_t held = word & 0x0F; // (the stick's four bits are in the same order)
        held |= static_cast<uint16_t>(((word >> 8) & 0x07) << 4); // punches
        held |= static_cast<uint16_t>(((word >> 12) & 0x07) << 7); // kicks
        return held;
    }

    void Frame(Machine &machine) {
        if (StageViewer() || BattleMode()) {
            // (nothing of training's is shown, or kept)
            g_fighting = false;
            g_roundFought = false;
            return;
        }
        // The round's fighting begins: kept, for starting over
        const bool fought = RoundFought();
        if (fought && !g_roundFought && FeatureOn("training_mode")) {
            g_roundStart.clear();
            Archive ar(g_roundStart, true);
            machine.Serialize(ar);
        }
        g_roundFought = fought;
        const bool readout = FeatureOn("damage_readout"), refill = FeatureOn("health_refill");
        const bool inputs = FeatureOn("input_display");
        const bool fighting = Fighting();
        if (!fighting || !g_fighting) {
            // (a round starting: start afresh)
            Afresh(fighting);
            SetHud({});
            SetBoxes({});
            g_nextBoxes.clear();
            return;
        }
        g_combos.Frame();
        std::vector<HudLine> lines;
        for (int n = 0; n < 2; n++) {
            const uint16_t health = Ram16(Player(n) + kHealth);
            if (refill && g_combos.Since(n) > kRefillAfter && health < kHealthFull) {
                SetRam16(Player(n) + kHealth, kHealthFull);
                SetRam16(Player(n) + kHealthCeiling, kHealthFull);
            }
            if (readout) {
                // The damage a player took is shown on that player's side, under the health bar (how much of it is
                // lost for good, how much is the white part of the bar that comes back, the round's best)
                g_combos.Hud(n, lines, 0.17f, true, {}, true);
            }
            // What the player holds, as the game read it off the panel this frame (input.cpp: the dummy's too),
            // newest on top, with how long
            g_inputs.Push(n, PanelHeld(n), false);
            if (inputs) {
                g_inputs.Hud(n, lines, 0.30f);
            }
        }
        std::vector<HudBox> boxes;
        FramesHud(FeatureOn("frame_data"), FeatureOn("frame_meter"), lines, boxes);
        if (FeatureOn("hit_boxes")) {
            // What the game's memory says now is what the next picture shows, so the boxes are drawn a frame
            // after they are worked out
            // (in a wide picture the stage is further right than on the board's: widescreen.h)
            for (HudBox &box : g_nextBoxes) {
                box.x += WideStage();
            }
            boxes.insert(boxes.begin(), g_nextBoxes.begin(), g_nextBoxes.end());
            g_nextBoxes.clear();
            const int cameraX = static_cast<int16_t>(Ram16(kCameraX)), cameraY = static_cast<int16_t>(Ram16(kCameraY));
            for (const PlacedBox &box : AllBoxes(machine)) {
                const fight::BoxKind kind = box.kind == BoxKind::Attack ? fight::BoxKind::Attack :
                                            box.kind == BoxKind::Push   ? fight::BoxKind::Push :
                                                                          fight::BoxKind::Hurt;
                g_nextBoxes.push_back(fight::ToPicture({kind, box.x, box.y, box.halfWidth, box.halfHeight, box.thrown}, cameraX, cameraY, kPictureFloor));
            }
        } else {
            g_nextBoxes.clear();
        }
        SetBoxes(std::move(boxes));
        g_dummy.Hud(lines, 0.17f);
        SetHud(std::move(lines));
    }
} // namespace

// A hit's damage is being taken off a player's health (from the hook in cheats.cpp)
void TrainingHit(int player, unsigned forGood, unsigned besides) {
    // A combo goes on while the player has not got free since the last hit
    const Doing doing = FramesDoing(player);
    g_combos.Hit(player, forGood + besides, besides, doing == Doing::HitStun || doing == Doing::Held);
}

// A hit has landed on a player, or been guarded (fight.cpp, HitLands)
void TrainingContact(int victim, bool guarded, uint8_t kind) {
    (void)kind;
    FramesContact(victim, guarded);
}

void TrainingBeforeFrame(Machine &machine) {
    BeforeFrame(machine);
}

void TrainingFrame(Machine &machine) {
    Frame(machine);
}

void TrainingState(Archive &ar) {
    ar.Mark(0x5452414E);
    ar.Pod(g_fighting);
    ar.Pod(g_combos);
    ar.Pod(g_inputs);
    g_dummy.Serialize(ar);
    ar.Vec(g_roundStart);
    ar.Pod(g_roundFought);
    FramesState(ar);
}

// A training session is on, or the stage viewer: no clock and no knockouts (fight.cpp)
bool TrainingMode() {
    return FeatureOn("training_mode") || StageViewer() || BattleNoKnockOut();
}

// "Training mode" on the menu's Alternative modes page (modes.cpp)
void StartTraining(Machine &machine) {
    Start(machine);
}

// The board switched off and on, and the way to the character choice with two players worked (the game runs
// unseen meanwhile)
void BootToChoice(Machine &machine) {
    machine.Reset();
    g_boot = true;
    g_bootFrames = 0;
    SetFastForward(true);
}
bool Booting() {
    return g_boot;
}

void RegisterTraining() {
    Game &game = TheGame();
    AddHook(0x00AE7C, "ChooseStage", ChooseStage);
    game.commands.push_back({"training_restart", "Training mode", "Start over",
                             "In a training session: the round as it began, with the players where \"Start over at\" says. Backspace "
                             "or a gamepad's left shoulder does this while playing; the stick held as it is pressed chooses the place "
                             "(left, right: a corner; down: sides swapped; up: the middle).",
                             StartOver, kKeyRestart});
    game.features.push_back({"restart_at", "Training mode", "Start over at",
                             "Where \"Start over\" puts the players: the middle or a corner, player 1 on the left or (swapped) on the right.",
                             0, {"Middle", "Left corner", "Right corner", "Middle, swapped", "Left corner, swapped", "Right corner, swapped"}});
    game.features.push_back({"training_stage", "Training mode", "Stage",
                             "Where a training session's fight is: the game's own choice (at random, from those that go with player "
                             "2's character) or a stage named here. Takes effect when the characters are next chosen.",
                             0, {"The game's choice", "Feast of the Damned", "Concrete Cave", "Tower of Arrogance", "Red Thirst", "Deserted Chateau",
                                 "Abaraya", "Vanity Paradise", "War Agony", "Forever Torment", "Green Scream", "Iron Horse, Iron Terror",
                                 "Fetus of God"}});
    game.features.push_back({"training_mode", "Training mode", "Training session",
                             "The fight has no clock and nobody is knocked out. (\"Start\" above switches this "
                             "on and takes the game to the character choice; Reset ends it.)",
                             kFeatureSession});
    game.features.push_back({"damage_readout", "Training mode", "Combo readout",
                             "Under the bar of the player hit: what the hit took off; for a combo (hits landing before the player is free again) the "
                             "hits and the damage; how much of it is lost for good and how much comes back; the round's best."});
    game.features.push_back({"input_display", "Training mode", "Input display",
                             "Shows what each player presses and for how many frames: the stick as a keypad number (5 is the middle, 6 right, 2 down), then the buttons."});
    game.features.push_back({"hit_boxes", "Training mode", "Hit boxes",
                             "Draws the boxes the game tests: where a player can be hit (blue), attacks (red; orange for things thrown), and the boxes that keep the players apart (green)."});
    game.features.push_back({"frame_data", "Training mode", "Frame data",
                             "After a move: the step it can first hit on (startup), how many it can hit for (active) and how many "
                             "follow before the player can act (recovery). After a hit or a guarded one: who can act first, by how "
                             "many steps (+: the attacker)."});
    game.features.push_back({"frame_log", "Training mode", "Frame data log",
                             "Writes every move's frame data to frame_data.csv, beside the settings file: the character, what was "
                             "held as it began, startup, active, recovery and the advantage after a hit or a guarded one."});
    game.features.push_back({"frame_meter", "Training mode", "Frame meter",
                             "A row of cells for each player, one for every step of the game: green before a move can hit, red "
                             "while it can, blue after; yellow reeling from a hit, light blue from a guarded one; grey in the air."});
    game.features.push_back({"health_refill", "Training mode", "Health refills",
                             "A player's health comes back when they have not been hit for a moment."});
    fight::DummyOptions dummy;
    // While guarding: buttons pressed one after another, guarding still (pushing the attacker away); or a guard
    // cancel: towards, down, down and towards with a punch or a kick, again and again
    dummy.guardActions.push_back({"Pushes away", [](int frame, uint16_t, uint16_t) -> uint16_t {
                                      static constexpr uint16_t kMash[] = {kPunch1, 0, kPunch2, 0, kPunch3, 0, kKick1, 0, kKick2, 0, kKick3, 0};
                                      return kMash[frame % 12];
                                  },
                                  true});
    dummy.guardActions.push_back({"Guard cancel, punch", [](int frame, uint16_t towards, uint16_t) -> uint16_t {
                                      const int at = (frame / 2) % 4;
                                      return at == 0 ? towards : at == 1 ? kDown : at == 2 ? static_cast<uint16_t>(kDown | towards | kPunch2) : 0;
                                  }});
    dummy.guardActions.push_back({"Guard cancel, kick", [](int frame, uint16_t towards, uint16_t) -> uint16_t {
                                      const int at = (frame / 2) % 4;
                                      return at == 0 ? towards : at == 1 ? kDown : at == 2 ? static_cast<uint16_t>(kDown | towards | kKick2) : 0;
                                  }});
    dummy.guardActionsWords = "What player 2 does while guarding a hit: presses buttons to push the attacker away, or does a guard "
                              "cancel (towards, down, down and towards with a punch or a kick: most characters have one or the other).";
    // Thrown: the throw's own command softens it ("tech hit")
    dummy.heldAction = [](int frame, uint16_t, uint16_t away) -> uint16_t {
        return static_cast<uint16_t>(away | ((frame & 1) ? (kPunch2 | kPunch3) : 0));
    };
    dummy.heldActionTitle = "Softens throws";
    dummy.heldActionWords = "Player 2 does a throw's own command while being thrown (a \"tech hit\").";
    // Knocked down (reeling for more than 34 frames): the stick held with a button tapped rolls it along the floor
    dummy.getUp.push_back({"Towards", [](int frame, uint16_t towards, uint16_t) -> uint16_t {
                               return static_cast<uint16_t>(towards | ((frame & 2) ? kPunch1 : 0));
                           }});
    dummy.getUp.push_back({"Away", [](int frame, uint16_t, uint16_t away) -> uint16_t {
                               return static_cast<uint16_t>(away | ((frame & 2) ? kPunch1 : 0));
                           }});
    dummy.getUpWords = "Where player 2 gets up after being knocked down: where it fell, or rolled towards player 1 or away.";
    dummy.downAfter = 34;
    fight::RegisterDummy(dummy);
}

} // namespace cps2recomp
