// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy
//
// Vampire Savior: the "Alternative modes" page at the head of the in-game menu. Ways of running the game that
// the board does not have: training (training.cpp), attract mode on a loop, and a viewer for the stages (the
// last two each with a page under it for their choices). Each is started by a command and ends when the
// board is reset, when another is started, or (attract mode) with Start.
//
// Attract mode is the game's own loop of notices, introduction, title, two fights and the rankings, with no
// coin in. "The intro only" and "the fights only" leave parts of the loop out by moving the game's own step
// on (vsav.h, kMode) where a part that is wanted ends.
//
// The stage viewer is a round of a two-player game on the stage chosen, in which nobody is drawn but the
// stage: the players stand a fixed distance apart, are moved together, and the game's own camera follows
// them; the objects of the players and of the panel are left out of the picture by their colour sets
// (video.h, hiddenObjectColours). The stage's pieces move and its music plays as in a fight.
#include "machine.h"
#include "state.h"
#include "vsav.h"

#include <algorithm>
#include <iterator>

namespace cps2recomp {

void StartTraining(Machine &machine); // training.cpp
void BootToChoice(Machine &machine);
bool Booting();

namespace {
    using namespace vsav;

    // The menu's pages
    constexpr const char *kModes = "Alternative modes", *kAttract = "Attract mode", *kViewer = "Stage viewer";

    enum class Mode : int32_t { None, Attract, Viewer };
    struct State {
        Mode mode = Mode::None;
        bool starting = false;  // on the way there: the board is starting up (the game runs unseen)
        int32_t frames = 0;     //   and for how many frames
        int32_t stage = 0;      // the viewer: the stage shown (1-12)
        int32_t camera = 0;     //   where the picture's left edge is wanted, in 256ths of a pixel
        int32_t direction = 1;  //   which way it scrolls by itself
        int32_t music = 0;      //   the "Music" choice that is playing
    } g;
    bool g_fast = false; // this file asked for the game to run unseen
    int g_chosenStage = -1; // the menu's stage for the viewer when last looked at

    constexpr int kStartMost = 9000; // frames after which getting to a stage is given up

    void Fast(bool on) {
        if (on != g_fast) {
            g_fast = on;
            SetFastForward(on);
        }
    }

    void End() {
        g = State();
        g_chosenStage = -1;
        Fast(false);
    }

    // --- attract mode ---

    // The parts of the game's loop, by kMode (each step is two; the odd steps between are fades)
    constexpr uint16_t kLoop = 0x0C;        // where the loop starts
    constexpr uint16_t kNotices = 0x10;     // the sound notice, then the maker's
    constexpr uint16_t kIntro = 0x16;       // the introduction, ending on the title
    constexpr uint16_t kFirstFight = 0x1A;  // a fight
    constexpr uint16_t kScores = 0x22;      // the score ranking, the maker's notice, the title
    constexpr uint16_t kSecondFight = 0x38; // another fight
    constexpr uint16_t kVersus = 0x40;      // the versus ranking; then the loop starts again from kNotices
    enum { kShowAll, kShowIntro, kShowFights };

    void StartAttract(Machine &machine) {
        End();
        SetFeature("training_mode", false);
        g.mode = Mode::Attract;
        g.starting = true;
        machine.Reset();
    }

    // The game's step moved to another part of the loop, as the game does it: the step, with the steps under
    // it and its timer cleared
    void GoTo(uint16_t part) {
        SetRam16(kMode, part);
        for (uint32_t at = kMode + 2; at < kMode + 0x10; at += 2) {
            SetRam16(at, 0);
        }
    }

    void AttractFrame() {
        const uint16_t part = Ram16(kMode);
        const int show = FeatureValue("attract_show");
        if (show == kShowAll) {
            Fast(false);
        } else if (part < kNotices) {
            Fast(true); // (the loop's own start)
        } else if (show == kShowIntro) {
            if (part < kIntro || part >= kFirstFight) {
                GoTo(kIntro);
            }
            Fast(false);
        } else {
            // (the game counts its fights on as it comes to a ranking, which is left out here: the count picks
            // who fights next, and where)
            if (part < kFirstFight) {
                GoTo(kFirstFight);
            } else if (part >= kVersus) {
                SetRam8(kAttractFight, static_cast<uint8_t>(Ram8(kAttractFight) + 2));
                GoTo(kFirstFight);
            } else if (part >= kScores && part < kSecondFight) {
                SetRam8(kAttractFight, static_cast<uint8_t>(Ram8(kAttractFight) + 2));
                GoTo(kSecondFight);
            }
            Fast(false);
        }
    }

