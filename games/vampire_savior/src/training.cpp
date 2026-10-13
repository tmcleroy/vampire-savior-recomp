// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy
//
// Vampire Savior: training aids. Optional features in the categories "Training" and "Dummy": a readout of the
// damage hits and combos do, a display of what the players press, health that comes back, and a second
// player that stands, crouches, jumps, guards, or does again what was recorded. Hits are counted where the
// game takes their damage off (cheats.cpp has the hook); the dummy is worked by changing what the game is
// told player 2 presses; the rest looks at the game's memory after every frame.
#include "boxes.h"
#include "frames.h"
#include "machine.h"
#include "modes.h"
#include "state.h"
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
    constexpr int kShownFor = 240;   // frames a readout stays up after its last hit
    constexpr size_t kRecordingMost = 20 * 60; // frames of a recording
    constexpr int kReplayPause = 60; // frames between one replay and the next
    constexpr int kInputLines = 7;   // lines of the input display

    constexpr uint16_t kSticks = kLeft | kRight | kUp | kDown;
    constexpr uint16_t kButtons = kPunch1 | kPunch2 | kPunch3 | kKick1 | kKick2 | kKick3;
    // What has happened to one player
    struct Taken {
        uint16_t last = 0;     // the last hit's damage
        uint16_t combo = 0;    // the damage of the combo: the hits that landed without the player getting free
        uint16_t comboBack = 0; //   and how much of that comes back by itself
        uint16_t hits = 0;
        uint16_t best = 0;     // the most a combo has done this round
        int32_t since = 1 << 20; // frames since the last hit
    };
    // One line of the input display: what was held, and for how many frames
    struct Held {
        uint16_t buttons = 0;
        uint16_t frames = 0;
    };
    struct State {
        Taken taken[2];
        bool fighting = false;
        Held held[2][kInputLines]; // [0] the newest
        // The dummy
        bool recording = false, replaying = false;
        uint32_t replayAt = 0;
        int32_t replayPause = 0;
        int32_t recordSlot = 0, replaySlot = 0;
        bool reversal = false;        // a recording is being played once, as an answer
        Doing dummyWas = Doing::Free; // what the dummy was doing the frame before
        int32_t stunFrames = 0;       // frames it has been reeling (or held) for
        bool inRange = false, guardThis = false; // an attack is in range, and the dummy has chosen to guard it
        uint32_t random = 0x2545F491;
    } g;
    // Starts the aids afresh (a round starting, or started over), but for what the dummy was told to do
    void Afresh(bool fighting) {
        State fresh;
        fresh.fighting = fighting;
        fresh.recording = g.recording;
        fresh.replaying = g.replaying;
        fresh.recordSlot = g.recordSlot;
        fresh.replaySlot = g.replaySlot;
        fresh.random = g.random;
        g = fresh;
        FramesReset();
    }
    constexpr int kSlots = 3;
    std::vector<uint16_t> g_recordings[kSlots]; // what player 2 held, frame by frame, in each slot
    constexpr int kGuardAfterHit = 90; // frames after a hit in which "after the first hit" guards
    constexpr int kDownAfter = 34;     // frames of reeling after which the dummy is taken to be knocked down
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
    // A recording keeps the stick as it would be with the dummy standing on the right, so that it works from
    // either side: this turns left and right about when the dummy stands on the left (both ways the same)
    uint16_t Turned(uint16_t held) {
        const uint16_t sides = held & (kLeft | kRight);
        if (!DummyOnRight() && (sides == kLeft || sides == kRight)) {
            held ^= kLeft | kRight;
        }
        return held;
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
            camera = kCameraLeft;
        } else if (place % 3 == 2) {
            x[0] = kStageRight - kCornerApart;
            x[1] = kStageRight;
            camera = kCameraRight;
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
        // The characters a mode wants, in place of the ones chosen (two alike: the second in its other colours)
        const MatchWanted &wanted = Wanted();
        for (int n = 0; n < 2; n++) {
            if (wanted.characters[n] >= 0) {
                SetRam8(Player(n) + kCharacter, static_cast<uint8_t>(wanted.characters[n]));
                SetRam8(Player(n) + kCharacterAgain, static_cast<uint8_t>(wanted.characters[n]));
            }
        }
        if (wanted.characters[0] >= 0 && wanted.characters[1] >= 0) {
            SetRam8(Player(1) + kColours, wanted.characters[0] == wanted.characters[1] ? Ram8(Player(0) + kColours) ^ 1 : Ram8(Player(1) + kColours));
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

    // --- the dummy: before a frame, what the game is told player 2 presses ---

    uint32_t Random() {
        g.random ^= g.random << 13;
        g.random ^= g.random >> 17;
        g.random ^= g.random << 5;
        return g.random;
    }

    std::vector<uint16_t> &Recording() {
        return g_recordings[std::clamp(FeatureValue("dummy_slot"), 0, kSlots - 1)];
    }

    // A recording is played from its start: the one of the slot chosen, or (any == true) of any slot with
    // something in it. False if there is nothing to play.
    bool StartReplay(bool any) {
        g.replaySlot = std::clamp(FeatureValue("dummy_slot"), 0, kSlots - 1);
        if (any) {
            int filled[kSlots], count = 0;
            for (int n = 0; n < kSlots; n++) {
                if (!g_recordings[n].empty()) {
                    filled[count++] = n;
                }
            }
            if (count) {
                g.replaySlot = filled[Random() % static_cast<uint32_t>(count)];
            }
        }
        g.replayAt = 0;
        return !g_recordings[g.replaySlot].empty();
    }

    // The stick turned to mean "away from player 1" (kLeft given) or "towards" (kRight), for where the dummy is
    uint16_t Away() {
        return DummyOnRight() ? kRight : kLeft;
    }
    uint16_t Towards() {
        return DummyOnRight() ? kLeft : kRight;
    }

    void Dummy(Machine &machine) {
        const bool record = FeatureOn("dummy_record");
        const bool replayAny = FeatureOn("dummy_replay_any"), replay = FeatureOn("dummy_replay") || replayAny;
        if (record && !g.recording) {
            Recording().clear(); // (switched on: a new recording)
        }
        if (!record && g.recording) {
            // (switched off: without the standing about before the first press and after the last, which
            // is only the time it took to work the menu)
            std::vector<uint16_t> &made = g_recordings[g.recordSlot];
            while (!made.empty() && made.back() == 0) {
                made.pop_back();
            }
            size_t lead = 0;
            while (lead < made.size() && made[lead] == 0) {
                lead++;
            }
            made.erase(made.begin(), made.begin() + static_cast<std::ptrdiff_t>(lead));
        }
        if (replay && !g.replaying) {
            StartReplay(replayAny);
            g.replayPause = 0;
        }
        g.recording = record;
        g.replaying = replay;
        if (record) {
            g.recordSlot = std::clamp(FeatureValue("dummy_slot"), 0, kSlots - 1);
        }
        if (!Fighting()) {
            return;
        }
        uint16_t &one = machine.players[0], &two = machine.players[1];
        const uint16_t keep = two & (kStart | kCoin);
        if (record) {
            // Player 1's controls work player 2, and are kept
            if (Recording().size() < kRecordingMost) {
                Recording().push_back(Turned(one & (kSticks | kButtons)));
            }
            two = static_cast<uint16_t>(keep | (one & (kSticks | kButtons)));
            one &= kStart | kCoin;
            return;
        }

        // What the dummy has just come out of
        const Doing doing = FramesDoing(1);
        const bool reeling = doing == Doing::HitStun || doing == Doing::GuardStun || doing == Doing::Held;
        g.stunFrames = reeling ? g.stunFrames + 1 : 0;
        const int reversal = FeatureValue("dummy_reversal"); // 1 after guarding, 2 after being hit, 3 either
        if (doing == Doing::Free && g.dummyWas != Doing::Free && !g.reversal) {
            const bool guarded = g.dummyWas == Doing::GuardStun;
            const bool hit = g.dummyWas == Doing::HitStun || g.dummyWas == Doing::Held;
            if (((guarded && (reversal & 1)) || (hit && (reversal & 2))) && StartReplay(replayAny)) {
                g.reversal = true;
            }
        }
        g.dummyWas = doing;

        // What it does left alone
        uint16_t held = two & (kSticks | kButtons); // (nothing chosen: player 2's own controls)
        bool busy = false;                          // doing something of a recording's
        if (g.reversal) {
            const std::vector<uint16_t> &recording = g_recordings[g.replaySlot];
            held = Turned(recording[g.replayAt]);
            busy = true;
            if (++g.replayAt >= recording.size()) {
                g.reversal = false;
                g.replayAt = 0;
                g.replayPause = kReplayPause;
            }
        } else if (replay) {
            const std::vector<uint16_t> &recording = g_recordings[g.replaySlot];
            held = 0;
            if (recording.empty()) {
                StartReplay(replayAny);
            } else if (g.replayPause > 0) {
                g.replayPause--;
            } else {
                held = Turned(recording[g.replayAt]);
                busy = true;
                if (++g.replayAt >= recording.size()) {
                    StartReplay(replayAny);
                    g.replayPause = kReplayPause;
                }
            }
        } else if (FeatureOn("dummy_crouch")) {
            held = kDown;
        } else if (FeatureOn("dummy_jump")) {
            held = kUp;
        } else if (FeatureOn("dummy_stand")) {
            held = 0;
        }

        // Guarding: when the game's own test says something is near enough to guard (an attack of player
        // 1's, or anything it has thrown): away from player 1, standing or crouching as the attack needs.
        // (Holding away all the time would walk it into the corner.)
        const int guard = FeatureValue("dummy_guard"); // 1 always, 2 after the first hit, 3 at random
        const bool inRange = guard && !busy && AttackInRange(machine.cpu, Player(1));
        if (inRange && !g.inRange) {
            g.guardThis = guard == 1 || (guard == 2 && g.taken[1].since < kGuardAfterHit) || (guard == 3 && (Random() & 0x100));
        }
        g.inRange = inRange;
        if (inRange && (g.guardThis || doing == Doing::GuardStun)) {
            held = static_cast<uint16_t>(Away() | (GuardStanding(machine.cpu, Player(1)) ? 0 : kDown));
        }

        // While guarding a hit
        if (doing == Doing::GuardStun) {
            const int phase = g.stunFrames;
            switch (FeatureValue("dummy_guard_action")) {
            case 1: { // pushes the attacker away: buttons pressed one after another, guarding still
                static constexpr uint16_t kMash[] = {kPunch1, 0, kPunch2, 0, kPunch3, 0, kKick1, 0, kKick2, 0, kKick3, 0};
                held |= kMash[phase % 12];
                break;
            }
            case 2:
            case 3: { // a guard cancel: towards, down, down and towards with a button, again and again
                const uint16_t button = FeatureValue("dummy_guard_action") == 2 ? kPunch2 : kKick2;
                static constexpr int kEach = 2;
                const int at = (phase / kEach) % 4;
                held = at == 0 ? Towards() : at == 1 ? kDown : at == 2 ? static_cast<uint16_t>(kDown | Towards() | button) : 0;
                break;
            }
            default: break;
            }
        }
        // Thrown: the throw's own command softens it ("tech hit")
        if (doing == Doing::Held && FeatureOn("dummy_tech")) {
            held = static_cast<uint16_t>(Away() | ((g.stunFrames & 1) ? (kPunch2 | kPunch3) : 0));
        }
        // Knocked down: the stick held with a button tapped rolls it along the floor before it gets up
        if (doing == Doing::HitStun && g.stunFrames > kDownAfter) {
            if (const int getUp = FeatureValue("dummy_getup")) {
                held = static_cast<uint16_t>((getUp == 1 ? Towards() : Away()) | ((g.stunFrames & 2) ? kPunch1 : 0));
            }
        }
        two = static_cast<uint16_t>(keep | held);
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

    // What is held, as fighting game players write it: the stick as a number on a keypad (5 in the middle,
    // 6 right, 2 down, ...), then the buttons
    std::string Notation(uint16_t held) {
        int column = 1, row = 1;
        if (held & kLeft) {
            column = 0;
        } else if (held & kRight) {
            column = 2;
        }
        if (held & kDown) {
            row = 0;
        } else if (held & kUp) {
            row = 2;
        }
        std::string text(1, static_cast<char>('1' + row * 3 + column));
        static const struct {
            uint16_t bit;
            const char *name;
        } kNames[] = {{kPunch1, "LP"}, {kPunch2, "MP"}, {kPunch3, "HP"}, {kKick1, "LK"}, {kKick2, "MK"}, {kKick3, "HK"}};
        for (const auto &button : kNames) {
            if (held & button.bit) {
                text += ' ';
                text += button.name;
            }
        }
        return text;
    }

    void Frame(Machine &machine) {
        if (StageViewer() || BattleMode()) {
            // (nothing of training's is shown, or kept)
            g.fighting = false;
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
        if (!fighting || !g.fighting) {
            // (a round starting: start afresh)
            Afresh(fighting);
            SetHud({});
            SetBoxes({});
            g_nextBoxes.clear();
            return;
        }
        std::vector<HudLine> lines;
        for (int n = 0; n < 2; n++) {
            Taken &t = g.taken[n];
            if (t.since < (1 << 20)) {
                t.since++;
            }
            const uint16_t health = Ram16(Player(n) + kHealth);
            if (refill && t.since > kRefillAfter && health < kHealthFull) {
                SetRam16(Player(n) + kHealth, kHealthFull);
                SetRam16(Player(n) + kHealthCeiling, kHealthFull);
            }
            const float x = n == 0 ? 0.03f : 0.97f;
            const int align = n == 0 ? -1 : 1;
            if (readout && t.hits && t.since < kShownFor) {
                // The damage a player took is shown on that player's side, under the health bar
                char text[64];
                if (t.hits > 1) {
                    snprintf(text, sizeof(text), "%u hits  %u", t.hits, t.combo);
                } else {
                    snprintf(text, sizeof(text), "%u", t.last);
                }
                lines.push_back({x, 0.17f, align, text});
                // (how much of it is lost for good, and how much is the white part of the bar that comes back;
                // and the round's best)
                snprintf(text, sizeof(text), "%u for good, %u back   best %u", t.combo - t.comboBack, t.comboBack, t.best);
                lines.push_back({x, 0.23f, align, text});
            }
            // What the player holds, as the game read it off the panel this frame (input.cpp: the dummy's
            // too), newest on top, with how long
            const uint16_t held = PanelHeld(n);
            Held *history = g.held[n];
            if (held == history[0].buttons && history[0].frames) {
                if (history[0].frames < 999) {
                    history[0].frames++;
                }
            } else {
                for (int i = kInputLines - 1; i > 0; i--) {
                    history[i] = history[i - 1];
                }
                history[0] = {held, 1};
            }
            if (inputs) {
                for (int i = 0; i < kInputLines && history[i].frames; i++) {
                    char text[48];
                    const std::string what = Notation(history[i].buttons);
                    if (n == 0) {
                        snprintf(text, sizeof(text), "%3u  %s", history[i].frames, what.c_str());
                    } else {
                        snprintf(text, sizeof(text), "%s  %3u", what.c_str(), history[i].frames);
                    }
                    lines.push_back({x, 0.30f + 0.062f * static_cast<float>(i), align, text});
                }
            }
        }
        std::vector<HudBox> boxes;
        FramesHud(FeatureOn("frame_data"), FeatureOn("frame_meter"), lines, boxes);
        if (FeatureOn("hit_boxes")) {
            // What the game's memory says now is what the next picture shows, so the boxes are drawn a frame
            // after they are worked out
            boxes.insert(boxes.begin(), g_nextBoxes.begin(), g_nextBoxes.end());
            g_nextBoxes.clear();
            const int cameraX = static_cast<int16_t>(Ram16(kCameraX)), cameraY = static_cast<int16_t>(Ram16(kCameraY));
            for (const PlacedBox &box : AllBoxes(machine)) {
                HudBox shown;
                shown.x = box.x - box.halfWidth - cameraX;
                shown.y = kPictureFloor - (box.y - cameraY) - box.halfHeight;
                shown.width = box.halfWidth * 2;
                shown.height = box.halfHeight * 2;
                switch (box.kind) {
                case BoxKind::Attack: shown.r = 255, shown.g = box.thrown ? 140 : 40, shown.b = 40; break;
                case BoxKind::Push: shown.r = 60, shown.g = 220, shown.b = 90; break;
                default: shown.r = 70, shown.g = 130, shown.b = 255; break;
                }
                g_nextBoxes.push_back(shown);
            }
        } else {
            g_nextBoxes.clear();
        }
        SetBoxes(std::move(boxes));
        if (g.recording || g.replaying) {
            char text[48];
            const std::vector<uint16_t> &recording = g_recordings[g.recording ? g.recordSlot : g.replaySlot];
            if (g.recording) {
                snprintf(text, sizeof(text), "REC %d  %.1f s", g.recordSlot + 1, recording.size() / 60.0);
            } else if (recording.empty()) {
                snprintf(text, sizeof(text), "nothing recorded");
            } else {
                snprintf(text, sizeof(text), "PLAY %d  %.1f / %.1f s", g.replaySlot + 1, g.replayAt / 60.0, recording.size() / 60.0);
            }
            lines.push_back({0.5f, 0.17f, 0, text});
        }
        SetHud(std::move(lines));
    }
} // namespace

// A hit's damage is being taken off a player's health (from the hook in cheats.cpp)
void TrainingHit(int player, unsigned forGood, unsigned besides) {
    Taken &t = g.taken[player];
    // A combo goes on while the player has not got free since the last hit
    const Doing doing = FramesDoing(player);
    if (!(t.hits && (doing == Doing::HitStun || doing == Doing::Held))) {
        t.combo = t.comboBack = 0;
        t.hits = 0;
    }
    t.last = static_cast<uint16_t>(forGood + besides);
    t.combo = static_cast<uint16_t>(t.combo + forGood + besides);
    t.comboBack = static_cast<uint16_t>(t.comboBack + besides);
    t.hits++;
    t.best = std::max(t.best, t.combo);
    t.since = 0;
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
    ar.Pod(g);
    for (auto &recording : g_recordings) {
        ar.Vec(recording);
    }
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
    game.features.push_back({"frame_meter", "Training mode", "Frame meter",
                             "A row of cells for each player, one for every step of the game: green before a move can hit, red "
                             "while it can, blue after; yellow reeling from a hit, light blue from a guarded one; grey in the air."});
    game.features.push_back({"health_refill", "Training mode", "Health refills",
                             "A player's health comes back when they have not been hit for a moment."});
    game.features.push_back({"dummy_stand", "Dummy", "Stands", "Player 2 stands still, whatever its controls do.", kFeatureExclusive});
    game.features.push_back({"dummy_crouch", "Dummy", "Crouches", "Player 2 crouches.", kFeatureExclusive});
    game.features.push_back({"dummy_jump", "Dummy", "Jumps", "Player 2 jumps on the spot, again and again.", kFeatureExclusive});
    game.features.push_back({"dummy_record", "Dummy", "Record",
                             "Player 1's controls work player 2, and what they do is recorded in the slot chosen (20 seconds at "
                             "most). Switch to Replay when done.",
                             kFeatureExclusive});
    game.features.push_back({"dummy_replay", "Dummy", "Replay", "Player 2 does what was recorded in the slot chosen, over and over.",
                             kFeatureExclusive});
    game.features.push_back({"dummy_replay_any", "Dummy", "Replay any slot",
                             "Player 2 does what was recorded, over and over, each time from a slot picked at random.", kFeatureExclusive});
    game.features.push_back({"dummy_slot", "Dummy", "Recording slot", "Which of three recordings Record makes and Replay plays.", 0,
                             {"1", "2", "3"}});
    game.features.push_back({"dummy_guard", "Dummy", "Guards",
                             "Player 2 guards player 1's attacks, standing or crouching as each needs: always; only for a moment "
                             "after being hit (to see whether a combo really is one); or one attack in two, at random.",
                             0, {"Off", "Always", "After a hit", "At random"}});
    game.features.push_back({"dummy_guard_action", "Dummy", "While guarding",
                             "What player 2 does while guarding a hit: presses buttons to push the attacker away, or does a guard "
                             "cancel (towards, down, down and towards with a punch or a kick: most characters have one or the other).",
                             0, {"Nothing", "Pushes away", "Guard cancel, punch", "Guard cancel, kick"}});
    game.features.push_back({"dummy_reversal", "Dummy", "Answers with the recording",
                             "Player 2 plays the recording once as soon as it can act after guarding, after being hit (or knocked "
                             "down, or thrown), or both.",
                             0, {"Off", "After guarding", "After being hit", "After either"}});
    game.features.push_back({"dummy_tech", "Dummy", "Softens throws", "Player 2 does a throw's own command while being thrown (a \"tech hit\")."});
    game.features.push_back({"dummy_getup", "Dummy", "Gets up",
                             "Where player 2 gets up after being knocked down: where it fell, or rolled towards player 1 or away.", 0,
                             {"Where it fell", "Towards", "Away"}});
}

} // namespace cps2recomp
