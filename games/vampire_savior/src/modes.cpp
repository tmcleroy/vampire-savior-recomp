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
// them; the objects of the players and of the panel are left out of the picture (video.h, hideObject). The
// stage's pieces move and its music plays as in a fight.
//
// The viewer's "infinite stage" is every stage in turn, each scrolled from end to end and then out of the
// picture as the next comes in. For the two to be seen and heard together there are two machines: the one
// the port runs, and another kept as a save state, which this file loads into the port's machine, runs a
// frame of and puts away again, every frame. The picture is then made of the two, and the sound mixed.
#include "modes.h"
#include "state.h"
#include "widescreen.h"

#include <algorithm>
#include <cstring>
#include <iterator>
#include <vector>

namespace cps2recomp {

void StartTraining(Machine &machine); // training.cpp

namespace {
    using namespace vsav;

    // The menu's pages
    constexpr const char *kModes = "Alternative modes", *kAttract = "Attract mode", *kViewer = "Stage viewer";
    constexpr const char *kTraining = "Training mode", *kDummy = "Dummy"; // (training.cpp's categories)

    enum class Mode : int32_t { None, Attract, Viewer };
    struct State {
        Mode mode = Mode::None;
        bool starting = false;  // on the way there: the board is starting up (the game runs unseen)
        int32_t frames = 0;     //   and for how many frames
        int32_t stage = 0;      // the viewer: the stage shown (1-12)
        int32_t camera = 0;     //   where the picture's left edge is wanted, in 256ths of a pixel
        int32_t direction = 1;  //   which way it scrolls by itself
        int32_t music = 0;      //   the "Music" choice that is playing
        // The infinite stage: the stage that comes next is kept running beside the one shown
        bool infinite = false;
        int32_t next = 0;        // the stage that comes next (1-12)
        bool nextReady = false;  //   its round is being fought (until then it is on its way there)
        int32_t nextFrames = 0;  //   frames it has run on the way
        int32_t slide = 0;       // how far the next stage has come into the picture, in 256ths of a pixel
    } g;
    std::vector<uint8_t> g_other;      // the machine of the stage that comes next, as a save state
    std::vector<uint8_t> g_mine;       // (the shown stage's, while the other's runs)
    std::vector<int16_t> g_otherSound; // sound the next stage has made and that has not been heard yet
    std::vector<int16_t> g_carry;      // sound to come before the next frame's
    size_t g_soundBefore = 0;          // how much sound the machine held before the frame
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
        if (g.mode == Mode::None) {
            return;
        }
        g = State();
        g_chosenStage = -1;
        Wanted() = MatchWanted();
        g_other.clear();
        g_otherSound.clear();
        g_carry.clear();
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
        EndBattle();
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

    constexpr uint16_t kApart = 0xC0;                  // the players stand this far apart (the camera then keeps their middle in the picture's)
    constexpr int32_t kStick = 384;                    // a frame's scrolling with the stick held, in 256ths of a pixel
    constexpr int32_t kScroll[] = {0, 64, 128, 256};   //   and by itself: slow, medium, fast
    // The camera's ends, in 256ths of a pixel, for the picture being played in (widescreen.h: a wide
    // picture's camera stops 64 short of where the board's does)
    int32_t LeftEnd() {
        return (kCameraLeft + WideMargin()) * 256;
    }
    int32_t RightEnd() {
        return (kCameraRight - WideMargin()) * 256;
    }
    constexpr int kInfinite = kStages;                 // the "Stage" choice after the stages
    int32_t Across() { // the picture's width: how far a stage slides to be gone
        return (Video::kWidth + 2 * WideSide()) * 256;
    }
    constexpr int kOnTheWay = 3;                       // frames a frame that the next stage runs until its round is on
    constexpr size_t kSoundAhead = 16;                 // samples of the next stage's sound kept in hand

    constexpr uint16_t kOtherTunes[] = {0x02}; // the tunes offered besides the stages'

    // The players put where the camera then goes to `camera` (it keeps the middle of the two in the middle
    // of the picture)
    void Place(int32_t camera) {
        const uint32_t middle = static_cast<uint32_t>(camera / 256) + kApart;
        SetRam32(Player(0) + kX, (middle - kApart / 2) << 16);
        SetRam32(Player(1) + kX, (middle + kApart / 2) << 16);
    }

    void StartViewer(Machine &machine) {
        End();
        EndBattle();
        SetFeature("training_mode", false);
        g.mode = Mode::Viewer;
        g.starting = true;
        g_chosenStage = FeatureValue("viewer_stage");
        g.infinite = g_chosenStage == kInfinite;
        g.stage = g.infinite ? 1 : std::clamp(g_chosenStage, 0, kStages - 1) + 1;
        g.next = g.stage % kStages + 1;
        g.camera = g.infinite ? LeftEnd() : (LeftEnd() + RightEnd()) / 2;
        Wanted().stage = g.stage;
        machine.Reset();
    }

