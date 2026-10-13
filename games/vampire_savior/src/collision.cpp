// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy
//
// Vampire Savior: the game's hit test, written again as C++ (rewrites: lift.h). The game's own code for these
// routines no longer runs; `--check-rewrites` runs both and compares.
//
// An attack hits when the attacker's attack box (named by the animation frame it is showing) overlaps one
// of the three boxes the victim can be hit in: head, body, legs. The test notes which parts were hit and
// where, and then the game's code for a hit that landed takes over.
#include "vsav.h"

#include <utility>

namespace cps2recomp {

namespace {
    using namespace vsav;

    // A box in the ROM: the middle's offsets from where the object stands, then half the width and height
    constexpr uint32_t kBoxX = 0, kBoxY = 2, kBoxHalfWidth = 4, kBoxHalfHeight = 6;

    // 019172. Do the box at A3 (belonging to the object at A6) and the box at A2 (of the object at A1)
    // overlap? The answer is in the flags, as the game's callers read it: "higher" means they do not. Also
    // left behind: the boxes' middles in the stage, and how far they reach into each other across.
    bool BoxesOverlap(M68K &c) {
        const uint32_t attacker = c.a[6], attackBox = c.a[3], victim = c.a[1], victimBox = c.a[2];

        // Across: a box's offset is the other way about when its object is turned
        uint16_t attackX = Mem16(c, attackBox + kBoxX), victimX = Mem16(c, victimBox + kBoxX);
        if (Mem8(c, attacker + kFacesRight)) {
            attackX = static_cast<uint16_t>(0u - attackX);
        }
        if (Mem8(c, victim + kFacesRight)) {
            victimX = static_cast<uint16_t>(0u - victimX);
        }
        attackX = static_cast<uint16_t>(attackX + Mem16(c, attacker + kX));
        victimX = static_cast<uint16_t>(victimX + Mem16(c, victim + kX));
        SetRam16(Var(c, kOverlapAttackX), attackX);
        SetRam16(Var(c, kOverlapVictimX), victimX);
        const uint16_t apartX = Size16(static_cast<uint16_t>(victimX - attackX));
        const uint16_t reachX = static_cast<uint16_t>(c.Add(Mem16(c, attackBox + kBoxHalfWidth), Mem16(c, victimBox + kBoxHalfWidth), 1, false, false));
        SetLow16(c.d[0], reachX);
        SetLow16(c.d[1], apartX);
        c.Sub(apartX, reachX, 1, false, false, false); // (the comparison the caller reads)
        if (apartX > reachX) {
            return false;
        }
        SetRam16(Var(c, kOverlapDepth), static_cast<uint16_t>(reachX - apartX));

        // Up and down
        const uint16_t attackY = static_cast<uint16_t>(Mem16(c, attackBox + kBoxY) + Mem16(c, attacker + kY));
        const uint16_t victimY = static_cast<uint16_t>(Mem16(c, victimBox + kBoxY) + Mem16(c, victim + kY));
        SetRam16(Var(c, kOverlapAttackY), attackY);
        SetRam16(Var(c, kOverlapVictimY), victimY);
        const uint16_t apartY = Size16(static_cast<uint16_t>(victimY - attackY));
        const uint16_t reachY = static_cast<uint16_t>(c.Add(Mem16(c, attackBox + kBoxHalfHeight), Mem16(c, victimBox + kBoxHalfHeight), 1, false, false));
        SetLow16(c.d[0], reachY);
        SetLow16(c.d[1], apartY);
        c.Sub(apartY, reachY, 1, false, false, false);
        return apartY <= reachY;
    }

    // 01923C. Nothing has been hit yet
    void ClearHit(M68K &c) {
        for (const int32_t distance : kHitDistance) {
            SetRam16(Var(c, distance), 0xFFFF);
        }
        SetRam8(Var(c, kHitParts), 0);
        for (const int32_t middles : kHitMiddles) {
            SetRam32(Var(c, middles), 0);
            SetRam32(Var(c, middles + 2), 0); // (six of the eight bytes: the victim's height is left)
        }
        c.d[0] = 0;
    }

