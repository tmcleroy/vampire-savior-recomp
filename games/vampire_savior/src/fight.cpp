// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy
//
// Vampire Savior: parts of a fight written again as C++ (rewrites: lift.h): a hit's damage coming off a
// player's health, a frame of the round (the clock, and seeing who has won), and the test for an attack being
// near enough to guard. The cheats that change damage and the clock are here, in the rules they change.
#include "vsav.h"

namespace cps2recomp {

void TrainingHit(int player, unsigned forGood, unsigned besides); // training.cpp
bool TrainingMode();
bool BattleNoClock(); // battle.cpp
void TrainingContact(int victim, bool guarded, uint8_t kind);

namespace {
    using namespace vsav;

    // 018AB0. Takes the damage of the hit being dealt off the health of the player at A1. A hit takes some
    // health for good and some besides that comes back by itself; A3 is the attack's entry.
    void TakeDamage(M68K &c) {
        const uint32_t victim = c.a[1] & 0xFFFFFF, attack = c.a[3];
        const int player = victim == Player(0) ? 0 : victim == Player(1) ? 1 : -1;
        uint16_t forGood = Ram16(Var(c, kDamageForGood)), besides = Ram16(Var(c, kDamageBesides));

        // (training and cheats)
        if (player >= 0) {
            if (forGood + besides) {
                TrainingHit(player, forGood, besides); // (a guarded hit that takes nothing is not a hit)
            }
            if (FeatureOn(player == 0 ? "no_damage" : "no_damage_2")) {
                forGood = besides = 0;
            } else if (player == 1 && FeatureOn("one_hit")) {
                forGood = static_cast<uint16_t>(Ram16(victim + kHealthCeiling) + 1); // (all there is and one more)
                besides = 0;
            }
        }

        SetRam16(Var(c, kHealthCeilingBefore), Ram16(victim + kHealthCeiling));
        SetRam16(Var(c, kHealthBefore), Ram16(victim + kHealth));
        SetRam16(victim + kHealthCeiling, static_cast<uint16_t>(Ram16(victim + kHealthCeiling) - forGood));
        const uint16_t all = static_cast<uint16_t>(forGood + besides);
        SetLow16(c.d[4], all);
        SetRam16(victim + kHealth, static_cast<uint16_t>(Ram16(victim + kHealth) - all));

        // Some attacks cannot knock out: they leave the health at the victim's floor (or, with no floor, at 0)
        auto floor = [&](uint32_t which) {
            c.d[0] = 0;
            const uint16_t now = Ram16(victim + which);
            if (!(now & 0x8000)) {
                c.d[0] = Ram16(victim + kHealthFloor);
                if (c.d[0] < now) {
                    return; // (above the floor still)
                }
            }
            SetRam16(victim + which, static_cast<uint16_t>(c.d[0] ? c.d[0] + 1 : 0));
        };
        if (Mem8(c, attack + kAttackFloorCeiling) & 0x80) {
            floor(kHealthCeiling);
        }
        if (Mem8(c, attack + kAttackFloorLeft) & 0x80) {
            floor(kHealth);
        }
        // (a training session: nobody is knocked out, so health stops at nothing left)
        if (TrainingMode()) {
            for (const uint32_t which : {kHealthCeiling, kHealth}) {
                if (Ram16(victim + which) & 0x8000) {
                    SetRam16(victim + which, 0);
                }
            }
        }
    }

    // How an attack must be guarded, by its kind (the byte kAttackKind of its entry)
    bool IsLow(uint8_t kind) { // (guarded crouching only)
        return kind == 0x02 || kind == 0x03 || kind == 0x38 || kind == 0x39;
    }
    bool IsOverhead(uint8_t kind) { // (guarded standing only)
        return kind == 0x4D || kind == 0x4A || kind == 0x2C || kind == 0x37 || kind == 0x42 || kind == 0x48;
    }
    bool IsJumpAttack(uint8_t kind) { // (guarded standing only when its player is in the air)
        return kind == 0x00 || kind == 0x01;
    }
    constexpr uint8_t kKindNoGuard = 0x2D;

