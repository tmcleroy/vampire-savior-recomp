// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy
//
// Vampire Savior: the fighting modes of the menu's "Alternative modes" page. Each is a two-player match that
// this file sets up (who fights, where, and which of the two the game itself plays), watches, and follows
// with another: the next match is made ready on a spare machine (modes.h) while the last one's ending plays,
// and then takes its place.
//
//   Survival        the player against one opponent after another, with the health left from the fight
//                   before; over at the first knock-down, with the count of opponents beaten.
//   Endless battle  two fighters the game plays, with no panel; the one knocked down is replaced. A person
//                   takes a fighter over by touching the controls, and gives it back by leaving them alone.
#include "modes.h"

#include "state.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <random>
#include <string>

namespace cps2recomp {

namespace {
    using namespace vsav;

    constexpr const char *kModes = "Alternative modes", *kSurvival = "Survival", *kEndless = "Endless battle";

    // The characters by their numbers (11 is nobody), as this set of the game names them
    constexpr const char *kNames[kCharacters] = {"Bulleta", "Demitri", "Gallon", "Victor", "Zabel", "Morrigan", "Anakaris", "Felicia",
                                                 "Bishamon", "Aulbath", "Sasquatch", nullptr, "Q-Bee", "Lei-Lei", "Lilith", "Jedah"};
    constexpr int kNobody = 11;

    constexpr uint32_t kResult = 0xFF810C;  // the round's result (vsav.h, kRoundResult)
    constexpr int kStartMost = 9000;        // frames after which getting to a round is given up
    constexpr int kAfter = 230;             // frames a decided round is left to play out before the next match
    constexpr int kFade = 24;               //   the last of which, and the next match's first, go to and from black
    constexpr int kIdle = 600;              // frames without a press after which the game takes a fighter back
    constexpr int kOnTheWay = 4;            // frames a frame that the next match's machine runs until its round is on
    constexpr int kOverFor = 480;           // frames the count is shown for when survival is over
    constexpr uint16_t kHealthBack = 0x30;  // health given back for an opponent beaten
    // The endless battle's stage: one stage; or the fighters drawn over a stage that another machine shows
    // (modes.h), which can then change under them: by fading into another, or by scrolling on into the next
    enum { kStageOne, kStageShuffle, kStageScroll };
    constexpr int kShuffleAfter = 2 * 60 * 60;  // frames of fighting on a stage before it fades into another
    constexpr int kBlend = 150;                 //   and frames the fade takes
    constexpr int32_t kScrollBy = 64;           // the scrolling stage's speed, in 256ths of a pixel a frame
    constexpr int32_t kLeftEnd = kCameraLeft * 256, kRightEnd = kCameraRight * 256;
    constexpr int32_t kAcross = Video::kWidth * 256;
    constexpr int kArena = (kCameraLeft + kCameraRight) / 2; // where the fighters' own camera is kept while the stage scrolls
    constexpr size_t kSoundAhead = 16;          // samples of another machine's sound kept in hand
    constexpr uint16_t kAnyButton = kLeft | kRight | kUp | kDown | kPunch1 | kPunch2 | kPunch3 | kKick1 | kKick2 | kKick3;