    void AttackAgainst(M68K &c);

    // 017FF4. Does the attack of the object at A6 hit the object at A1? If it does, the game's code for a
    // hit that landed runs.
    void HitTest(M68K &c) {
        const uint32_t attacker = c.a[6], victim = c.a[1];
        SetRam16(Var(c, kHitAttacker), static_cast<uint16_t>(attacker));
        SetRam16(Var(c, kHitAttacker2), static_cast<uint16_t>(attacker));
        SetRam16(Var(c, kHitVictim), static_cast<uint16_t>(victim));
        SetRam16(Var(c, kHitVictim2), static_cast<uint16_t>(victim));
        ClearHit(c);

        // The attacker has to be there, and not held back from hitting
        if (!Mem8(c, attacker + kActive) || Mem8(c, attacker + 0x134) || Mem8(c, attacker + 0xB5)) {
            return;
        }
        // Some attacks only hit a victim in particular kinds of frame
        const uint8_t only = Mem8(c, attacker + 0xB3);
        c.a[0] = Mem32(c, victim + kFrame);
        const uint8_t victimKind = Mem8(c, c.a[0] + kFrameKind);
        if (only == 0) {
            if ((victimKind & 0x80) && victimKind != 0xFF) {
                return;
            }
        } else if (!(only & 0x80)) {
            if (victimKind != 0xFD && victimKind != 0xFE) {
                return;
            }
        } else {
            c.a[0] = c.a[0]; // (this way the game does not look at the victim's frame at all)
        }
        AttackAgainst(c);
    }