    // 018230. An attack of the object at A6 (a player, or a thing thrown) has hit the player at A1; A3 is
    // the attack's entry. Sees whether the player guards it or stands through it, sets the player reeling,
    // and hands on to what a hit and a guarded hit do.
    void HitLands(M68K &c) {
        const uint32_t attacker = c.a[6], victim = c.a[1], attack = c.a[3];
        const bool byPlayer = Mem8(c, attacker + kIsThrown) == 0;
        const uint8_t kind = Mem8(c, attack + kAttackKind);
        auto set8 = [&](uint32_t address, uint8_t value) { c.Write(address, 0, value); };
        auto set16 = [&](uint32_t address, uint16_t value) { c.Write(address, 1, value); };
        auto reaction = [&](uint8_t which) { // 018968: which reaction the player goes into
            SetLow8(c.d[0], which);
            Call(c, 0x018968);
        };
        auto after = [&](bool landed) { // what follows, for a hit that landed and for a guarded one
            if (landed) {
                Call(c, 0x01841C);
            }
            Call(c, 0x01884E);
            Call(c, 0x018952);
            Call(c, 0x018980);
            const int player = victim == Player(0) ? 0 : victim == Player(1) ? 1 : -1;
            if (player >= 0) {
                TrainingContact(player, !landed, kind);
            }
        };

        set8(attacker + 0x39, 1);
        set8(Var(c, kHitWasGuarded), 0);
        if (!byPlayer) {
            // A thing thrown: its player is told, and it has one hit fewer left in it
            const uint32_t owner = c.a[0] = static_cast<uint32_t>(static_cast<int16_t>(Mem16(c, Var(c, kHitAttacker2))));
            set8(owner + 0xA0, 1);
            set8(victim + 0xA0, 0xFF);
            if (Mem8(c, owner + 0x178)) {
                set8(owner + 0x39, 1);
            }
            set16(attacker + 0x50, static_cast<uint16_t>(Mem16(c, attacker + 0x50) - 1));
        } else {
            set8(attacker + 0x169, 0x0E);
            set8(victim + 0xA0, 0);
            // (the attack's number is kept, so that it does not hit this player twice)
            c.d[0] = Mem8(c, attacker + kSide);
            c.a[0] = victim + kHitBy;
            set8(c.a[0] + c.d[0], Mem8(c, attack + kAttackNumber));
        }
        set8(victim + 0x56, Mem8(c, attack + 0x19));
        set8(victim + 0x5A, Mem8(c, attack + 0x0F));
        reaction(Mem8(c, attack + kAttackReaction));

        // A player with armour stands through hits until they have added up to too much
        bool armoured = false;
        if (!Mem8(c, victim + kUnshakeable) && Mem16(c, victim + kArmour)) {
            if (Mem8(c, victim + kArmourAlways)) {
                armoured = true;
            } else if (!Mem8(c, victim + kAirborne)) {
                SetLow8(c.d[0], Mem8(c, attack + kAttackArmourWeight));
                set8(victim + kArmourTaken, static_cast<uint8_t>(Mem8(c, victim + kArmourTaken) + Mem8(c, attack + kAttackArmourWeight)));
                set8(victim + kArmourTimer, 0xF0);
                SetLow8(c.d[0], Mem8(c, victim + kArmourMost));
                if (Mem8(c, victim + kArmourMost) >= Mem8(c, victim + kArmourTaken)) {
                    c.a[0] = Mem32(c, victim + kFrame);
                    armoured = static_cast<int8_t>(Mem8(c, c.a[0] + kFrameKind)) >= 0;
                }
            }
        }
        const bool crouching = Mem8(c, victim + kCrouching) != 0;
        const bool fromTheAir = byPlayer && Mem8(c, attacker + kAirborne) != 0;
        if (armoured) {
            // 0187BC. In a guarding pose it guards as anyone would (but for overheads, which it also guards);
            // otherwise the hit lands and the player does not reel
            c.a[0] = Mem32(c, victim + kFrame);
            const bool posed = static_cast<int8_t>(Mem8(c, c.a[0] + kFrameKind)) > 0;
            const bool guards = posed && (crouching ? !(fromTheAir && IsJumpAttack(kind)) : !(kind == 0x02 || kind == 0x03));
            if (guards) {
                c.Write(victim + kState, 2, kReeling);
                set8(victim + kGuarded, 0xFF);
                reaction(Mem8(c, attack + kAttackGuardReaction));
                return after(false);
            }
            set8(victim + kGuarded, 0);
            set16(victim + 0x164, 0);
            set8(victim + 0x163, 1);
            set8(victim + 0x14E, 0x18);
            set16(victim + 0x182, 4);
            Call(c, 0x01884E);
            Call(c, 0x0188BA);
            Call(c, 0x018952);
            Call(c, 0x018980);
            if (victim == Player(0) || victim == Player(1)) {
                TrainingContact(victim == Player(0) ? 0 : 1, false, kind);
            }
            return;
        }

        c.Write(victim + kState, 2, kReeling);
        set8(victim + kArmourTaken, 0);
        set8(victim + kArmourTimer, 0);
        // Guarded? Only from a guarding pose, and then by the attack's kind
        auto guarded = [&]() {
            if (kind == kKindNoGuard) {
                return false;
            }
            c.a[0] = Mem32(c, victim + kFrame);
            const int8_t pose = static_cast<int8_t>(Mem8(c, c.a[0] + kFrameKind));
            SetLow8(c.d[0], static_cast<uint8_t>(pose));
            if (pose <= 0) {
                return false;
            }
            if (Mem8(c, attacker + 0xAC) && Mem8(c, victim + kAirborne)) {
                return true;
            }
            if (pose == kGuardPoseAir) {
                // In the air: not what cannot be guarded there, and nothing a player does from the floor
                if (Mem8(c, attack + kAttackNoAirGuard) || (byPlayer && !Mem8(c, attacker + kAirborne))) {
                    return false;
                }
            }
            if (Mem8(c, victim + kAutoGuard) || (byPlayer && Mem8(c, attacker + 0x190))) {
                return true;
            }
            if (Mem8(c, Var(c, 0x85)) && (Mem8(c, Var(c, 0x7421)) & 0x20)) {
                return true;
            }
            if (!crouching) {
                return !IsLow(kind);
            }
            if (IsOverhead(kind)) {
                return false;
            }
            return !(fromTheAir && !Mem8(c, attacker + 0xAD) && IsJumpAttack(kind));
        };
        if (guarded() && !Mem8(c, victim + kUnshakeable)) {
            set8(Var(c, kHitWasGuarded), 1);
            set8(victim + kGuarded, 0xFF);
            reaction(Mem8(c, attack + kAttackGuardReaction));
            return after(false);
        }
        after(true);
    }

