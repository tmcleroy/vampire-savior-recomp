// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy
//
// Vampire Savior: moves that go by the picture's edges, written again as C++ (rewrites: lift.h). A stage has
// no walls (edges.cpp): a move that goes to "the wall", or keeps someone in the picture, works out where the
// picture's edges are from the left edge of the scroll screen the player rides (01559E: `+10` of its record)
// and the board's 384 columns. In a wide picture (widescreen.h) the edges are 64 further out on each side, as
// the players' own are, so these moves ask WideMargin too; on the board's picture they are the game's code.
//
// Others of the kind, not written again yet (found by the board's width, 0x180, near a read of the plane's
// place; whose they are by the table of the characters' moves at 0BD1FA): Zabel 037312, Morrigan 03A96A,
// Lilith 04F412 and 0517BA, Jedah 055AE6, and more in the code shared by things thrown (063D58 on).
#include "vsav.h"
#include "widescreen.h"

#include <utility>

namespace cps2recomp {

namespace {
    using namespace vsav;

    constexpr uint32_t kPlaneOf = 0x01559E;  // A3: the record of the scroll screen the object at A6 rides (picture.cpp)
    constexpr uint32_t kDivide = 0x01688A;   // D3 = D3 / D2, signed longs (the game has no division instruction)
    constexpr uint32_t kAnimate = 0x027EB8;  // the player starts its animation D0 of the first list
    constexpr uint32_t kGoOn = 0x027F70;     // the player's animation goes on a frame
    constexpr int kPictureAcross = 0x180;
    constexpr uint32_t kFacing = 0x0B;       // byte: 1 facing left (towards the picture's left edge: 0)
    constexpr uint32_t kStep2 = 0x07;        // byte: the move's own step (in twos)
    constexpr uint32_t kTimer = 0x26;        // byte: frames the move's step waits for
    constexpr uint32_t kFloor = 0x3A;        // word: the height of the floor under the player

    int16_t Signed(uint32_t word) {
        return static_cast<int16_t>(word);
    }
    // The left edge of the picture the fight is played in, from the record at A3, and its width
    uint16_t PictureLeft(M68K &c) {
        return static_cast<uint16_t>(Mem16(c, c.a[3] + kLayerShownX) - WideMargin());
    }
    int PictureAcross() {
        return kPictureAcross + 2 * WideMargin();
    }

    // 022D96. A player that clings to the wall is put on it: 16 past the picture's edge behind it, and given a
    // push of 8 away from it (every character's code: the jump that clings, the cling, the jump off it)
    void OnTheWall(M68K &c) {
        const uint32_t player = c.a[6] & 0xFFFFFF;
        Call(c, kPlaneOf);
        uint16_t x = static_cast<uint16_t>(PictureLeft(c) - 0x10);
        c.d[1] = static_cast<uint32_t>(-8);
        if (Ram8(player + kFacing)) {
            x = static_cast<uint16_t>(x + PictureAcross() + 0x20);
            SetLow16(c.d[1], 8);
        }
        SetLow16(c.d[0], x);
        SetRam16(player + kX, x);
        SetRam16(player + 0x10C, static_cast<uint16_t>(c.d[1]));
    }

    // 0494B4. Q-Bee's flight to the wall behind her, its first frame: in the air, the speed that takes her to
    // the picture's edge in 32 frames and up to 104 above the floor in 16 (and down again in 32 more); then
    // an animation for how far she has to go and how high she is. `+360` keeps where the plane was, so that
    // she goes with it (049576).
    void QBeeToTheWall(M68K &c) {
        const uint32_t bee = c.a[6] & 0xFFFFFF;
        SetRam8(bee + kStep2, Ram8(bee + kStep2) + 2);
        SetRam8(bee + kAirborne, 1);
        Call(c, kPlaneOf);
        const uint16_t plane = Mem16(c, c.a[3] + kLayerShownX), left = PictureLeft(c);
        const bool right = Ram8(bee + kFacing) != 0;
        SetRam16(bee + 0x360, plane);
        // (a word of pixels made a long of 65536ths, and divided by the frames)
        c.d[3] = static_cast<uint32_t>(static_cast<uint16_t>((right ? left + PictureAcross() : left) - Ram16(bee + kX))) << 16;
        c.d[2] = 0x20;
        Call(c, kDivide);
        SetRam32(bee + kSpeedX, c.d[3]);
        SetRam32(bee + kPullX, 0);
        const uint16_t up = static_cast<uint16_t>(Ram16(bee + kFloor) + 0x68 - Ram16(bee + kY));
        c.d[3] = (static_cast<uint32_t>(up) << 16) | (c.d[3] >> 16);
        c.d[2] = 0x10;
        Call(c, kDivide);
        SetRam32(bee + kSpeedY, c.d[3]);
        c.d[2] = 0x20;
        Call(c, kDivide);
        c.d[3] = static_cast<uint32_t>(-static_cast<int32_t>(c.d[3]));
        SetRam32(bee + kPullY, c.d[3]);
        Call(c, kPlaneOf);
        // (how far she is from the left edge, in steps of 96: facing right, the game's sum comes to nothing)
        const uint16_t edge = PictureLeft(c);
        SetLow16(c.d[0], right ? edge - Ram16(bee + kX) : Ram16(bee + kX) - edge);
        c.d[1] = 0;
        c.d[3] = 2;
        for (;;) {
            SetLow16(c.d[0], c.d[0] - 0x60);
            if (Signed(c.d[0]) <= 0) {
                break;
            }
            SetLow8(c.d[1], c.d[1] + 1);
            SetLow16(c.d[3], c.d[3] - 1);
            if ((c.d[3] & 0xFFFF) == 0xFFFF) {
                break;
            }
        }
        // (and how high, in steps of 24)
        SetLow16(c.d[0], Ram16(bee + kY) - Ram16(bee + kFloor));
        c.d[2] = 8;
        c.d[3] = 1;
        for (;;) {
            SetLow16(c.d[0], c.d[0] - 0x18);
            if (Signed(c.d[0]) <= 0) {
                break;
            }
            SetLow8(c.d[2], c.d[2] - 4);
            SetLow16(c.d[3], c.d[3] - 1);
            if ((c.d[3] & 0xFFFF) == 0xFFFF) {
                break;
            }
        }
        SetLow16(c.d[1], c.d[1] + c.d[2]);
        SetLow8(c.d[0], c.opcodes[0x04956A + Signed(c.d[1])] + 0x35);
        Call(c, kAnimate);
    }

