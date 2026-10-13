// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy
//
// Vampire Savior: frame data. Watches what the two players are doing at every step of the game (not every
// picture: at the faster speed settings the game takes more than one step in some pictures) and from that
// tells, for each move: how many steps before it can hit (startup: the number of the first step it can hit
// on), how many it can hit for (active), and how many after that before the player can do anything else
// (recovery); and after a hit or a guarded hit, who can act first and by how many steps (the advantage).
// A meter along the bottom shows the same, a cell a step.
#include "frames.h"

#include "state.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace cps2recomp {

namespace {
    using namespace vsav;

    constexpr int kMeterCells = 60;      // steps the meter shows
    constexpr int kMeterIdle = 30;       // steps with both players free after which the meter starts afresh
    constexpr int kShownFor = 300;       // steps a move's numbers stay up

    struct Track {
        Doing doing = Doing::Free;
        bool inMove = false, hadActive = false, frozen = false;
        uint16_t total = 0, startup = 0, active = 0, lastActiveAt = 0; // of the move being done
        uint16_t shownStartup = 0, shownActive = 0, shownRecovery = 0; // of the last move finished
        uint32_t shownAt = 0;                                          // (the step; 0: nothing to show)
        uint32_t freeAt = 0;   // the step at which the player last became free to act
        bool guarded = false;  // the hit it is reeling from was guarded
        bool wasHit = false;   // a hit has landed on it (or been guarded) since it was last free
    };
    struct State {
        Track track[2];
        uint32_t step = 0;
        // A player has got over a hit and the other's move is not over yet: the advantage is known when it is
        bool pending = false;
        int pendingVictim = 0;
        uint32_t pendingAt = 0;
        bool pendingGuarded = false;
        // The last advantage worked out, for the player who attacked
        int advantageFor = -1;
        int32_t advantage = 0;
        bool advantageGuarded = false;
        uint32_t advantageAt = 0;
        // The meter: what each player was doing at each of the last steps (oldest first)
        uint8_t meter[2][kMeterCells] = {};
        int meterLength = 0, idle = 0;
    } g;

    bool AttackOut(M68K &c, uint32_t player) {
        const uint32_t frame = Mem32(c, player + kFrame);
        return Mem8(c, frame + kFrameAttack) != 0 && !(Mem8(c, frame + kFrameHarmless) & 0x80);
    }

    void BothStepped();

    // What a player was doing at the step before this one. (Seen as the player's own routine starts on this
    // step: its record is then as the last step left it, which is what the picture showed and the hit tests
    // tested; and whether the last step was a frozen one was to be seen as that step started.)
    Doing Classify(M68K &c, int n) {
        const uint32_t player = Player(n);
        Track &t = g.track[n];
        const uint8_t control = Ram8(player + kControl), action = Ram8(player + kAction);
        // Frozen by a hit landing: such steps do not count (some animation frames go on through it: those do)
        const bool wasFrozen = t.frozen;
        t.frozen = control == kControlOwn && Ram8(player + kHitStop) != 0 && !Mem8(c, Mem32(c, player + kFrame) + kFrameThroughStop);
        if (control != kControlOwn) {
            t.inMove = false; // (a move hit out of is not reported)
            if (control != kControlReeling) {
                return Doing::Held;
            }
            return t.guarded ? Doing::GuardStun : Doing::HitStun;
        }
        if (CanAct(n)) {
            if (t.inMove) {
                // The move is over
                t.inMove = false;
                if (t.hadActive) {
                    t.shownStartup = t.startup;
                    t.shownActive = t.active;
                    t.shownRecovery = static_cast<uint16_t>(t.total - t.lastActiveAt);
                    t.shownAt = g.step;
                }
            }
            return Doing::Free;
        }
        const bool attacking = Ram8(player + kAttacking) != 0;
        if (action == kActionJump && !attacking && !t.inMove) {
            return Doing::Air;
        }
        if (!t.inMove) {
            t.inMove = true;
            t.hadActive = false;
            t.total = t.startup = t.active = t.lastActiveAt = 0;
        } else if (wasFrozen) {
            return t.doing;
        }
        t.total++;
        if (AttackOut(c, player)) {
            if (!t.hadActive) {
                t.hadActive = true;
                t.startup = t.total;
            }
            t.active++;
            t.lastActiveAt = t.total;
            return Doing::Active;
        }
        return t.hadActive ? Doing::Recovery : Doing::Startup;
    }