    // 0097DC. A frame of a round being fought: sees whether someone is knocked out, runs the clock, and when
    // the round is decided hands on to what comes after.
    void RoundFighting(M68K &c) {
        if (Ram8(Var(c, kRoundPaused))) {
            SetRam8(Var(c, 0x12D), 0);
            SetRam8(Var(c, 0xBD), 0xFF);
            SetRam8(Var(c, 0x10E), 0xFF);
            Call(c, 0x00995A);
            return;
        }
        Call(c, 0x00A60A);
        const bool training = TrainingMode();
        if (training) {
            // (whatever else takes health off: not below nothing)
            for (int n = 0; n < 2; n++) {
                if (Ram16(Player(n) + kHealth) & 0x8000) {
                    SetRam16(Player(n) + kHealth, 0);
                }
            }
        }
        // Knocked out: health below zero
        uint8_t result = 0;
        if (Ram16(Player(1) + kHealth) & 0x8000) {
            result |= 1;
        }
        if (Ram16(Player(0) + kHealth) & 0x8000) {
            result |= 2;
        }
        c.d[0] = result;
        SetRam8(Var(c, kRoundResult), result);
        if (!result && !Ram8(Var(c, kRoundClockStopped))) {
            // The clock: a second is so many frames (by the game's speed setting)
            const uint8_t part = static_cast<uint8_t>(Ram8(Var(c, kRoundClockPart)) - 1);
            SetRam8(Var(c, kRoundClockPart), part);
            if (part == 0) {
                Call(c, 0x00A3EA); // (the frames of the next second)
                if (!FeatureOn("infinite_time") && !training && !BattleNoClock()) {
                    const uint8_t seconds = static_cast<uint8_t>(Ram8(Var(c, kRoundClock)) - 1);
                    SetRam8(Var(c, kRoundClock), seconds);
                    if (seconds & 0x80) {
                        // Time is up: whoever has more health left has won
                        SetRam8(Var(c, kRoundClock), 0);
                        SetRam8(Var(c, kRoundTimeOver), 0xFF);
                        SetRam8(Var(c, 0x14C), 1);
                        Call(c, 0x00A3B2);
                    }
                }
            }
        }
        result = Ram8(Var(c, kRoundResult));
        SetLow8(c.d[0], result);
        if (!result) {
            return;
        }
        // Decided: on to the end of the round
        Call(c, 0x009880);
        Call(c, 0x0099A8);
        SetRam16(Var(c, 0x0C), static_cast<uint16_t>(Ram16(Var(c, 0x0C)) + 2));
        c.d[0] = 0;
        SetRam32(Var(c, 0x10), 0);
        SetRam8(Var(c, 0x8F), 1);
        SetRam8(Var(c, 0xBD), 0xFF);
        SetRam8(Var(c, 0x107), 1);
        SetRam8(Var(c, 0x10E), 1);
        SetRam8(Var(c, 0x12D), 0);
        SetRam8(Var(c, 0x125), 0);
        Call(c, 0x00995A);
        Call(c, 0x00D560);
    }