    // 049576. A frame of Q-Bee's flight to the wall: she goes with the plane, and on her way; landing ends it;
    // once she is on her way down (or high enough), at the picture's edge behind her, she is put on the wall
    // (16 past the edge, as 022D96 has it) and starts the attack from it
    constexpr uint32_t kMoves = 0x0273E2;    // the player moves by its speed; flags: "greater or equal" until it lands
    constexpr uint32_t kLands = 0x0270B6;    // the player is on the floor again
    constexpr uint32_t kAtEdge = 0x116;      // byte: 1 at the picture's left edge this frame, 2 at its right (edges.cpp)
    void QBeeFlies(M68K &c) {
        const uint32_t bee = c.a[6] & 0xFFFFFF;
        Call(c, kPlaneOf);
        SetLow16(c.d[0], Mem16(c, c.a[3] + kLayerShownX));
        SetLow16(c.d[1], c.d[0] - Ram16(bee + 0x360));
        SetRam16(bee + kX, static_cast<uint16_t>(Ram16(bee + kX) + c.d[1]));
        SetRam16(bee + 0x360, static_cast<uint16_t>(c.d[0]));
        Call(c, kMoves);
        const bool rising = Signed(Ram16(bee + kSpeedY)) >= 0;
        if (!rising && Signed(Ram16(bee + kY)) < Signed(Ram16(bee + kFloor))) {
            Call(c, kLands);
            return;
        }
        if (rising) {
            SetLow16(c.d[0], Ram16(bee + kFloor) + 0x68);
            if (Signed(c.d[0]) > Signed(Ram16(bee + kY))) {
                Call(c, kGoOn);
                return;
            }
        }
        SetLow8(c.d[0], Ram8(bee + kAtEdge));
        if (!(c.d[0] & 0xFF)) {
            Call(c, kGoOn);
            return;
        }
        SetLow8(c.d[0], c.d[0] - 1);
        if ((c.d[0] & 0xFF) != Ram8(bee + kFacing)) {
            Call(c, kGoOn);
            return;
        }
        SetRam8(bee + kStep2, Ram8(bee + kStep2) + 2);
        uint16_t x = static_cast<uint16_t>(PictureLeft(c) - 0x10);
        if (Ram8(bee + kFacing)) {
            x = static_cast<uint16_t>(x + PictureAcross() + 0x20);
        }
        SetRam16(bee + kX, x);
        c.d[0] = 0x38;
        Call(c, kAnimate);
    }