    // 018054. The rest of the hit test: the attack of the object at A6 against the object at A1
    void AttackAgainst(M68K &c) {
        const uint32_t attacker = c.a[6], victim = c.a[1];
        c.d[0] = 0;
        // The victim has to be there and able to be hit
        if (!Mem8(c, victim + kActive) || Mem8(c, victim + 0x134) || Mem8(c, victim + 0x147) || Mem8(c, victim + 0x11E)) {
            return;
        }
        c.a[0] = Mem32(c, victim + kFrame);
        if (Mem16(c, c.a[0] + kFrameCanBeHit) == 0) {
            return;
        }
        // The attacker's frame: an attack too weak for the state the victim is in does not hit
        const uint32_t frame = c.a[0] = Mem32(c, attacker + kFrame);
        if (!Mem8(c, attacker + 0xB4)) {
            const uint8_t strength = Mem8(c, frame + kFrameStrength), needed = Mem8(c, victim + 0x145);
            SetLow8(c.d[0], strength);
            if (strength < needed) {
                return;
            }
            if (needed != 0 && Mem8(c, victim + 0x1A4) == 0) {
                return;
            }
        }
        // The frame names its attack box (none: it is not an attack)
        const uint8_t attackBoxNumber = Mem8(c, frame + kFrameAttack);
        SetLow8(c.d[0], attackBoxNumber);
        if (!attackBoxNumber) {
            return;
        }
        SetLow16(c.d[0], static_cast<uint16_t>(c.d[0] << 5));
        const uint32_t attack = c.a[3] = Mem32(c, attacker + kAttackBoxes) + attackBoxNumber * kAttackBoxSize;
        // An attack hits a victim once: the victim remembers the number of the attack that hit it last
        const uint8_t attackNumber = Mem8(c, attack + kAttackNumber);
        SetLow8(c.d[0], attackNumber);
        if (!Mem8(c, attacker + 0x08)) {
            c.a[2] = victim + kHitBy;
            c.d[1] = Mem8(c, attacker + kSide);
            if (Mem8(c, victim + kHitBy + c.d[1]) == attackNumber) {
                return;
            }
        }
        if (Mem8(c, victim + 0x173)) {
            const uint8_t kind = Mem8(c, attack + kAttackKind);
            if (kind >= 0x0B && kind <= 0x0E) {
                return;
            }
        }
        // The victim's three boxes
        c.a[2] = Mem32(c, victim + kHurtBoxes);
        const uint32_t numbers = Mem32(c, c.a[2]);
        SetRam32(Var(c, kHitVictimBoxes), numbers);
        if (!numbers) {
            return;
        }
        static constexpr uint32_t kTables[3] = {kHeadBoxes, kBodyBoxes, kLegBoxes};
        for (int part = 0; part < 3; part++) {
            const uint8_t number = Ram8(Var(c, kHitVictimBoxes + part));
            c.d[0] = number;
            if (!number) {
                continue;
            }
            c.d[0] = number * 8u;
            c.a[2] = Mem32(c, victim + kTables[part]) + number * 8u;
            if (!BoxesOverlap(c)) {
                continue;
            }
            SetRam8(Var(c, kHitParts), static_cast<uint8_t>(Ram8(Var(c, kHitParts)) + (1u << part)));
            // Where: the two boxes' middles, and how far apart they are (across plus up and down)
            const uint16_t attackX = Ram16(Var(c, kOverlapAttackX)), victimX = Ram16(Var(c, kOverlapVictimX));
            const uint16_t attackY = Ram16(Var(c, kOverlapAttackY)), victimY = Ram16(Var(c, kOverlapVictimY));
            SetRam16(Var(c, kHitMiddles[part]), attackX);
            SetRam16(Var(c, kHitMiddles[part] + 4), victimX);
            const uint16_t apartX = Size16(static_cast<uint16_t>(attackX - victimX));
            SetRam16(Var(c, kHitDistance[part]), apartX);
            SetRam16(Var(c, kHitMiddles[part] + 2), attackY);
            SetRam16(Var(c, kHitMiddles[part] + 6), victimY);
            const uint16_t apartY = Size16(static_cast<uint16_t>(attackY - victimY));
            SetRam16(Var(c, kHitDistance[part]), static_cast<uint16_t>(apartX + apartY));
            SetLow16(c.d[0], apartY);
        }
        if (!Ram8(Var(c, kHitParts))) {
            return;
        }
        // It hit: the game's code for what happens next
        for (const uint32_t routine : {0x018230u, 0x0189BAu, 0x0188BAu, 0x018E8Au, 0x019030u, 0x01904Eu}) {
            Call(c, routine);
        }
    }
    // 017F22. Each of the four player records against the others its attacks can hit
    void PlayersAgainstPlayers(M68K &c) {
        for (int attacker = 0; attacker < kPlayerRecords; attacker++) {
            c.a[6] = c.a[5] + 0x400 * static_cast<uint32_t>(attacker + 1);
            for (int victim = 0; victim < kPlayerRecords; victim++) {
                // (the byte is read afresh each time: a hit can change it)
                if (victim != attacker && (Mem8(c, c.a[5] + 0x400 * static_cast<uint32_t>(attacker + 1) + kCanHit) & (1u << victim))) {
                    c.a[1] = c.a[5] + 0x400 * static_cast<uint32_t>(victim + 1);
                    HitTest(c);
                }
            }
        }
    }