    // --- the stage viewer ---

    // The objects that are not the stage's: the players, what they throw, sparks and the panel use colour
    // sets 0-21 and a stage's own pieces 22-31; the players' shadows are in set 22 with some stages' pieces,
    // and are told by their tiles
    bool NotTheStage(uint32_t tile, uint16_t attributes) {
        const unsigned colours = attributes & 0x1F;
        return colours < 22 || (colours == 22 && tile >= 0xF8E0 && tile < 0xF920);
    }
    constexpr uint16_t kApart = 0xC0;                  // the players stand this far apart (the camera then keeps their middle in the picture's)
    constexpr int32_t kStick = 384;                    // a frame's scrolling with the stick held, in 256ths of a pixel
    constexpr int32_t kScroll[] = {0, 64, 128, 256};   //   and by itself: slow, medium, fast

    // The tune of each stage, in the order of the stages' numbers, and the others offered
    constexpr uint16_t kStageTune = 0x09; // (a stage's own tune: this and its number)
    constexpr uint16_t kOtherTunes[] = {0x02};
    constexpr uint16_t kTuneOff = 0xFF05; // the game sends this before a new tune

    void PlayTune(uint16_t code) {
        const uint16_t at = Ram16(kSoundQueueEnd) & 0xFF0;
        SetRam32(kSoundQueue + at, code);
        SetRam32(kSoundQueue + at + 4, 0);
        SetRam32(kSoundQueue + at + 8, 0);
        SetRam16(kSoundQueueEnd, static_cast<uint16_t>((at + 0x10) & 0xFF0));
    }

    bool RoundFought() {
        return Ram16(kMode) == 2 && Ram16(kGameStep) == kGameMatch && Ram16(kMatchStep) == kMatchRound && Ram16(kRoundStep) == kRoundFought;
    }

    void StartViewer(Machine &machine) {
        End();
        SetFeature("training_mode", false);
        g.mode = Mode::Viewer;
        g.starting = true;
        g_chosenStage = FeatureValue("viewer_stage");
        g.stage = std::clamp(g_chosenStage, 0, kStages - 1) + 1;
        g.camera = (kCameraLeft + kCameraRight) / 2 * 256;
        BootToChoice(machine);
    }

    void ViewerBeforeFrame(Machine &machine) {
        uint16_t &one = machine.players[0], &two = machine.players[1];
        if (g.starting) {
            if (Booting()) {
                return; // (training.cpp works the controls as far as the character choice)
            }
            // The characters and their choices taken as they come, and on through the match's start
            Fast(true);
            one = two = (g.frames++ % 16 < 6) ? kPunch1 : 0;
            if (RoundFought()) {
                g.starting = false;
                Fast(false);
            } else if (g.frames > kStartMost) {
                End();
            }
            return;
        }
        const uint16_t stick = (one | two) & (kLeft | kRight);
        one = two = 0;
        const int scroll = std::clamp(FeatureValue("viewer_scroll"), 0, static_cast<int>(std::size(kScroll)) - 1);
        if (scroll == 0) {
            g.camera += stick == kRight ? kStick : stick == kLeft ? -kStick : 0;
        } else {
            g.camera += g.direction * kScroll[scroll];
        }
        if (g.camera >= kCameraRight * 256) {
            g.camera = kCameraRight * 256;
            g.direction = -1;
        } else if (g.camera <= kCameraLeft * 256) {
            g.camera = kCameraLeft * 256;
            g.direction = 1;
        }
        // The players where the camera then goes (it keeps the middle of the two in the middle of the picture)
        const uint32_t middle = static_cast<uint32_t>(g.camera / 256) + kApart;
        SetRam32(Player(0) + kX, (middle - kApart / 2) << 16);
        SetRam32(Player(1) + kX, (middle + kApart / 2) << 16);
    }

    void ViewerFrame(Machine &machine) {
        if (g.starting) {
            return;
        }
        // Another stage chosen in the menu: the viewer goes there. (Not when what is shown is not what the
        // menu says for another reason: a save state of the viewer loaded.)
        const int chosen = FeatureValue("viewer_stage");
        if (g_chosenStage >= 0 && chosen != g_chosenStage) {
            StartViewer(machine);
            return;
        }
        g_chosenStage = chosen;
        const int music = FeatureValue("viewer_music"); // 0: the stage's own; then the stages' tunes; then the others
        if (music != g.music) {
            g.music = music;
            const int stage = music == 0 ? g.stage : music;
            PlayTune(kTuneOff);
            PlayTune(stage <= kStages ? static_cast<uint16_t>(kStageTune + stage - 1) :
                     kOtherTunes[std::min(stage - kStages - 1, static_cast<int>(std::size(kOtherTunes)) - 1)]);
        }
        SetHud({});
        SetBoxes({});
    }

