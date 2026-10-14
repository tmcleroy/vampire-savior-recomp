// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy
//
// Vampire Savior: frame data. Each player is looked at once for every step of the game (not every picture: at the
// faster speed settings the game takes more than one step in some pictures), where its routine dispatches, and
// what it is doing is told to the toolkit's frame counting (fight.h): startup, active and recovery of each move,
// the advantage after a hit or a guarded one, and a meter of the last sixty steps.
#include "frames.h"

#include "state.h"
#include "widescreen.h"

#include <cstdlib>

namespace cps2recomp {

namespace {
    using namespace vsav;

    fight::Frames g_frames;

    bool AttackOut(M68K &c, uint32_t player) {
        const uint32_t frame = Mem32(c, player + kFrame);
        return Mem8(c, frame + kFrameAttack) != 0 && !(Mem8(c, frame + kFrameHarmless) & 0x80);
    }

    // 02244C: a player's routine, where it goes on to whatever is in charge of the player; once a step for each
    // player. (Seen as the player's own routine starts on this step: its record is then as the last step left it,
    // which is what the picture showed and the hit tests tested.)
    bool PlayerStep(M68K &c) {
        const uint32_t player = c.a[6] & 0xFFFFFF;
        const int n = player == Player(0) ? 0 : player == Player(1) ? 1 : -1;
        if (n < 0 || Ram8(player + kState) != 2) {
            return false;
        }
        fight::StepView view;
        const uint8_t control = Ram8(player + kControl);
        view.control = control == kControlOwn ? fight::StepView::Own : control == kControlReeling ? fight::StepView::Reeling : fight::StepView::Held;
        // (frozen by a hit landing; some animation frames go on through it)
        view.frozen = Ram8(player + kHitStop) != 0 && !Mem8(c, Mem32(c, player + kFrame) + kFrameThroughStop);
        view.canAct = CanAct(n);
        view.attacking = Ram8(player + kAttacking) != 0;
        view.jumping = Ram8(player + kAction) == kActionJump;
        view.attackOut = AttackOut(c, player);
        g_frames.Step(n, view);
        return false;
    }
} // namespace

void RegisterFrames() {
    g_frames.print = getenv("VSAV_FRAMES") != nullptr;
    AddHook(0x02244C, "PlayerStep", PlayerStep);
}

void FramesContact(int victim, bool guarded) {
    g_frames.Contact(victim, guarded);
}

Doing FramesDoing(int player) {
    return g_frames.Now(player);
}

void FramesReset() {
    g_frames.Reset();
}

void FramesState(Archive &ar) {
    g_frames.Serialize(ar);
}

void FramesHud(bool numbers, bool meter, std::vector<HudLine> &lines, std::vector<HudBox> &boxes) {
    // (in the middle of a wide picture too)
    g_frames.Hud(numbers, meter, lines, boxes, 0.72f, 193, Video::kWidth, WideSide());
}

} // namespace cps2recomp