    enum class Kind : int32_t { None, Survival, Endless };
    enum class Phase : int32_t { Starting, Fighting, Decided, Over };
    struct State {
        Kind kind = Kind::None;
        Phase phase = Phase::Starting;
        int32_t frames = 0;              // on the way to the first round
        int32_t since = 0;               // frames in this phase
        int32_t stage = 0;               // 1-12; 0: the game's choice
        int32_t characters[2] = {-1, -1};
        bool computer[2] = {false, false};
        int32_t idle[2] = {0, 0};
        int32_t score = 0;
        int32_t winner = 0;
        int32_t health = 0, ceiling = 0; // survival: what the player has left
        int32_t fadeIn = 0;              // frames of coming from black still to go
        uint32_t random = 1;
        // The stage behind the fighters, when another machine shows it
        int32_t way = kStageOne;
        int32_t back = 0, backNext = 0;  // its stage, and the one that comes after (1-12; 0: none yet)
        int32_t backFor = 0;             // frames of fighting on it
        int32_t blend = 0;               // shuffle: how far the fade to the next has got (of kBlend)
        int32_t backCamera = 0;          // scroll: where its picture's left edge is, in 256ths of a pixel
        int32_t slide = 0;               //   and how far the next stage has come in
    } g;
    Spare g_next;
    Spare g_back, g_backNext;            // the machines of the stage behind and of the one after
    // Sound another machine has made and that has not been heard yet (two machines' frames do not make quite
    // the same number of samples)
    struct Stream {
        std::vector<int16_t> waiting;
        void Start() {
            waiting.assign(kSoundAhead * 2, 0);
        }
        void Add(const std::vector<int16_t> &sound) {
            waiting.insert(waiting.end(), sound.begin(), sound.end());
        }
        // Mixed into `sound` (as many samples as it has), `gain` of 256
        void Mix(std::vector<int16_t> &sound, int gain) {
            if (waiting.size() < sound.size()) {
                waiting.resize(sound.size(), 0);
            }
            for (size_t i = 0; i < sound.size(); i++) {
                sound[i] = static_cast<int16_t>(std::clamp(sound[i] + waiting[i] * gain / 256, -32768, 32767));
            }
            waiting.erase(waiting.begin(), waiting.begin() + static_cast<std::ptrdiff_t>(sound.size()));
        }
    } g_backSound, g_backNextSound;
    bool g_fast = false;
    size_t g_soundBefore = 0;

    void Fast(bool on) {
        if (on != g_fast) {
            g_fast = on;
            SetFastForward(on);
        }
    }

    void End() {
        if (g.kind == Kind::None) {
            return;
        }
        g = State();
        g_next.Clear();
        g_back.Clear();
        g_backNext.Clear();
        Wanted() = MatchWanted();
        CameraAt() = CameraWanted();
        Fast(false);
        SetHud({});
    }

    uint32_t Random() {
        g.random ^= g.random << 13;
        g.random ^= g.random >> 17;
        g.random ^= g.random << 5;
        return g.random;
    }
    // A character at random: not `not1`, not `not2`
    int AnyCharacter(int not1 = -1, int not2 = -1) {
        for (;;) {
            const int c = static_cast<int>(Random() % kCharacters);
            if (c != kNobody && c != not1 && c != not2) {
                return c;
            }
        }
    }
    // A feature's choice of character: 0 at random, then the characters in the order of their numbers
    int ChosenCharacter(const char *feature) {
        int choice = FeatureValue(feature);
        if (choice <= 0) {
            return -1;
        }
        for (int c = 0; c < kCharacters; c++) {
            if (c != kNobody && --choice == 0) {
                return c;
            }
        }
        return -1;
    }
    // The endless battle's stage: its feature's choices are two ways of changing it, then the stages
    int ChosenStage() {
        const int choice = FeatureValue("endless_stage");
        return choice >= 2 && choice < 2 + kStages ? choice - 1 : static_cast<int>(Random() % kStages) + 1;
    }
    int AnyStage(int notThis) {
        for (;;) {
            const int stage = static_cast<int>(Random() % kStages) + 1;
            if (stage != notThis) {
                return stage;
            }
        }
    }
    bool Layered() {
        return g.way != kStageOne;
    }

