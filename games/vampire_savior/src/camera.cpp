// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy
//
// Vampire Savior: the camera of a fight written again as C++ (rewrites: lift.h). The camera is the record of
// the stage's own scroll screen, the one the players stand in (kCameraRecord, at A6 here): where its
// picture's left edge and bottom are in the stage. Each frame it looks at where the players are and goes
// towards where it wants them in the picture, no faster than its most for a frame and no further than its
// ends; the other scroll screens are then placed from it (planes.cpp).
//
// What the modes and widescreen do to the camera is here, in the rules they change: a mode can have it go
// to a place of its own (modes.h, CameraAt), and in a wide picture its ends are 64 in from the board's.
#include "modes.h"
#include "stage.h"
#include "widescreen.h"

#include <algorithm>

namespace cps2recomp {

namespace {
    using namespace vsav;

    // The board's picture is 384 wide: its middle, and the margins the camera keeps the players' outer
    // edges inside while they are less than half a picture apart
    constexpr int kMiddle = 0xC0, kKeepRightOf = 0x60, kKeepLeftOf = 0x120;
    // ... and up the picture, the height it keeps the higher player at
    constexpr int kKeepBelow = 0x60;

    int16_t Signed(uint32_t word) {
        return static_cast<int16_t>(word);
    }

    // 01E8F4. The players' outer edges across the stage: the left one in D0, the right one in D1. A player
    // the camera is not to follow is left out, and the edges are the other's.
    void PlayersAcross(M68K &c) {
        const uint32_t one = Var(c, 0x400), two = Var(c, 0x800);
        c.a[1] = c.a[5] + 0x400;
        c.a[2] = c.a[5] + 0x800;
        auto left = [](uint32_t player) { return static_cast<uint16_t>(Ram16(player + kX) - Ram16(player + kHalfWidth)); };
        auto right = [](uint32_t player) { return static_cast<uint16_t>(Ram16(player + kX) + Ram16(player + kHalfWidth)); };
        auto leftOut = [](uint32_t player) { return (Ram8(player + kNotFollowed) & 0x80) && !Ram8(player + kFollowedAnyway); };
        if (leftOut(one) || leftOut(two)) {
            const uint32_t followed = leftOut(one) ? two : one;
            SetLow16(c.d[0], left(followed));
            SetLow16(c.d[1], right(followed));
            return;
        }
        SetLow16(c.d[0], Signed(left(one)) <= Signed(left(two)) ? left(one) : left(two));
        SetLow16(c.d[2], right(two));
        SetLow16(c.d[1], Signed(right(one)) >= Signed(right(two)) ? right(one) : right(two));
    }

    // 01E962. The players' heights: the lower in D0, the higher in D1 (a player not followed is left out)
    void PlayersUp(M68K &c) {
        const uint32_t one = Var(c, 0x400), two = Var(c, 0x800);
        c.a[1] = c.a[5] + 0x400;
        c.a[2] = c.a[5] + 0x800;
        if ((Ram8(one + kNotFollowed) & 0x80) || (Ram8(two + kNotFollowed) & 0x80)) {
            const uint32_t followed = (Ram8(one + kNotFollowed) & 0x80) ? two : one;
            SetLow16(c.d[0], Ram16(followed + kY));
            SetLow16(c.d[1], Ram16(followed + kY));
            return;
        }
        SetLow16(c.d[0], Ram16(one + kY));
        SetLow16(c.d[1], Ram16(two + kY));
        if (Signed(c.d[1]) < Signed(c.d[0])) {
            std::swap(c.d[0], c.d[1]);
        }
    }

    // 01E450, 01E4A0. The camera is put at the place in D0, across or up (stage.h)
    void CameraSetAcross(M68K &c) {
        PlaceLayerAcross(c, kLayer16);
    }
    void CameraSetUp(M68K &c) {
        PlaceLayerUp(c, kLayer16);
    }

    // The camera stays where it is, across
    void CameraHeld(M68K &c) {
        SetLow16(c.d[0], Ram16((c.a[6] & 0xFFFFFF) + kLayerX));
        CameraSetAcross(c);
    }

    // The camera goes `by` (D2, never less than nothing) one way, no further than its most for a frame and
    // its end that way
    struct Way {
        uint32_t at;
        void (*place)(M68K &c);
    };
    constexpr Way kAcross = {kLayerX, CameraSetAcross}, kUp = {kLayerY, CameraSetUp};
    void CameraGoes(M68K &c, const Way &way, uint32_t most, uint32_t end, bool forwards) {
        const uint32_t camera = c.a[6] & 0xFFFFFF;
        if ((c.d[2] & 0xFFFF) > Ram16(camera + most)) {
            SetLow16(c.d[2], Ram16(camera + most));
        }
        const uint16_t to = static_cast<uint16_t>(forwards ? Ram16(camera + way.at) + c.d[2] : Ram16(camera + way.at) - c.d[2]);
        SetRam16(camera + way.at, to);
        const uint16_t stops = Ram16(camera + end);
        const bool past = forwards ? Signed(stops) <= Signed(to) : Signed(stops) >= Signed(to);
        SetLow16(c.d[0], past ? stops : to);
        way.place(c);
    }

