// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy
//
// Vampire Savior: the character choice, where the hidden characters' codes are read, written again as C++ (a
// rewrite: lift.h), and the "Hidden characters" feature, which gives them without their codes.
//
// What the board hides, as its code has it:
// - Dark Gallon (character 0x12, Gallon's number with bit 4): Gallon chosen while Start is held, with two or
//   three punches (his first colours) or kicks (his second) pressed together. Only these two sets of colours.
// - Oboro Bishamon (0x18): the last opponent of every one-player game, reached when it has gone well enough
//   (00A738). No code chooses him on this set; the ladder's tables (00AF16) have a row for him as a player,
//   as for every number from 0x10 up.
// - Shadow: not a character but a way to play. Start pressed exactly five times on the "?" box, then a
//   button: the player begins as the character the box was showing (or the one Start was pressed on three
//   times before, if any), and after each match won becomes the opponent just beaten (009BB2).
#include "modes.h"
#include "vsav.h"

namespace cps2recomp {

namespace {
    using namespace vsav;

    // In a player's record while choosing (bytes but for the controls):
    constexpr uint32_t kNumber = 0x381;        // 0 player 1, 1 player 2
    constexpr uint32_t kCursor = 0x03;         // the character under the cursor (kRandomBox: the "?" box)
    constexpr uint32_t kHeldNow = 0x394;       // word: what the player holds (bit 15 Start, 8-10 punches, 12-14 kicks)
    constexpr uint32_t kStartsOnBox = 0x42;    // Start pressed on the "?" box, one after another
    constexpr uint32_t kShadowCode = 0x43;     // not zero: those were five
    constexpr uint32_t kShadow = 0x3BC;        // not zero: the player is Shadow (from kShadowCode, when it chooses)
    constexpr uint8_t kRandomBox = 0x0B;
    constexpr int kShadowStarts = 5;
    // From A5: the codes are not read when this is set (nothing in this set of the game sets it)
    constexpr int32_t kNoCodes = 0xBF;
    // From A5: both players are in (kPlayersIn, and who was playing then) and bit n here: player n has chosen
    constexpr int32_t kPlayersInNow = 0x8D, kPlayersThenVar = 0xAC, kHaveChosen = 0x131;

    constexpr uint8_t kGallon = 0x02, kBishamon = 0x08, kOboro = kOboroBishamon; // (kDarkGallon: vsav.h)
    // Buttons, in the word the player holds
    constexpr uint16_t kStartHeld = 0x8000, kPunches = 0x0700, kKicks = 0x7000;
    constexpr uint16_t kButtonColours[6] = {0x0100, 0x0200, 0x0400, 0x1000, 0x2000, 0x4000}; // colours 0-5

    // "Hidden characters" for this player (0 player 1, 1 player 2)
    bool HiddenFor(int n) {
        const int who = FeatureValue("hidden_characters"); // 1 player 1, 2 player 2, 3 both
        return (who & (1 << n)) != 0;
    }

    // 020C18 and 020C38: two or three punches (or kicks) pressed together, and nothing else
    bool TwoPunches(uint16_t pressed) {
        return pressed == 0x0300 || pressed == 0x0600 || pressed == 0x0500 || pressed == 0x0700;
    }
    bool TwoKicks(uint16_t pressed) {
        return pressed == 0x3000 || pressed == 0x6000 || pressed == 0x5000 || pressed == 0x7000;
    }

