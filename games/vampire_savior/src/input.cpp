// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy
//
// Vampire Savior: reading the control panel, written again as C++ (a rewrite: lift.h). The game does this
// once a frame, in its frame interrupt; everything it knows of the sticks and buttons comes from here.
#include "vsav.h"

namespace cps2recomp {

namespace {
    using namespace vsav;

    // The board's input ports (bytes; a line reads 0 while pressed)
    constexpr uint32_t kPortPlayer2 = 0x804000, kPortPlayer1 = 0x804001; // stick in bits 0-3, punches in 4-6
    constexpr uint32_t kPortKicks = 0x804011;   // player 1's kicks in bits 0-2, player 2's light and medium in 4-5
    constexpr uint32_t kPortStarts = 0x804020;  // starts and coins; player 2's hard kick in bit 6
    constexpr uint32_t kPortService = 0x804021; // the service and test switches (and the settings memory's line)

    // 001576. Reads the panel: what each player holds now (and keeps what they held the frame before), and
    // the starts, coins and switches.
    void ReadControls(M68K &c) {
        // A byte a frame of the starts and coins, and of the switches: the last four frames in a long
        SetRam32(Var(c, kStartsAndCoins), Ram32(Var(c, kStartsAndCoins)) >> 8);
        SetRam32(Var(c, kService), Ram32(Var(c, kService)) >> 8);
        const uint8_t starts = static_cast<uint8_t>(~Mem8(c, kPortStarts));
        SetRam8(Var(c, kStartsAndCoins), starts);
        SetRam8(Var(c, kService), static_cast<uint8_t>(~Mem8(c, kPortService)));

        for (int player = 0; player < 2; player++) {
            SetRam16(Var(c, kControlsBefore[player]), Ram16(Var(c, kControls[player])));
        }
        // A player's word: the buttons above (punches in bits 0-2, kicks in 4-6), the stick below
        const uint8_t kickPort = static_cast<uint8_t>(~Mem8(c, kPortKicks));
        const uint8_t kicks[2] = {static_cast<uint8_t>((kickPort << 4) & 0x70),
                                  static_cast<uint8_t>((kickPort & 0x30) | (starts & 0x40))};
        const uint32_t ports[2] = {kPortPlayer1, kPortPlayer2};
        for (int player = 0; player < 2; player++) {
            const uint8_t port = static_cast<uint8_t>(~Mem8(c, ports[player]));
            SetRam8(Var(c, kControls[player]) + 1, port & 0x0F);
            SetRam8(Var(c, kControls[player]), static_cast<uint8_t>((port >> 4) | kicks[player]));
        }
        // (the kicks' port as it was read, this frame and the last; and something cleared)
        SetRam8(Var(c, 0x7425), Ram8(Var(c, 0x7424)));
        SetRam8(Var(c, 0x7424), Mem8(c, 0x804010));
        SetRam32(Var(c, 0x7420), 0);
    }
} // namespace

void RegisterInput() {
    AddRewrite(kReadControls, "ReadControls", ReadControls, kFreeD0 | kFreeD1 | kFreeD3 | kFreeD4 | kFreeFlags);
}

} // namespace cps2recomp
