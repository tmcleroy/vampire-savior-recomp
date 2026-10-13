// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy
//
// Vampire Savior: the picture's edges for the players, written again as C++ (rewrites: lift.h). A stage has
// no walls: what stops a player is the edge of the picture, and the camera only goes so far (camera.cpp).
// Each frame a player beyond an edge is put back at it, and when that player is being knocked back by a
// hit, the attacker standing near is pushed away by as much instead (the corner's push back).
//
// In a wide picture (widescreen.h) the edges are the wide one's, 64 further out on each side. In a ring
// out fight (battle.cpp) a player who is not its own master is not stopped at the stage's ends.
#include "modes.h"
#include "stage.h"
#include "widescreen.h"

namespace cps2recomp {

bool RingOutLetsOut(uint32_t player); // battle.cpp

namespace {
    using namespace vsav;

    constexpr int kPictureAcross = 0x180;
    // More of a player's record
    constexpr uint32_t kAtEdge = 0x116;         // byte: 1 at the picture's left edge this frame, 2 at its right
    constexpr uint32_t kNoEdges = 0x150;        // byte: not zero: the player is not kept in the picture
    constexpr uint32_t kKnockedBack = 0x0202;   // the word at kState while a hit is knocking the player back
    constexpr uint32_t kNoPushBack = 0x186;     // byte (either player's): not zero: no push back at the corner
    constexpr uint32_t kPushedAnyway[3] = {0x1A2, 0x193, 0x17B}; // bytes of the attacker's: any not zero: pushed back whatever follows
    constexpr uint32_t kHitFromAfar = 0xA0;     // byte: bit 7: the hit was not the attacker's own body's (nothing to push back)
    // How near the attacker has to be to be pushed back, and when neither player has health below nothing
    constexpr uint16_t kPushBackWithin = 0x100, kPushBackWithinAlive = 0x150;

    uint32_t Other(uint32_t player) {
        return 0xFFFF0000u | Ram16((player & 0xFFFFFF) + kOpponent); // (as the game has it in a register)
    }
    void Move(uint32_t player, uint32_t by) {
        SetRam16((player & 0xFFFFFF) + kX, static_cast<uint16_t>(Ram16((player & 0xFFFFFF) + kX) - by));
    }
    // The scroll screen the players stand in is at A3 (where its left edge is shown is the picture's)
    uint16_t PictureLeft(M68K &c) {
        return Mem16(c, c.a[3] + kLayerShownX);
    }

    // 028192. The player at A6 is kept inside the picture
    void KeepIn(M68K &c) {
        const uint32_t player = c.a[6] & 0xFFFFFF;
        if (RingOutLetsOut(player)) {
            SetRam8(player + kAtEdge, 0);
            return;
        }
        if (Ram8(player + kNotFollowed)) {
            return;
        }
        SetRam8(player + kAtEdge, 0);
        if (Ram8(player + kNoEdges)) {
            return;
        }
        c.d[0] = Ram8(player + kIsRiding);
        c.a[3] = kLayerRecords[(c.d[0] >> 2) & 3];
        if (!Mem8(c, c.a[3])) {
            return;
        }
        c.a[4] = Other(player);
        const uint32_t other = c.a[4] & 0xFFFFFF;
        const bool alive = !(Ram16(player + kHealthCeiling) & 0x8000) && !(Ram16(other + kHealthCeiling) & 0x8000);
        SetLow16(c.d[6], alive ? kPushBackWithinAlive : kPushBackWithin);
        SetLow16(c.d[1], Size16(static_cast<uint16_t>(Ram16(other + kX) - Ram16(player + kX))));

        const int side = WideMargin(); // (the wide picture's edges are that much further out: widescreen.h)
        const uint16_t half = Ram16(player + kHalfWidth), left = static_cast<uint16_t>(PictureLeft(c) - side);
        SetLow16(c.d[2], half);
        // (how far the player's left edge is inside the picture's: not above nothing, and it is put back)
        SetLow16(c.d[0], Ram16(player + kX) - half - left);
        if (static_cast<uint16_t>(Ram16(player + kX) - half) <= left) {
            Move(player, c.d[0]);
            SetRam8(player + kAtEdge, 1);
        } else {
            // (and how far its right edge is beyond the picture's)
            SetLow16(c.d[2], half * 2);
            SetLow16(c.d[0], c.d[0] + half * 2 - kPictureAcross - 2 * side);
            if (c.d[0] & 0x8000) {
                return;
            }
            Move(player, c.d[0]);
            SetRam8(player + kAtEdge, 2);
        }

        // The corner's push back: the player is being knocked back on the floor, and the attacker is near
        if (Ram16(player + kState) != kKnockedBack || (c.d[1] & 0xFFFF) >= (c.d[6] & 0xFFFF)) {
            return;
        }
        if (Ram8(player + kAirborne) || Ram8(player + kNoPushBack) || Ram8(other + kNoPushBack)) {
            return;
        }
        if (!Ram8(other + kPushedAnyway[0]) && !Ram8(other + kPushedAnyway[1]) && !Ram8(other + kPushedAnyway[2])) {
            if (Ram8(other + kAirborne) || (Ram8(player + kHitFromAfar) & 0x80)) {
                return;
            }
        }
        Move(other, c.d[0]);
    }

    // 02824C. The player at A6 and the one it holds are kept inside the picture together
    void KeepBothIn(M68K &c) {
        const uint32_t player = c.a[6] & 0xFFFFFF;
        c.a[4] = Other(player);
        const uint32_t other = c.a[4] & 0xFFFFFF;
        SetRam8(other + kAtEdge, 0);
        if (Ram8(other + kNoEdges)) {
            return;
        }
        SetLow16(c.d[1], Ram16(player + kX));
        const bool playerLeft = Ram16(player + kX) < Ram16(other + kX);
        c.a[0] = playerLeft ? c.a[6] : c.a[4];
        c.a[1] = playerLeft ? c.a[4] : c.a[6];
        const uint32_t leftOne = c.a[0] & 0xFFFFFF, rightOne = c.a[1] & 0xFFFFFF;
        c.d[0] = Ram8(player + kIsRiding);
        c.a[3] = kLayerRecords[(c.d[0] >> 2) & 3];

        const int side = WideMargin();
        const uint16_t left = static_cast<uint16_t>(PictureLeft(c) - side);
        SetLow16(c.d[2], Ram16(leftOne + kHalfWidth));
        SetLow16(c.d[0], Ram16(leftOne + kX) - c.d[2] - left);
        if (static_cast<uint16_t>(Ram16(leftOne + kX) - c.d[2]) <= left) {
            Move(player, c.d[0]);
            Move(other, c.d[0]);
            SetRam8(other + kAtEdge, Ram8(other + kAtEdge) | 1);
        }
        SetLow16(c.d[2], Ram16(rightOne + kHalfWidth));
        SetLow16(c.d[0], Ram16(rightOne + kX) - left + c.d[2] - kPictureAcross - 2 * side);
        if (c.d[0] & 0x8000) {
            return;
        }
        Move(player, c.d[0]);
        Move(other, c.d[0]);
        SetRam8(other + kAtEdge, Ram8(other + kAtEdge) | 2);
    }
} // namespace

void RegisterEdges() {
    // (the last number is the time a call is charged: the game's own, as the check measured it)
    AddRewrite(0x028192, "KeepIn", KeepIn, kFreeFlags, 338);
    AddRewrite(0x02824C, "KeepBothIn", KeepBothIn, kFreeFlags, 104);
}

} // namespace cps2recomp