    // 02244C: a player's routine, where it goes on to whatever is in charge of the player; once a step for
    // each player
    bool PlayerStep(M68K &c) {
        const uint32_t player = c.a[6] & 0xFFFFFF;
        const int n = player == Player(0) ? 0 : player == Player(1) ? 1 : -1;
        if (n < 0 || Ram8(player + kState) != 2) {
            return false;
        }
        if (n == 0) {
            g.step++;
        }
        Track &t = g.track[n];
        const Doing before = t.doing;
        t.doing = Classify(c, n);
        if (t.doing == Doing::Free && before != Doing::Free && t.shownAt == g.step && getenv("VSAV_FRAMES")) {
            // (for tests: every move's numbers as it ends)
            fprintf(stderr, "frames: player %d startup %u active %u recovery %u\n", n + 1, t.shownStartup, t.shownActive, t.shownRecovery);
        }
        if (t.doing == Doing::Free && before != Doing::Free) {
            t.freeAt = g.step;
            const bool wasHit = t.wasHit;
            t.wasHit = false;
            if (wasHit) {
                // Over a hit: the advantage is the other player's, once that one is free too
                g.pending = true;
                g.pendingVictim = n;
                g.pendingAt = g.step;
                g.pendingGuarded = before == Doing::GuardStun;
            }
        }
        if (n == 1) {
            BothStepped();
        }
        return false;
    }

    // With both players looked at: the advantage, and the meter
    void BothStepped() {
        const Doing doing[2] = {g.track[0].doing, g.track[1].doing};
        if (g.pending) {
            const int attacker = 1 - g.pendingVictim;
            if (doing[g.pendingVictim] != Doing::Free) {
                g.pending = false; // (hit again before the other was free)
            } else if (doing[attacker] == Doing::Free) {
                g.pending = false;
                g.advantageFor = attacker;
                g.advantage = static_cast<int32_t>(g.pendingAt) - static_cast<int32_t>(g.track[attacker].freeAt);
                g.advantageGuarded = g.pendingGuarded;
                g.advantageAt = g.step;
                if (getenv("VSAV_FRAMES")) {
                    fprintf(stderr, "frames: player %d %+d on %s\n", attacker + 1, static_cast<int>(g.advantage), g.advantageGuarded ? "guard" : "hit");
                }
            }
        }
        // The meter stands still while both players are free, and starts afresh after a while of that
        if (doing[0] == Doing::Free && doing[1] == Doing::Free) {
            if (g.idle < kMeterIdle) {
                g.idle++;
            }
            return;
        }
        if (g.idle >= kMeterIdle) {
            g.meterLength = 0;
        }
        g.idle = 0;
        if (g.meterLength == kMeterCells) {
            for (int n = 0; n < 2; n++) {
                memmove(g.meter[n], g.meter[n] + 1, kMeterCells - 1);
            }
            g.meterLength--;
        }
        for (int n = 0; n < 2; n++) {
            g.meter[n][g.meterLength] = static_cast<uint8_t>(doing[n]);
        }
        g.meterLength++;
    }
} // namespace

void RegisterFrames() {
    AddHook(0x02244C, "PlayerStep", PlayerStep);
}

void FramesContact(int victim, bool guarded) {
    g.track[victim].guarded = guarded;
    g.track[victim].wasHit = true;
}

Doing FramesDoing(int player) {
    return g.track[player].doing;
}

void FramesReset() {
    g = State();
}

void FramesState(Archive &ar) {
    ar.Mark(0x4652414D);
    ar.Pod(g);
}

void FramesHud(bool numbers, bool meter, std::vector<HudLine> &lines, std::vector<HudBox> &boxes) {
    if (numbers) {
        for (int n = 0; n < 2; n++) {
            const Track &t = g.track[n];
            const float x = n == 0 ? 0.03f : 0.97f;
            const int align = n == 0 ? -1 : 1;
            char text[64];
            if (t.shownAt && g.step - t.shownAt < kShownFor) {
                snprintf(text, sizeof(text), "startup %u  active %u  recovery %u", t.shownStartup, t.shownActive, t.shownRecovery);
                lines.push_back({x, 0.785f, align, text});
            }
            if (g.advantageFor == n && g.step - g.advantageAt < kShownFor) {
                snprintf(text, sizeof(text), "%+d on %s", static_cast<int>(g.advantage), g.advantageGuarded ? "guard" : "hit");
                lines.push_back({x, 0.72f, align, text});
            }
        }
    }
    if (meter && g.meterLength) {
        // Two rows of cells above the gauges, player 1's on top
        constexpr int kCell = 6, kLeft = (384 - kMeterCells * kCell) / 2, kTop = 193;
        static constexpr struct {
            uint8_t r, g, b;
        } kColors[] = {{40, 44, 56},    // free
                       {60, 200, 110},  // startup
                       {240, 60, 60},   // active
                       {70, 120, 250},  // recovery
                       {250, 210, 60},  // reeling from a hit
                       {90, 220, 230},  // ... from a guarded one
                       {150, 150, 160}, // in the air
                       {200, 110, 230}}; // held
        boxes.push_back({kLeft - 2, kTop - 2, kMeterCells * kCell + 3, 2 * kCell + 3, 10, 12, 18, 200});
        for (int n = 0; n < 2; n++) {
            for (int i = 0; i < g.meterLength; i++) {
                const auto &color = kColors[g.meter[n][i]];
                boxes.push_back({kLeft + i * kCell, kTop + n * kCell, kCell - 1, kCell - 1, color.r, color.g, color.b, 255});
            }
        }
    }
}

} // namespace cps2recomp