    void Start(Machine &machine, Kind kind) {
        EndModes();
        End();
        SetFeature("training_mode", false);
        g.kind = kind;
        g.phase = Phase::Starting;
        g.random = std::random_device()() | 1;
        if (kind == Kind::Survival) {
            g.characters[0] = -1; // (the player's choice)
            g.characters[1] = AnyCharacter();
            g.computer[1] = true;
            g.stage = 0;
        } else {
            g.characters[0] = ChosenCharacter("endless_one");
            g.characters[1] = ChosenCharacter("endless_two");
            if (g.characters[0] < 0) {
                g.characters[0] = AnyCharacter(g.characters[1]);
            }
            if (g.characters[1] < 0) {
                g.characters[1] = AnyCharacter(g.characters[0]);
            }
            g.computer[0] = g.computer[1] = true;
            g.stage = ChosenStage();
            const int choice = FeatureValue("endless_stage");
            g.way = choice == 0 ? kStageShuffle : choice == 1 ? kStageScroll : kStageOne;
            if (Layered()) {
                g.back = g.stage;
                g.backCamera = kLeftEnd;
                g_backSound.Start();
            }
        }
        machine.Reset();
    }
    void StartSurvival(Machine &machine) {
        Start(machine, Kind::Survival);
    }
    void StartEndless(Machine &machine) {
        Start(machine, Kind::Endless);
    }

    void Want() {
        Wanted().stage = g.stage;
        Wanted().characters[0] = g.characters[0];
        Wanted().characters[1] = g.characters[1];
    }

    // Who the game plays, told to the game: the players' records say (vsav.h). Both stay players that are in
    // the game, or the panel would count down for the one that is not to come back.
    void SayWhoPlays() {
        for (int n = 0; n < 2; n++) {
            SetRam8(Player(n) + kComputer, g.computer[n] ? 1 : 0);
            SetRam8(Player(n) + kPlays, 2);
        }
    }

    void BeforeFrame(Machine &machine) {
        g_soundBefore = machine.audio.size();
        Want();
        uint16_t &one = machine.players[0], &two = machine.players[1];
        if (g.phase == Phase::Starting) {
            // Survival: the player chooses a character, at the game's own pace; the rest goes by unseen
            const bool choosing = g.kind == Kind::Survival && Ram16(kMode) == 2 && Ram16(kGameStep) == kGameChoosing && Ram8(kPlayersIn) == 3;
            Fast(!choosing);
            if (ToRound(machine, g.frames, g.kind == Kind::Survival) && (!Layered() || g_back.ready)) {
                Fast(false);
                g.phase = Phase::Fighting;
                g.since = 0;
                g.fadeIn = kFade;
                g.characters[0] = Ram8(Player(0) + kCharacter);
                g.characters[1] = Ram8(Player(1) + kCharacter);
                if (Layered()) {
                    PlaySound(kTuneOff); // (the music is the stage's, from its own machine)
                }
            } else if (g.frames > kStartMost && !choosing) {
                End();
            }
            return;
        }
        if (g.phase == Phase::Over) {
            one = two = 0;
            return;
        }
        if (g.kind == Kind::Endless) {
            // A press takes a fighter over; none for a while gives it back
            const uint16_t held[2] = {one, two};
            for (int n = 0; n < 2; n++) {
                if (held[n] & kAnyButton) {
                    g.computer[n] = false;
                    g.idle[n] = 0;
                } else if (!g.computer[n] && ++g.idle[n] > kIdle) {
                    g.computer[n] = true;
                }
            }
        }
        // (a fighter the game plays hears nothing of the controls; nobody's Start or coin does anything)
        one = g.computer[0] ? 0 : one & kAnyButton;
        two = g.computer[1] ? 0 : two & kAnyButton;
        SayWhoPlays();
        if (g.way == kStageScroll) {
            // The floor moves under the fighters: they and what they have thrown are carried left with it,
            // and the picture's left edge (the fighters' camera stays where it is) pushes them along
            CameraAt().x = kArena;
            const uint32_t by = static_cast<uint32_t>(kScrollBy) << 8;
            for (int n = 0; n < kPlayerRecords; n++) {
                const uint32_t record = kPlayer1 + static_cast<uint32_t>(n) * kPlayerSize;
                if (Ram8(record + kActive)) {
                    SetRam32(record + kX, Ram32(record + kX) - by);
                }
            }
            for (int n = 0; n < kThrownRecords; n++) {
                const uint32_t record = kThrown + static_cast<uint32_t>(n) * kThrownSize;
                if (Ram8(record + kActive)) {
                    SetRam32(record + kX, Ram32(record + kX) - by);
                }
            }
        }
    }