    // 049ACC. Q-Bee's throw that carries the opponent to the wall: the opponent flies until it is at the
    // picture's edge (less a distance of the character's), and is held there
    void QBeeCarries(M68K &c) {
        const uint32_t bee = c.a[6] & 0xFFFFFF;
        c.a[4] = 0xFFFF0000u | Ram16(bee + kOpponent);
        const uint32_t other = c.a[4] & 0xFFFFFF;
        SetRam32(other + kX, Ram32(other + kX) + Ram32(other + kSpeedX));
        SetRam32(other + kSpeedX, Ram32(other + kSpeedX) + Ram32(other + kPullX));
        SetRam32(other + kY, Ram32(other + kY) + Ram32(other + kSpeedY));
        Call(c, kPlaneOf);
        SetLow16(c.d[1], PictureLeft(c));
        SetLow16(c.d[0], Ram16(other + kX));
        c.d[2] = static_cast<uint16_t>((c.opcodes[0x049B30 + Ram8(bee + kCharacter) * 2] << 8) | c.opcodes[0x049B31 + Ram8(bee + kCharacter) * 2]);
        if (Ram8(other + kFacing)) {
            SetLow16(c.d[1], c.d[1] + c.d[2]);
            SetLow16(c.d[2], c.d[1]);
        } else {
            SetLow16(c.d[1], c.d[1] + PictureAcross() - c.d[2]);
            SetLow16(c.d[2], c.d[1]);
            std::swap(c.d[0], c.d[1]);
        }
        if (Signed(c.d[1]) > Signed(c.d[0])) {
            SetRam16(other + kX, static_cast<uint16_t>(c.d[2]));
            SetRam8(bee + kStep2, Ram8(bee + kStep2) + 2);
            SetRam8(bee + kTimer, 0x1E);
        }
        Call(c, kGoOn);
    }

    // 042542. A move of Bishamon's (number 0E) when what he sent out (the record at A3) has caught the opponent
    // (A4): it, and the opponent with it, are put inside the picture, 32 in from its edge, if they are not;
    // then Bishamon 128 from it, and if that would leave him outside the picture, he goes 160 in from the edge
    // and it 128 from him
    void BishamonCaught(M68K &c) {
        const uint32_t bishamon = c.a[6] & 0xFFFFFF, helper = c.a[3] & 0xFFFFFF, other = c.a[4] & 0xFFFFFF;
        const int side = WideMargin();
        const uint16_t left = static_cast<uint16_t>(Ram16(kCameraX) - side), inside = static_cast<uint16_t>(PictureAcross() - 0x40);
        SetLow16(c.d[0], Ram16(helper + kX) - left - 0x20);
        if ((c.d[0] & 0xFFFF) >= inside) {
            SetLow16(c.d[2], Ram16(other + kX) - Ram16(helper + kX));
            SetLow16(c.d[0], left);
            c.d[1] = 0x20;
            if (Ram8(helper + kFacing)) {
                SetLow16(c.d[0], c.d[0] + PictureAcross());
                SetLow16(c.d[1], -0x20);
            }
            SetLow16(c.d[0], c.d[0] + c.d[1]);
            SetRam16(helper + kX, static_cast<uint16_t>(c.d[0]));
            SetLow16(c.d[0], c.d[0] + c.d[2]);
            SetRam16(other + kX, static_cast<uint16_t>(c.d[0]));
        }
        SetLow16(c.d[0], Ram16(bishamon + kX) - Ram16(helper + kX));
        if (Signed(c.d[0]) < 0) {
            SetLow16(c.d[0], -Signed(c.d[0]));
        }
        if (Signed(c.d[0]) > 0x80) {
            return;
        }
        c.d[0] = static_cast<uint32_t>(-0x80);
        if (Ram8(bishamon + kFacing)) {
            SetLow16(c.d[0], 0x80);
        }
        SetLow16(c.d[0], c.d[0] + Ram16(bishamon + kX) - left - 0x20);
        if ((c.d[0] & 0xFFFF) >= inside) {
            SetLow16(c.d[0], left);
            SetLow16(c.d[1], 0xA0);
            if (Ram8(bishamon + kFacing)) {
                SetLow16(c.d[0], c.d[0] + PictureAcross());
                SetLow16(c.d[1], -0xA0);
            }
            SetLow16(c.d[0], c.d[0] + c.d[1]);
            SetRam16(bishamon + kX, static_cast<uint16_t>(c.d[0]));
        }
        c.d[0] = static_cast<uint32_t>(-0x80);
        if (Ram8(bishamon + kFacing)) {
            SetLow16(c.d[0], 0x80);
        }
        SetLow16(c.d[0], c.d[0] + Ram16(bishamon + kX));
        SetLow16(c.d[1], Ram16(other + kX) - Ram16(helper + kX));
        SetRam16(helper + kX, static_cast<uint16_t>(c.d[0]));
        SetLow16(c.d[0], c.d[0] + c.d[1]);
        SetRam16(other + kX, static_cast<uint16_t>(c.d[0]));
    }
} // namespace

void RegisterMoves() {
    AddRewrite(0x022D96, "OnTheWall", OnTheWall, kFreeFlags);
    AddRewrite(0x0494B4, "QBeeToTheWall", QBeeToTheWall, kFreeFlags, 534);
    AddRewrite(0x049576, "QBeeFlies", QBeeFlies, kFreeFlags, 167);
    AddRewrite(0x049ACC, "QBeeCarries", QBeeCarries, kFreeFlags, 260);
    AddRewrite(0x042542, "BishamonCaught", BishamonCaught, kFreeFlags);
}

} // namespace cps2recomp