    // 0193B8. Everything thrown that is in use against the two players (but not against the player whose
    // side it is on)
    void ThrownAgainstPlayers(M68K &c) {
        const uint8_t count = Ram8(Var(c, kThrownInUseCount));
        SetLow8(c.d[7], count);
        if (!count) {
            return;
        }
        SetLow16(c.d[7], static_cast<uint16_t>(static_cast<int8_t>(count - 1)));
        c.a[4] = c.a[5] + static_cast<uint32_t>(kThrownInUse);
        for (;;) {
            c.a[4] -= 2;
            c.a[6] = static_cast<uint32_t>(static_cast<int16_t>(Mem16(c, c.a[4])));
            SetRam32(Var(c, kThrownListAt), c.a[4]);
            if (Mem8(c, c.a[6] + kActive) == 1 && !Mem8(c, c.a[6] + 0xB5)) {
                // (its side: 0 is player 1's, 1 player 2's, anything else nobody's)
                for (int player = 0; player < 2; player++) {
                    if (Mem8(c, c.a[6] + kSide) == player) {
                        continue;
                    }
                    ClearHit(c);
                    c.a[1] = c.a[5] + (player == 0 ? 0x400u : 0x800u);
                    c.a[2] = c.a[5] + (player == 0 ? 0x800u : 0x400u);
                    SetRam16(Var(c, kHitAttacker), static_cast<uint16_t>(c.a[6]));
                    SetRam16(Var(c, kHitAttacker2), static_cast<uint16_t>(c.a[2]));
                    SetRam16(Var(c, kHitVictim), static_cast<uint16_t>(c.a[1]));
                    SetRam16(Var(c, kHitVictim2), static_cast<uint16_t>(c.a[1]));
                    // Most things do not hit a player in one particular kind of frame; some hit only then;
                    // and some do not care
                    const uint8_t only = Mem8(c, c.a[6] + 0xB3);
                    bool test = true;
                    if (!(only & 0x80)) {
                        c.a[0] = Mem32(c, c.a[1] + kFrame);
                        const bool thatKind = Mem8(c, c.a[0] + kFrameKind) == 0xFE;
                        test = only == 0 ? !thatKind : thatKind;
                    }
                    if (test) {
                        AttackAgainst(c);
                    }
                }
            }
            c.a[4] = Ram32(Var(c, kThrownListAt));
            const uint16_t left = static_cast<uint16_t>(c.d[7]);
            SetLow16(c.d[7], static_cast<uint16_t>(left - 1));
            if (left == 0) {
                break;
            }
        }
    }

    // 017F0C. A frame's hit tests
    void AllHitTests(M68K &c) {
        if (Ram8(Var(c, kNoHitTests))) {
            return;
        }
        Call(c, kThrownAgainstThrown);
        ThrownAgainstPlayers(c);
        PlayersAgainstPlayers(c);
    }