    void ViewerBeforeFrame(Machine &machine) {
        g_soundBefore = machine.audio.size();
        Wanted().stage = g.stage;
        if (g.starting) {
            Fast(true);
            if (ToRound(machine, g.frames)) {
                g.starting = false;
                Fast(false);
            } else if (g.frames > kStartMost) {
                End();
            }
            if (g.starting || g.mode != Mode::Viewer) {
                return;
            }
        }
        const uint16_t stick = (machine.players[0] | machine.players[1]) & (kLeft | kRight);
        machine.players[0] = machine.players[1] = 0;
        const int scroll = std::clamp(FeatureValue("viewer_scroll"), 0, static_cast<int>(std::size(kScroll)) - 1);
        const int32_t byStick = stick == kRight ? kStick : stick == kLeft ? -kStick : 0;
        if (g.infinite) {
            // Always onwards by itself; with the stick, back as well, as far as this stage's start. The stage
            // is scrolled to its end, and then slides out as the next comes in (if that one is ready).
            int32_t by = scroll == 0 ? byStick : kScroll[scroll];
            if (g.slide > 0 || (g.camera + by >= RightEnd() && by > 0)) {
                by -= RightEnd() - g.camera;
                g.camera = RightEnd();
                g.slide = g.nextReady ? std::clamp(g.slide + by, 0, Across()) : 0;
            } else {
                g.camera = std::max(g.camera + by, LeftEnd());
            }
        } else {
            g.camera += scroll == 0 ? byStick : g.direction * kScroll[scroll];
            if (g.camera >= RightEnd()) {
                g.camera = RightEnd();
                g.direction = -1;
            } else if (g.camera <= LeftEnd()) {
                g.camera = LeftEnd();
                g.direction = 1;
            }
        }
        Place(g.camera);
    }