    // 0276A6. Is something of the other side's near enough to the player at A6 that it should guard? Yes
    // (D0 = 1) if anything the other side has thrown is about, or if the other player's attack, in this frame
    // or one of the frames to come, reaches to within half a screen.
    void GuardRangeAttack(M68K &c) {
        const uint32_t player = c.a[6];
        auto answer = [&](bool yes) {
            c.d[0] = yes ? 1 : 0;
            c.flagN = c.flagV = c.flagC = false; // (as the game's last instruction leaves the flags)
            c.flagZ = !yes;
        };
        // Things thrown by the other side
        c.d[6] = kThrownRecords - 1;
        c.a[4] = c.a[5] + 0x1400;
        for (int n = 0; n < kThrownRecords; n++, c.a[4] += kThrownSize) {
            const uint32_t thrown = c.a[4];
            if (Mem8(c, thrown + kActive) == 1 && Mem8(c, thrown + 1) && !Mem8(c, thrown + 0xA2)) {
                SetLow8(c.d[0], Mem8(c, player + kSide));
                if (Mem8(c, player + kSide) != Mem8(c, thrown + kSide)) {
                    c.a[1] = Mem32(c, thrown + kFrame);
                    if (!(Mem8(c, c.a[1] + kFrameHarmless) & 0x80)) {
                        SetLow16(c.d[6], static_cast<uint16_t>(kThrownRecords - 1 - n));
                        return answer(true);
                    }
                }
            }
        }
        SetLow16(c.d[6], 0xFFFF);
        // The other player
        const uint32_t other = c.a[4] = static_cast<uint32_t>(static_cast<int16_t>(Mem16(c, player + kOpponent)));
        if (Mem8(c, other + 0x154)) {
            return answer(true);
        }
        if (!Mem8(c, other + kAttacking)) {
            return answer(false);
        }
        if (!Mem8(c, other + 0x38) && !Mem8(c, other + 0xAC) && Mem8(c, player + 0x38)) {
            return answer(false);
        }
        c.a[0] = Mem32(c, other + kFrame);
        if (Mem8(c, c.a[0] + kFrameHarmless) & 0x80) {
            return answer(false);
        }
        // The attack boxes of this frame and of those that follow it in the move
        for (;;) {
            c.a[1] = Mem32(c, other + kAttackBoxes);
            const uint8_t number = Mem8(c, c.a[0] + kFrameAttack);
            SetLow8(c.d[0], number);
            if (number) {
                const uint16_t index = static_cast<uint16_t>(static_cast<int8_t>(number) * 32);
                c.a[1] += static_cast<uint32_t>(static_cast<int16_t>(index));
                uint16_t x = Mem16(c, c.a[1]);
                if (Mem8(c, other + kFacesRight)) {
                    x = static_cast<uint16_t>(0u - x);
                }
                const uint16_t apart = Size16(static_cast<uint16_t>(x + Mem16(c, other + kX) - Mem16(c, player + kX)));
                const uint16_t reach = static_cast<uint16_t>(Mem16(c, c.a[1] + 4) + 0x80);
                SetLow16(c.d[0], apart);
                SetLow16(c.d[1], reach);
                if (apart <= reach) {
                    return answer(true);
                }
            }
            const uint16_t end = Mem8(c, c.a[0] + kFrameEnd) & 0xC0;
            SetLow16(c.d[0], end);
            if (end) {
                break;
            }
            c.a[0] += kFrameSize;
        }
        // No attack box to come: near enough anyway?
        const uint16_t apart = static_cast<uint16_t>(Mem16(c, player + kX) - Mem16(c, other + kX) + 0x80);
        return answer(apart < 0x100);
    }