    // A mode's own place for the camera (modes.h): it goes there, no faster than the game lets it go
    void CameraTo(M68K &c, const Way &way, uint32_t most, int wanted) {
        const uint32_t camera = c.a[6] & 0xFFFFFF;
        const int now = Signed(Ram16(camera + way.at)), step = std::max<int>(Ram16(camera + most), 1);
        const int to = now + std::clamp(wanted - now, -step, step);
        SetRam16(camera + way.at, static_cast<uint16_t>(to));
        SetLow16(c.d[0], static_cast<uint16_t>(to));
        way.place(c);
    }

    // 01E36E. A frame of the camera, across. While the players' outer edges are less than half a picture
    // apart it only moves when one of them is near the picture's side; further apart, it keeps the middle
    // between them in the picture's middle.
    void CameraAcross(M68K &c) {
        const uint32_t camera = c.a[6] & 0xFFFFFF;
        WideCameraEnds(camera);
        if (CameraAt().x >= 0) {
            return CameraTo(c, kAcross, kCameraMostX, CameraAt().x);
        }
        if (Ram8(camera + kCameraHeld)) {
            return CameraHeld(c);
        }
        PlayersAcross(c);
        const uint16_t left = static_cast<uint16_t>(c.d[0]), right = static_cast<uint16_t>(c.d[1]), at = Ram16(camera + kLayerX);
        // How far the camera is from where it wants to be, and which way that is
        int off;
        bool goesRight;
        if (static_cast<uint16_t>(right - left) >= kMiddle) {
            off = Signed(static_cast<uint16_t>((static_cast<uint16_t>(right + left) >> 1) - at)) - kMiddle;
            goesRight = off >= 0;
        } else if (Signed(right - at) >= kKeepLeftOf) {
            off = Signed(right - at) - kKeepLeftOf;
            goesRight = true;
        } else if (Signed(left - at) <= kKeepRightOf) {
            off = Signed(left - at) - kKeepRightOf;
            goesRight = false;
        } else {
            SetLow16(c.d[2], left - at - kKeepRightOf);
            return CameraHeld(c);
        }
        SetLow16(c.d[2], off < 0 ? -off : off);
        CameraGoes(c, kAcross, kCameraMostX, goesRight ? kCameraEndRight : kCameraEndLeft, goesRight);
    }

    // 01E3F2. A frame of the camera, up: it keeps the higher player kKeepBelow up the picture.
    void CameraUp(M68K &c) {
        const uint32_t camera = c.a[6] & 0xFFFFFF;
        if (CameraAt().y >= 0) {
            return CameraTo(c, kUp, kCameraMostY, CameraAt().y);
        }
        if (Ram8(camera + kCameraHeld)) {
            return CameraHeld(c); // (the game's own: it is the place across that is set again here)
        }
        PlayersUp(c);
        const int off = Signed(static_cast<uint16_t>(c.d[1]) - Ram16(camera + kLayerY)) - kKeepBelow;
        SetLow16(c.d[2], off < 0 ? -off : off);
        if (off > 0) {
            CameraGoes(c, kUp, kCameraMostY, kCameraEndTop, true);
        } else if (off < 0) {
            CameraGoes(c, kUp, kCameraMostY, kCameraEndBottom, false);
        } else {
            SetLow16(c.d[0], Ram16(camera + kLayerY));
            CameraSetUp(c);
        }
    }
} // namespace

void RegisterCamera() {
    // (the last number is the time a call is charged: the game's own, as the check measured it)
    AddRewrite(kPlayersAcross, "PlayersAcross", PlayersAcross, kFreeFlags, 186);
    AddRewrite(kPlayersUp, "PlayersUp", PlayersUp, kFreeFlags, 100);
    AddRewrite(kCameraAcrossSet, "CameraSetAcross", CameraSetAcross, kFreeFlags, 86);
    AddRewrite(kCameraUpSet, "CameraSetUp", CameraSetUp, kFreeFlags, 86);
    AddRewrite(kCameraAcross, "CameraAcross", CameraAcross, kFreeFlags, 487);
    AddRewrite(kCameraUp, "CameraUp", CameraUp, kFreeFlags, 370);
}

} // namespace cps2recomp