    // 01926A. The two players cannot stand in each other: when their pushing boxes overlap they are moved
    // apart, half the overlap each, or the whole of it for one when the other cannot be moved.
    void PushApart(M68K &c) {
        uint32_t first = c.a[5] + 0x400, second = c.a[5] + 0x800;
        SetRam8(first + kPushCount, 0);
        SetRam8(second + kPushCount, 0);
        if (Ram8(first + 0x148)) {
            SetRam8(first + 0x148, 0);
            SetRam8(second + 0x148, 0);
            std::swap(first, second);
        }
        c.a[6] = first;
        c.a[4] = second;
        const uint8_t firstNumber = Ram8(first + kPushBox), secondNumber = Ram8(second + kPushBox);
        if (!firstNumber || !secondNumber) {
            return;
        }
        const uint32_t firstBox = Ram32(first + kPushBoxes) + firstNumber * 8u, secondBox = Ram32(second + kPushBoxes) + secondNumber * 8u;
        // Up and down: no overlap, nothing to do
        const uint16_t apartY = Size16(static_cast<uint16_t>((Mem16(c, secondBox + kBoxY) + Ram16(second + kY)) -
                                                             (Mem16(c, firstBox + kBoxY) + Ram16(first + kY))));
        const uint16_t reachY = static_cast<uint16_t>(Mem16(c, firstBox + kBoxHalfHeight) + Mem16(c, secondBox + kBoxHalfHeight));
        if (!(static_cast<uint16_t>(apartY - reachY) & 0x8000)) {
            return;
        }
        // Across
        uint16_t firstX = Mem16(c, firstBox + kBoxX), secondX = Mem16(c, secondBox + kBoxX);
        if (Ram8(first + kFacesRight)) {
            firstX = static_cast<uint16_t>(0u - firstX);
        }
        if (Ram8(second + kFacesRight)) {
            secondX = static_cast<uint16_t>(0u - secondX);
        }
        const uint16_t between = static_cast<uint16_t>((secondX + Ram16(second + kX)) - (firstX + Ram16(first + kX)));
        const bool secondOnLeft = between & 0x8000; // (of the boxes' middles)
        const uint16_t reachX = static_cast<uint16_t>(Mem16(c, firstBox + kBoxHalfWidth) + Mem16(c, secondBox + kBoxHalfWidth));
        const int16_t gap = static_cast<int16_t>(Size16(between) - reachX);
        if (gap > 0) {
            return;
        }
        SetRam8(first + kPushCount, static_cast<uint8_t>(Ram8(first + kPushCount) + 1));
        SetRam8(second + kPushCount, static_cast<uint8_t>(Ram8(second + kPushCount) + 1));
        // How far they overlap, as a position's 16.16 number
        uint32_t overlap = static_cast<uint32_t>(static_cast<uint16_t>(-gap)) << 16;
        auto move = [&](uint32_t player, uint32_t by) { SetRam32(player + kX, Ram32(player + kX) + by); };
        const uint8_t firstPinned = Ram8(first + 0x116), secondPinned = Ram8(second + 0x116);
        auto moveSecondBy116 = [&]() { move(second, firstPinned == 1 ? overlap : 0u - overlap); };
        auto moveFirstBy116 = [&]() { move(first, secondPinned == 1 ? overlap : 0u - overlap); };
        if (firstPinned) {
            // The first is against a wall (which wall says which way the other goes)
            if (!secondPinned) {
                return moveSecondBy116();
            }
            if (Ram8(first + 0x115) || Ram8(first + 0x38)) {
                return moveFirstBy116();
            }
            if (Ram8(second + 0x115) || Ram8(second + 0x38)) {
                return moveSecondBy116();
            }
            return moveFirstBy116();
        }
        if (secondPinned) {
            return moveFirstBy116();
        }
        const bool firstHeld = Ram8(first + 0x38), secondHeld = Ram8(second + 0x38);
        if (firstHeld && !secondHeld) {
            // Only the first moves, away from the second
            return move(first, secondOnLeft ? overlap : 0u - overlap);
        }
        if (secondHeld && !firstHeld) {
            return move(second, secondOnLeft ? 0u - overlap : overlap);
        }
        // Half each, away from each other
        uint32_t half = overlap >> 1, firstBy = 0u - half, secondBy = half;
        if (secondOnLeft && !(Ram16(first + kX) < Ram16(second + kX))) {
            firstBy = half;
            secondBy = 0u - half;
        }
        move(first, firstBy);
        move(second, secondBy);
    }
} // namespace

void RegisterCollision() {
    AddRewrite(kBoxesOverlap, "BoxesOverlap", [](M68K &c) { BoxesOverlap(c); });
    AddRewrite(kClearHit, "ClearHit", ClearHit, kFreeFlags);
    constexpr uint32_t kHitTestFree = kFreeD0 | kFreeD1 | kFreeA0 | kFreeA2 | kFreeA3 | kFreeFlags;
    AddRewrite(kHitTest, "HitTest", HitTest, kHitTestFree);
    AddRewrite(kAttackAgainst, "AttackAgainst", AttackAgainst, kHitTestFree);
    AddRewrite(kPlayersAgainstPlayers, "PlayersAgainstPlayers", PlayersAgainstPlayers, kHitTestFree);
    AddRewrite(kThrownAgainstPlayers, "ThrownAgainstPlayers", ThrownAgainstPlayers, kHitTestFree);
    AddRewrite(kAllHitTests, "AllHitTests", AllHitTests, kHitTestFree);
    AddRewrite(kPushApart, "PushApart", PushApart, kFreeD0 | kFreeD1 | kFreeD2 | kFreeA0 | kFreeA1 | kFreeFlags);
}

} // namespace cps2recomp