    // 027694. The same for a player who has to be holding away to guard (or has chosen to guard by itself)
    void GuardRange(M68K &c) {
        const uint32_t player = c.a[6];
        if (!Mem8(c, player + kAutoGuard) && !(Mem8(c, player + kHeld) & 1)) {
            c.d[0] = 0;
            c.flagN = c.flagV = c.flagC = false;
            c.flagZ = true;
            return;
        }
        GuardRangeAttack(c);
    }
} // namespace

// For the training dummy: would the player whose record is at `player` be in range to guard now?
bool AttackInRange(M68K &c, uint32_t player) {
    // (the game's test, with the registers it changes kept and put back)
    M68K::Kept kept = c.Keep();
    c.a[5] = 0xFF8000;
    c.a[6] = player;
    GuardRangeAttack(c);
    const bool yes = c.d[0] != 0;
    c.PutBack(kept);
    return yes;
}

// For the training dummy: how what is coming at the player at `player` has to be guarded: standing (true)
// or crouching. Standing for overheads and for a player attacking from the air; crouching for everything
// else (which guards low attacks, and all that can be guarded either way).
bool GuardStanding(M68K &c, uint32_t player) {
    const uint32_t other = 0xFF0000u | Mem16(c, player + kOpponent);
    auto standing = [&](uint32_t object, bool fromTheAir) {
        // The attack boxes of this frame and of those that follow it in the move
        uint32_t frame = Mem32(c, object + kFrame);
        for (int n = 0; n < 64; n++, frame += kFrameSize) {
            if (const uint8_t number = Mem8(c, frame + kFrameAttack)) {
                const uint8_t kind = Mem8(c, Mem32(c, object + kAttackBoxes) + number * kAttackBoxSize + kAttackKind);
                return IsOverhead(kind) || (fromTheAir && IsJumpAttack(kind));
            }
            if (Mem8(c, frame + kFrameEnd) & 0xC0) {
                break;
            }
        }
        return fromTheAir;
    };
    if (Mem8(c, other + kAttacking)) {
        return standing(other, Mem8(c, other + kAirborne) != 0);
    }
    for (int n = 0; n < kThrownRecords; n++) {
        const uint32_t thrown = kThrown + static_cast<uint32_t>(n) * kThrownSize;
        if (Mem8(c, thrown + kActive) == 1 && Mem8(c, thrown + kSide) != Mem8(c, player + kSide) && standing(thrown, false)) {
            return true;
        }
    }
    return false;
}

void RegisterFight() {
    AddRewrite(kTakeDamage, "TakeDamage", TakeDamage, kFreeFlags);
    AddRewrite(kRoundFighting, "RoundFighting", RoundFighting, kFreeFlags);
    AddRewrite(kHitLands, "HitLands", HitLands, kFreeFlags);
    AddRewrite(kGuardRange, "GuardRange", GuardRange, kFreeFlags);
    AddRewrite(kGuardRangeAttack, "GuardRangeAttack", GuardRangeAttack, kFreeFlags);
}

} // namespace cps2recomp