    // --- the stage behind the fighters ---

    // One of the stage machines for a frame: on its way to its round, or showing its stage with the camera
    // where it is wanted
    void RunStage(Machine &machine, Spare &spare, int stage, int cameraX, int cameraY, Made *made) {
        RunSpare(machine, spare, [&](Machine &m) {
            Wanted().stage = stage;
            if (!spare.ready) {
                spare.ready = ToRound(m, spare.frames);
            }
            if (spare.ready) {
                m.players[0] = m.players[1] = 0;
                m.video.hideObject = NotTheStage;
                CameraAt().x = cameraX;
                CameraAt().y = cameraY;
            }
        }, made);
    }

    // After the fighters' frame: the stage machines' frames, and the picture and sound put together. `show`
    // of 256 is how much of the fighters is seen (they fade when one match follows another).
    void Layer(Machine &machine, int show) {
        static Made fighters, back, next;
        Take(machine, g_soundBefore, fighters);
        const int cameraY = static_cast<int16_t>(Ram16(kCameraY));
        const int cameraX = g.way == kStageScroll ? g.backCamera / 256 : static_cast<int16_t>(Ram16(kCameraX));
        const bool fighting = g.phase != Phase::Starting;

        // The stage after this one: wanted when this one has been fought on long enough (shuffle), or as
        // soon as this one is the one shown (scroll)
        if (fighting && !g.backNext && (g.way == kStageScroll || g.backFor >= kShuffleAfter)) {
            g.backNext = AnyStage(g.back);
            g_backNext.Clear();
            g_backNextSound.Start();
        }
        RunStage(machine, g_back, g.back, cameraX, cameraY, &back);
        if (!g_back.ready) {
            RunStage(machine, g_back, g.back, cameraX, cameraY, &back); // (twice as fast as the fighters' machine on the way)
            Give(machine, fighters);
            return;
        }
        g_backSound.Add(back.sound);
        bool nextShown = false;
        if (g.backNext) {
            const bool wasReady = g_backNext.ready;
            for (int n = 0; n < (wasReady ? 1 : 3); n++) {
                RunStage(machine, g_backNext, g.backNext, g.way == kStageScroll ? kCameraLeft : cameraX, cameraY, &next);
                if (g_backNext.ready) {
                    break;
                }
            }
            if (wasReady) {
                g_backNextSound.Add(next.sound);
                nextShown = true;
            }
        }
        if (!fighting) {
            g_backSound.waiting.resize(kSoundAhead * 2); // (not heard yet)
            Give(machine, fighters);
            return;
        }
        g.backFor++;

        // The stage's picture: one stage, or two (faded, or side by side)
        int nextGain = 0;
        if (nextShown && g.way == kStageShuffle) {
            g.blend = std::min(g.blend + 1, kBlend);
            nextGain = g.blend * 256 / kBlend;
            for (size_t i = 0; i < back.picture.size(); i++) {
                const uint32_t a = back.picture[i], b = next.picture[i];
                const uint32_t rb = (((a & 0xFF00FF) * static_cast<uint32_t>(256 - nextGain) + (b & 0xFF00FF) * static_cast<uint32_t>(nextGain)) >> 8) & 0xFF00FF;
                const uint32_t gr = (((a & 0x00FF00) * static_cast<uint32_t>(256 - nextGain) + (b & 0x00FF00) * static_cast<uint32_t>(nextGain)) >> 8) & 0x00FF00;
                back.picture[i] = rb | gr;
            }
        } else if (g.way == kStageScroll) {
            // (onwards: across this stage, then this stage out and the next in, if that one is ready)
            if (g.slide > 0 || g.backCamera + kScrollBy >= kRightEnd) {
                g.backCamera = kRightEnd;
                g.slide = nextShown ? std::min(g.slide + kScrollBy, kAcross) : 0;
            } else {
                g.backCamera += kScrollBy;
            }
            const int slide = g.slide / 256;
            nextGain = slide * 256 / Video::kWidth;
            if (slide > 0) {
                for (int y = 0; y < Video::kHeight; y++) {
                    uint32_t *row = &back.picture[static_cast<size_t>(y) * Video::kWidth];
                    memmove(row, row + slide, static_cast<size_t>(Video::kWidth - slide) * sizeof(uint32_t));
                    memcpy(row + Video::kWidth - slide, &next.picture[static_cast<size_t>(y) * Video::kWidth], static_cast<size_t>(slide) * sizeof(uint32_t));
                }
            }
        }
        // The fighters over it: the pixels the fighters' machine drew an object at
        for (size_t i = 0; i < back.picture.size(); i++) {
            const uint32_t f = fighters.picture[i];
            if (f & 0xFF000000) {
                if (show >= 256) {
                    back.picture[i] = f & 0xFFFFFF;
                } else {
                    const uint32_t b = back.picture[i], keep = static_cast<uint32_t>(256 - show), put = static_cast<uint32_t>(show);
                    back.picture[i] = ((((b & 0xFF00FF) * keep + (f & 0xFF00FF) * put) >> 8) & 0xFF00FF) | ((((b & 0x00FF00) * keep + (f & 0x00FF00) * put) >> 8) & 0x00FF00);
                }
            }
        }
        fighters.picture = back.picture;
        // The sound: the fighters', and the stages' music, one fading into the other
        g_backSound.Mix(fighters.sound, 256 - nextGain);
        if (nextShown) {
            g_backNextSound.Mix(fighters.sound, nextGain);
        }
        // The next stage is all there is: it is the stage now
        if ((g.way == kStageShuffle && g.blend >= kBlend) || (g.way == kStageScroll && g.slide >= kAcross)) {
            g_back = std::move(g_backNext);
            g_backNext.Clear();
            g_backSound = g_backNextSound;
            g.back = g.backNext;
            g.backNext = 0;
            g.backFor = 0;
            g.blend = 0;
            g.slide = 0;
            g.backCamera = kLeftEnd;
        }
        Give(machine, fighters);
    }