    // 020B38. The player at A6 has chosen a character with the buttons newly pressed in D0: the buttons choose
    // its colours, and Gallon chosen with Start held is Dark Gallon. Two players who chose the same are kept
    // apart by the second's colours.
    void ChooseColours(M68K &c) {
        const uint32_t me = c.a[6];
        SetLow16(c.d[0], c.d[0] & (kPunches | kKicks));
        const uint16_t pressed = static_cast<uint16_t>(c.d[0]);
        uint32_t colours = 0;
        if (TwoPunches(pressed)) {
            colours = 6;
            c.d[2] = 1;
        } else if (TwoKicks(pressed)) {
            colours = 7;
            c.d[2] = 1;
        } else {
            c.d[2] = 0;
            colours = 0;
            bool one = false;
            for (uint32_t n = 0; n < 6 && !one; n++) {
                if (pressed & kButtonColours[n]) {
                    colours = n;
                    one = true;
                }
            }
            if (!one) {
                SetLow16(c.d[0], pressed & kPunches);
            }
        }
        // Dark Gallon: with Start held (and the feature: whatever the buttons, punches his first colours and
        // kicks his second); and with the feature, Oboro Bishamon the same way
        const uint8_t character = Ram8(me + kCharacter);
        const int number = Ram8(me + kNumber);
        if (Ram8(Var(c, kNoCodes)) == 0 && character == kGallon && (Ram16(me + kHeldNow) & kStartHeld)) {
            if (TwoPunches(pressed)) {
                colours = 0;
                SetRam8(me + kCharacter, kDarkGallon);
            }
            c.d[2] = TwoKicks(pressed) ? 1 : 0;
            if (c.d[2]) {
                colours = 1;
                SetRam8(me + kCharacter, kDarkGallon);
            }
        }
        if (HiddenFor(number) && (character == kGallon || character == kBishamon)) {
            SetRam8(me + kCharacter, character == kGallon ? kDarkGallon : kOboro);
            colours = (pressed & kPunches) || !(pressed & kKicks) ? 0 : 1;
        }
        // The same character as the other player, who has chosen: the next colours (of two, for the hidden ones)
        const uint32_t other = Player(number == 0 ? 1 : 0);
        c.a[1] = other;
        SetLow8(c.d[0], Ram8(Var(c, kPlayersInNow)) | Ram8(Var(c, kPlayersThenVar)));
        if ((c.d[0] & 0xFF) == 3) {
            SetLow8(c.d[0], Ram8(other + kNumber));
            if (Ram8(Var(c, kHaveChosen)) & (1 << (c.d[0] & 7))) {
                const uint8_t mine = Ram8(me + kCharacter);
                SetLow8(c.d[0], mine);
                if (mine == Ram8(other + kCharacter) && (colours & 0xFF) == Ram8(other + kColours)) {
                    colours = (colours + 1) & 7;
                    if (mine == kDarkGallon || mine == kOboro) {
                        colours &= 1;
                    }
                }
            }
        }
        c.d[1] = colours;
        SetRam8(me + kColours, static_cast<uint8_t>(colours));
    }

    // 020C98. Called every frame of the choosing with D3 the buttons newly pressed: counts Start on the "?"
    // box, and five presses (no more) are Shadow's code. With the feature, the box is Shadow without them.
    void CountShadowCode(M68K &c) {
        const uint32_t me = c.a[6];
        const bool onBox = Ram8(me + kCursor) == kRandomBox;
        if (onBox && HiddenFor(Ram8(me + kNumber))) {
            SetRam8(me + kShadowCode, 0xFF);
            return;
        }
        if (Ram8(Var(c, kNoCodes)) != 0 || !onBox) {
            SetRam8(me + kStartsOnBox, 0);
        } else if (!(c.d[3] & kStartHeld)) {
            return;
        } else {
            SetRam8(me + kStartsOnBox, static_cast<uint8_t>(Ram8(me + kStartsOnBox) + 1));
            if (Ram8(me + kStartsOnBox) == kShadowStarts) {
                SetRam8(me + kShadowCode, 0xFF);
                return;
            }
        }
        SetRam8(me + kShadowCode, 0);
        SetRam8(me + kShadow, 0);
    }
} // namespace

// A mode's fighter (modes.h, MatchWanted): its hidden form, if the feature asks for one on that side
int HiddenForm(int n, int character) {
    if (!HiddenFor(n)) {
        return character;
    }
    return character == kGallon ? kDarkGallon : character == kBishamon ? kOboro : character;
}

void RegisterChoosing() {
    AddRewrite(0x020B38, "ChooseColours", ChooseColours, kFreeFlags, 365);
    AddRewrite(0x020C98, "CountShadowCode", CountShadowCode, kFreeFlags);
    TheGame().features.push_back({"hidden_characters", "Game", "Hidden characters",
                                  "Choosing Gallon gives Dark Gallon and choosing Bishamon gives Oboro Bishamon (the "
                                  "last boss, not chosen by any code on the board); a punch chooses their first colours, "
                                  "a kick their second. The \"?\" box gives Shadow, who becomes each opponent beaten. "
                                  "For player 1, player 2 or both; the alternative modes' fighters too.",
                                  0, {"Off", "Player 1", "Player 2", "Both"}});
}

} // namespace cps2recomp