    // The infinite stage, after the shown stage's frame: a frame of the next stage's machine (or a few, while
    // it is on its way to its round), and the picture and the sound made of the two.
    void RunNext(Machine &machine) {
        const int width = machine.PictureWidth();
        const size_t pixels = static_cast<size_t>(width) * Video::kHeight;
        const Video::Wide wide = machine.video.wide;
        static std::vector<uint32_t> shown;
        shown.assign(machine.Picture(), machine.Picture() + pixels);
        g_soundBefore = std::min(g_soundBefore, machine.audio.size());
        std::vector<int16_t> sound(machine.audio.begin() + static_cast<std::ptrdiff_t>(g_soundBefore), machine.audio.end());
        machine.audio.resize(g_soundBefore);
        const uint16_t held[2] = {machine.players[0], machine.players[1]};

        // The shown stage's machine put away, the next stage's taken out (or switched on)
        g_mine.clear();
        {
            Archive ar(g_mine, true);
            machine.Serialize(ar);
        }
        if (g_other.empty()) {
            machine.Reset();
            g.nextReady = false;
            g.nextFrames = 0;
            g_otherSound.assign(kSoundAhead * 2, 0);
        } else {
            Archive ar(g_other, false);
            machine.Serialize(ar);
        }
        Wanted().stage = g.next;
        machine.video.hideObject = NotTheStage;
        WideForSpare(machine); // (its picture as wide as the shown stage's)
        for (int n = 0; n < kOnTheWay; n++) {
            machine.audio.resize(g_soundBefore);
            g.nextReady = g.nextReady || ToRound(machine, g.nextFrames);
            if (g.nextReady) {
                machine.players[0] = machine.players[1] = 0;
                Place(LeftEnd());
            }
            machine.RunFrame();
            if (g.nextReady || g.nextFrames > kStartMost) {
                break;
            }
        }
        if (g.nextReady) {
            g_otherSound.insert(g_otherSound.end(), machine.audio.begin() + static_cast<std::ptrdiff_t>(g_soundBefore), machine.audio.end());
        }
        machine.audio.resize(g_soundBefore);

        // The picture: the shown stage moved left by the slide, the next stage after it
        const int slide = std::clamp(g.slide / 256, 0, width);
        if (slide > 0 && machine.PictureWidth() == width) {
            const uint32_t *next = machine.Picture();
            for (int y = 0; y < Video::kHeight; y++) {
                uint32_t *row = &shown[static_cast<size_t>(y) * width];
                memmove(row, row + slide, static_cast<size_t>(width - slide) * sizeof(uint32_t));
                memcpy(row + width - slide, next + static_cast<size_t>(y) * width, static_cast<size_t>(slide) * sizeof(uint32_t));
            }
        }
        // The sound: one fades into the other as the next stage comes in. As many samples as the shown stage
        // made are taken of the next stage's (the two machines' frames do not make quite the same number).
        const size_t count = sound.size();
        if (g_otherSound.size() < count) {
            g_otherSound.resize(count, 0);
        }
        const int32_t nextGain = slide * 256 / width, shownGain = 256 - nextGain;
        for (size_t i = 0; i < count; i++) {
            const int32_t mixed = (sound[i] * shownGain + (g.nextReady ? g_otherSound[i] : 0) * nextGain) / 256;
            sound[i] = static_cast<int16_t>(std::clamp(mixed, -32768, 32767));
        }
        g_otherSound.erase(g_otherSound.begin(), g_otherSound.begin() + static_cast<std::ptrdiff_t>(count));

        if (g.slide >= Across()) {
            // The next stage has come all the way in: its machine is the one the port runs from here, and
            // the stage after it is switched on
            g.stage = g.next;
            g.next = g.stage % kStages + 1;
            g.camera = LeftEnd();
            g.slide = 0;
            g.nextReady = false;
            g.nextFrames = 0;
            g_other.clear();
            g_carry = g_otherSound; // (what it has made and was not yet heard comes first)
            g_otherSound.clear();
        } else {
            g_other.clear();
            {
                Archive ar(g_other, true);
                machine.Serialize(ar);
            }
            Archive ar(g_mine, false);
            machine.Serialize(ar);
        }
        Wanted().stage = g.stage;
        machine.players[0] = held[0];
        machine.players[1] = held[1];
        machine.video.wide = wide;
        WideForPort(machine);
        memcpy(machine.Picture(), shown.data(), pixels * sizeof(uint32_t));
        machine.audio.insert(machine.audio.end(), sound.begin(), sound.end());
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
        if (g.infinite) {
            if (!g_carry.empty()) {
                machine.audio.insert(machine.audio.begin() + static_cast<std::ptrdiff_t>(std::min(g_soundBefore, machine.audio.size())),
                                     g_carry.begin(), g_carry.end());
                g_carry.clear();
            }
            RunNext(machine); // (each stage with its own tune)
        } else {
            const int music = FeatureValue("viewer_music"); // 0: the stage's own; then the stages' tunes; then the others
            if (music != g.music) {
                g.music = music;
                const int stage = music == 0 ? g.stage : music;
                PlaySound(kTuneOff);
                PlaySound(stage <= kStages ? static_cast<uint16_t>(kStageTune + stage - 1) :
                         kOtherTunes[std::min(stage - kStages - 1, static_cast<int>(std::size(kOtherTunes)) - 1)]);
            }
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
// The stage the machine that is running is to fight on (the game's choice of stage: training.cpp)
int StageViewerStage() {
    return g.mode == Mode::Viewer ? Wanted().stage : 0;
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
    ar.Vec(g_other);
    if (!ar.Saving() && !RunningAhead()) { // (not when run-ahead puts the frame back: runtime/game.h)
        Fast(false);
        g_otherSound.assign(kSoundAhead * 2, 0);
        g_carry.clear();
        g_chosenStage = -1;
        Wanted().stage = g.stage;
    }
}

void RegisterModes() {
    Game &game = TheGame();
    // The page at the head of the menu, and a page under it for each mode (and the dummy's under training's)
    game.pages.push_back({kModes, "", "Other ways to run the game: training, attract mode, viewers for the stages and the characters, survival, endless battle, ring out.", true});
    game.pages.push_back({kTraining, kModes, "A fight with no clock that nobody loses, and the aids for practising in it."});
    game.pages.push_back({kDummy, kTraining, "What player 2 does in a training session."});
    game.pages.push_back({kAttract, kModes,
                          "The game's own show on a loop, with no coin in: the introduction, the title, fights and rankings."});
    game.pages.push_back({kViewer, kModes, "A stage with nobody on it, to look at and listen to."});
    game.commands.push_back({"training_start", kTraining, "Start",
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
    game.features.push_back({"viewer_stage", kViewer, "Stage",
                             "The stage the viewer shows, or Infinite: every stage in turn, each scrolling into the next with its "
                             "own tune. Changing it while viewing goes to the new one.",
                             0, {"Feast of the Damned", "Concrete Cave", "Tower of Arrogance", "Red Thirst", "Deserted Chateau", "Abaraya",
                                 "Vanity Paradise", "War Agony", "Forever Torment", "Green Scream", "Iron Horse, Iron Terror", "Fetus of God",
                                 "Infinite"}});
    game.features.push_back({"viewer_scroll", kViewer, "Scrolling",
                             "How the viewer moves across the stage: with the stick, or by itself from one end to the other and back "
                             "(Infinite: always onwards).",
                             0, {"With the stick", "Slow", "Medium", "Fast"}});
    game.features.push_back({"viewer_music", kViewer, "Music",
                             "The tune that plays in the viewer: the stage's own, or another's. (Infinite plays each stage's own.)", 0,
                             {"The stage's own", "Feast of the Damned", "Concrete Cave", "Tower of Arrogance", "Red Thirst", "Deserted Chateau",
                              "Abaraya", "Vanity Paradise", "War Agony", "Forever Torment", "Green Scream", "Iron Horse, Iron Terror",
                              "Fetus of God", "Character select"}});
}

} // namespace cps2recomp