    // The picture nearer black by `part` of 256
    void Darken(Machine &machine, int part) {
        const int keep = 256 - std::clamp(part, 0, 256);
        uint32_t *picture = machine.Picture();
        for (int i = 0; i < Video::kWidth * Video::kHeight; i++) {
            const uint32_t p = picture[i];
            picture[i] = ((((p & 0xFF00FF) * static_cast<uint32_t>(keep)) >> 8) & 0xFF00FF) | ((((p & 0x00FF00) * static_cast<uint32_t>(keep)) >> 8) & 0x00FF00);
        }
    }

    // The next match made ready on the spare machine, a few frames a frame until its round is on
    void MakeNext(Machine &machine) {
        for (int n = 0; n < kOnTheWay && !g_next.ready && g_next.frames <= kStartMost; n++) {
            RunSpare(machine, g_next, [](Machine &spare) {
                Want();
                g_next.ready = ToRound(spare, g_next.frames);
            });
        }
    }

    void Frame(Machine &machine) {
        if (g.phase != Phase::Starting && Ram16(kMode) == 0) {
            End(); // (the board was reset)
            return;
        }
        std::vector<HudLine> lines;
        int dark = 0;         // how far the fighters (or, with one stage, the whole picture) are faded, of 256
        bool layered = false; // the picture and sound have been put together already
        if (g.phase == Phase::Fighting) {
            const uint8_t result = Ram8(kResult);
            if (result) {
                g.winner = result == 2 ? 1 : 0; // (both down at once: the first player's)
                g.phase = Phase::Decided;
                g.since = 0;
                g_next.Clear();
                if (g.kind == Kind::Survival) {
                    if (result != 1) {
                        g.phase = Phase::Over;
                        if (g.score > FeatureValue("survival_best")) {
                            SetFeatureValue("survival_best", std::min(g.score, 99));
                        }
                    } else {
                        g.score++;
                        const auto left = [](uint32_t which) {
                            const int now = static_cast<int16_t>(Ram16(Player(0) + which));
                            return std::min<int>(std::max(now, 0) + kHealthBack, kHealthFull);
                        };
                        g.health = left(kHealth);
                        g.ceiling = std::max(left(kHealthCeiling), g.health);
                        g.characters[1] = AnyCharacter(g.characters[1]);
                    }
                } else {
                    // The one knocked down is replaced (by the one chosen for that side in the menu, if that
                    // is not who has just lost)
                    const int loser = 1 - g.winner;
                    const int chosen = ChosenCharacter(loser == 0 ? "endless_one" : "endless_two");
                    g.characters[loser] = chosen >= 0 && chosen != g.characters[loser] ? chosen : AnyCharacter(g.characters[0], g.characters[1]);
                    g.score++;
                    if (!Layered()) {
                        g.stage = ChosenStage();
                    }
                }
            }
        } else if (g.phase == Phase::Decided) {
            g.since++;
            MakeNext(machine);
            if (g.since > kAfter - kFade) {
                dark = (g.since - (kAfter - kFade)) * 256 / kFade;
            }
            if (g.since >= kAfter && g_next.ready) {
                if (Layered()) {
                    Layer(machine, 0); // (this frame's stage and sound, before the machine is another)
                    layered = true;
                }
                Promote(machine, g_next);
                if (g.kind == Kind::Survival) {
                    SetRam16(Player(0) + kHealth, static_cast<uint16_t>(g.health));
                    SetRam16(Player(0) + kHealthCeiling, static_cast<uint16_t>(g.ceiling));
                }
                if (Layered()) {
                    PlaySound(kTuneOff);
                }
                dark = 256;
                g.phase = Phase::Fighting;
                g.since = 0;
                g.fadeIn = kFade;
            } else if (g_next.frames > kStartMost) {
                End();
                return;
            }
        } else if (g.phase == Phase::Over) {
            g.since++;
            char text[64];
            lines.push_back({0.5f, 0.36f, 0, "SURVIVAL OVER"});
            snprintf(text, sizeof(text), "Opponents beaten: %d", g.score);
            lines.push_back({0.5f, 0.46f, 0, text});
            snprintf(text, sizeof(text), "Best: %d", std::max(g.score, FeatureValue("survival_best")));
            lines.push_back({0.5f, 0.54f, 0, text});
            if (g.since > kOverFor) {
                End();
                machine.Reset();
                return;
            }
        }
        if (g.phase == Phase::Fighting && g.fadeIn > 0) {
            dark = std::max(dark, g.fadeIn * 256 / kFade);
            g.fadeIn--;
        }
        if (Layered()) {
            if (!layered) {
                Layer(machine, 256 - dark);
            }
        } else if (dark) {
            Darken(machine, dark);
        }
        if (g.kind == Kind::Survival && g.phase != Phase::Starting && g.phase != Phase::Over) {
            char text[32];
            snprintf(text, sizeof(text), "Beaten: %d", g.score);
            lines.push_back({0.5f, 0.21f, 0, text});
        }
        SetHud(std::move(lines));
        SetBoxes({});
    }
} // namespace

bool BattleMode() {
    return g.kind != Kind::None;
}
void EndBattle() {
    End();
}
bool BattleNoClock() {
    return g.kind == Kind::Endless;
}
bool BattleNoKnockOut() {
    return false;
}

void BattleBeforeFrame(Machine &machine) {
    if (g.kind != Kind::None) {
        BeforeFrame(machine);
    }
}

void BattleFrame(Machine &machine) {
    if (g.kind != Kind::None) {
        Frame(machine);
    }
    // What the next frame of the port's machine draws: all of it; or no panel; or the fighters alone, marked,
    // to be laid over the stage that another machine shows
    if (g.kind != Kind::None || !StageViewer()) {
        const bool fighting = g.kind == Kind::Endless && g.phase != Phase::Starting;
        const bool layered = fighting && Layered();
        machine.video.hideObject = layered ? NotTheFighters : fighting ? ThePanel : nullptr;
        machine.video.layerMask = layered ? 1 : 0xF;
        machine.video.markObjects = layered;
    }
}

void BattleState(Archive &ar) {
    ar.Mark(0x42415454);
    ar.Pod(g);
    ar.Vec(g_next.state);
    ar.Pod(g_next.ready);
    ar.Pod(g_next.frames);
    for (Spare *spare : {&g_back, &g_backNext}) {
        ar.Vec(spare->state);
        ar.Pod(spare->ready);
        ar.Pod(spare->frames);
    }
    if (!ar.Saving()) {
        g_fast = false;
        g_backSound.Start();
        g_backNextSound.Start();
    }
}

void RegisterBattle() {
    Game &game = TheGame();
    game.pages.push_back({kSurvival, kModes, "One opponent after another on the health you have left. How many can you beat?"});
    game.pages.push_back({kEndless, kModes, "Two fighters the game plays, for as long as you like. Touch the controls to take one over."});
    game.commands.push_back({"survival_start", kSurvival, "Start",
                             "Choose a character; then one opponent after another, each on a knock-down. Your health is what the "
                             "fight before left you, and a little back. Over the first time you go down.",
                             StartSurvival});
    {
        // (kept with the features so that it is remembered; the numbers are its choices)
        static std::vector<std::string> numbers;
        std::vector<const char *> choices;
        for (int n = 0; n < 100; n++) {
            numbers.push_back(std::to_string(n));
        }
        for (const std::string &number : numbers) {
            choices.push_back(number.c_str());
        }
        game.features.push_back({"survival_best", kSurvival, "Best", "The most opponents beaten in one survival. (Left puts it back.)", 0, choices});
    }
    game.commands.push_back({"endless_start", kEndless, "Start",
                             "Two fighters, played by the game, with no bars and no clock. The one knocked down is replaced. Press "
                             "anything on a player's controls to take that fighter over; leave them alone for ten seconds to give "
                             "it back. Reset ends it.",
                             StartEndless});
    std::vector<const char *> fighters = {"Random"};
    for (const char *name : kNames) {
        if (name) {
            fighters.push_back(name);
        }
    }
    game.features.push_back({"endless_one", kEndless, "Fighter 1", "Who is on player 1's side: someone new each time, or always the same.", 0, fighters});
    game.features.push_back({"endless_two", kEndless, "Fighter 2", "Who is on player 2's side: someone new each time, or always the same.", 0, fighters});
    game.features.push_back({"endless_stage", kEndless, "Stage",
                             "Where they fight. Shuffle: a stage that fades into another every two minutes. Scroll: the stages "
                             "go by, one after another, and the fighters are carried along. Or one stage.",
                             0,
                             {"Shuffle", "Scroll", "Feast of the Damned", "Concrete Cave", "Tower of Arrogance", "Red Thirst", "Deserted Chateau",
                              "Abaraya", "Vanity Paradise", "War Agony", "Forever Torment", "Green Scream", "Iron Horse, Iron Terror",
                              "Fetus of God"}});
}

} // namespace cps2recomp