    void TrainingCommand(Machine &machine) {
        StartTraining(machine);
    }
} // namespace

bool AttractMode() {
    return g.mode == Mode::Attract;
}
bool StageViewer() {
    return g.mode == Mode::Viewer;
}
int StageViewerStage() {
    return g.mode == Mode::Viewer ? g.stage : 0;
}
void EndModes() {
    End();
}

void ModesBeforeFrame(Machine &machine) {
    if (g.mode == Mode::Attract) {
        // Start or a coin ends it, and is passed on; nothing else reaches the game
        if ((machine.players[0] | machine.players[1]) & (kStart | kCoin)) {
            End();
        } else {
            machine.players[0] = machine.players[1] = 0;
        }
    } else if (g.mode == Mode::Viewer) {
        ViewerBeforeFrame(machine);
    }
}

void ModesFrame(Machine &machine) {
    // The board was reset (the menu's Reset, or another mode starting), or a game begun: the mode is over.
    // The game's step is below the loop's while the board starts up and in a game, and 0 at switching on.
    if (g.mode == Mode::Attract) {
        const bool loop = Ram16(kMode) >= kLoop;
        if (g.starting) {
            g.starting = !loop;
            Fast(g.starting && FeatureValue("attract_show") != kShowAll); // (the board starting up, unseen when only a part is wanted)
        } else if (!loop) {
            End();
        }
    } else if (g.mode == Mode::Viewer && !g.starting && Ram16(kMode) == 0) {
        End();
    }
    if (g.mode == Mode::Attract && !g.starting) {
        AttractFrame();
    } else if (g.mode == Mode::Viewer) {
        ViewerFrame(machine);
    }
    machine.video.hideObject = g.mode == Mode::Viewer && !g.starting ? NotTheStage : nullptr;
}

void ModesState(Archive &ar) {
    ar.Mark(0x4D4F4445);
    ar.Pod(g);
    if (!ar.Saving()) {
        Fast(false);
    }
}

void RegisterModes() {
    Game &game = TheGame();
    // The page at the head of the menu, and a page under it for each mode that has choices
    game.pages.push_back({kModes, "", "Other ways to run the game: training, attract mode on a loop, a viewer for the stages.", true});
    game.pages.push_back({kAttract, kModes,
                          "The game's own show on a loop, with no coin in: the introduction, the title, fights and rankings."});
    game.pages.push_back({kViewer, kModes, "A stage with nobody on it, to look at and listen to."});
    game.commands.push_back({"training_start", kModes, "Training mode",
                             "Straight to the character choice with two players (player 1's controls choose both), then a fight "
                             "with no clock that nobody loses.",
                             TrainingCommand});
    game.commands.push_back({"attract_start", kAttract, "Start",
                             "Starts attract mode: the game's own show, over and over. Start (or a coin) ends it.", StartAttract});
    game.features.push_back({"attract_show", kAttract, "Shows",
                             "What attract mode plays: everything the board does, the introduction over and over, or one fight "
                             "after another.",
                             0, {"Everything", "The intro only", "The fights only"}});
    game.commands.push_back({"viewer_start", kViewer, "Start",
                             "Shows the stage chosen below with nobody on it. Left and right scroll it, or it scrolls by itself. "
                             "Reset ends it.",
                             StartViewer});
    game.features.push_back({"viewer_stage", kViewer, "Stage", "The stage the viewer shows. Changing it while viewing goes to the new one.", 0,
                             {"Feast of the Damned", "Concrete Cave", "Tower of Arrogance", "Red Thirst", "Deserted Chateau", "Abaraya",
                              "Vanity Paradise", "War Agony", "Forever Torment", "Green Scream", "Iron Horse, Iron Terror", "Fetus of God"}});
    game.features.push_back({"viewer_scroll", kViewer, "Scrolling",
                             "How the viewer moves across the stage: with the stick, or by itself from one end to the other and back.",
                             0, {"With the stick", "Slow", "Medium", "Fast"}});
    game.features.push_back({"viewer_music", kViewer, "Music", "The tune that plays in the viewer: the stage's own, or another's.", 0,
                             {"The stage's own", "Feast of the Damned", "Concrete Cave", "Tower of Arrogance", "Red Thirst", "Deserted Chateau",
                              "Abaraya", "Vanity Paradise", "War Agony", "Forever Torment", "Green Scream", "Iron Horse, Iron Terror",
                              "Fetus of God", "Character select"}});
}

} // namespace cps2recomp
