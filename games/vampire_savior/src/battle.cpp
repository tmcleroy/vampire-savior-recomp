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
    } g;
    Spare g_next;
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
        Wanted() = MatchWanted();
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
            if (ToRound(machine, g.frames, g.kind == Kind::Survival)) {
                Fast(false);
                g.phase = Phase::Fighting;
                g.since = 0;
                g.fadeIn = kFade;
                g.characters[0] = Ram8(Player(0) + kCharacter);
                g.characters[1] = Ram8(Player(1) + kCharacter);
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
                    g.stage = ChosenStage();
                }
            }
        } else if (g.phase == Phase::Decided) {
            g.since++;
            MakeNext(machine);
            if (g.since > kAfter - kFade) {
                Darken(machine, (g.since - (kAfter - kFade)) * 256 / kFade);
            }
            if (g.since >= kAfter && g_next.ready) {
                Promote(machine, g_next);
                if (g.kind == Kind::Survival) {
                    SetRam16(Player(0) + kHealth, static_cast<uint16_t>(g.health));
                    SetRam16(Player(0) + kHealthCeiling, static_cast<uint16_t>(g.ceiling));
                }
                Darken(machine, 256);
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
            Darken(machine, g.fadeIn * 256 / kFade);
            g.fadeIn--;
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
    if (g.kind != Kind::None || !StageViewer()) {
        machine.video.hideObject = g.kind == Kind::Endless && g.phase != Phase::Starting ? ThePanel : nullptr;
    }
}

void BattleState(Archive &ar) {
    ar.Mark(0x42415454);
    ar.Pod(g);
    ar.Vec(g_next.state);
    ar.Pod(g_next.ready);
    ar.Pod(g_next.frames);
    if (!ar.Saving()) {
        g_fast = false;
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
    game.features.push_back({"endless_stage", kEndless, "Stage", "Where they fight: somewhere new for each fight, or one stage.", 0,
                             {"Shuffle", "Scroll", "Feast of the Damned", "Concrete Cave", "Tower of Arrogance", "Red Thirst", "Deserted Chateau",
                              "Abaraya", "Vanity Paradise", "War Agony", "Forever Torment", "Green Scream", "Iron Horse, Iron Terror",
                              "Fetus of God"}});
}

} // namespace cps2recomp
