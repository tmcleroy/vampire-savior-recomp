// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy
//
// Vampire Savior: a stage's pieces, written again as C++ (rewrites: lift.h). What moves on a stage and is
// not a scroll screen is an object with a record: the dancers and the lady under the bird cage on Feast of
// the Damned, the desert's camel. There are twenty-four records for them (0x80 bytes each, from kPieces),
// and each frame every one in use runs its script: the game's code for that kind of piece, chosen by a
// byte of the record. A script steps the piece's animation, moves it, and ends by asking for it to be drawn
// (picture.cpp). Fifty-eight scripts between the twelve stages; the ones written again here are named in
// kScripts, and the rest are the game's own code still.
//
// A script has four steps (the record's kStep, in twos): 0 sets the piece up, 2 is a frame of it, and 4 and
// 6 give the record back.
#include "stage.h"
#include "widescreen.h"

#include <cstdio>
#include <cstdlib>

namespace cps2recomp {

namespace {
    using namespace vsav;

    constexpr uint32_t kPieces = 0xFFC800, kPieceSize = 0x80;
    constexpr int kPieceRecords = 24;
    constexpr int32_t kPiecesLeft = 0xB5;  // from A5, a byte: the records the frame has still to run
    constexpr int32_t kFrozen = 0x134;     // from A5, a byte: not zero: nothing on the stage is animated (a super move's flash)
    // The records not in use: a list of them (a long from A5: its end, going down) and how many (a byte)
    constexpr int32_t kFreePieces = 0x7972, kFreePiecesCount = 0x79C1;
    constexpr uint32_t kTakePiece = 0x016FD0; // a free record in A4, if there is one
    constexpr uint32_t kRandom = 0x014E8A;    // a made-up byte in D0
    constexpr uint32_t kCopyColours = 0x01C3A4; // sets of sixteen colours from A0 to A1: D7 and one more

    // A piece's record
    constexpr uint32_t kScript = 0x02;    // byte: which script
    constexpr uint32_t kWhich = 0x03;     // byte: which piece of that kind (for most, its animation's number)
    constexpr uint32_t kStep = 0x04;      // byte
    constexpr uint32_t kDepth = 0x09;
    constexpr uint32_t kHalf = 0x0A;      // byte (a piece that is two by turns): which of the two it is now
    constexpr uint32_t kPlaces = 0x18;    // long: copied from a piece to the pieces it makes
    constexpr uint32_t kTimer = 0x20;     // byte: frames the animation's frame is still shown for
    constexpr uint32_t kOver = 0x21;      // byte: FF when the animation has come to its end
    constexpr uint32_t kEvent = 0x23;     // byte: set by an animation's frame for its script to act on
    constexpr uint32_t kRow = 0x24;
    constexpr uint32_t kTurn = 0x26;      // byte: a script's own count
    constexpr uint32_t kMaker = 0x30;     // word: the record that made this one
    constexpr uint32_t kKeptFrame = 0x40, kKeptFrame2 = 0x44; // longs: for a piece that is two by turns: each one's frame

    // The stage's pieces' animations: a table for each stage of the animations' places; an animation is
    // frames of eight bytes, of which the first four are copied to the record's kTimer on: how long it is
    // shown, the end's mark, and an event for the script. The second byte of a frame says what follows it:
    // 0 the next frame, below nothing a jump (to the long after it), bit 6 the end.
    constexpr uint32_t kStageAnimations = 0x2776B0;

    void SetFrame(M68K &c) {
        const uint32_t piece = c.a[6] & 0xFFFFFF;
        SetRam32(piece + kFrame, c.a[0]);
        SetRam32(piece + kTimer, Mem32(c, c.a[0]));
    }

    // 015084. The animation numbered D0 of the table at A0 is started for the piece at A6
    void SetAnimation(M68K &c) {
        SetLow16(c.d[0], (c.d[0] & 0xFF) * 2);
        SetLow16(c.d[0], Mem16(c, c.a[0] + static_cast<uint32_t>(static_cast<int16_t>(c.d[0]))));
        c.a[0] += static_cast<uint32_t>(static_cast<int16_t>(c.d[0]));
        SetFrame(c);
    }

    // 07D372. ... of the stage's own table
    void SetStageAnimation(M68K &c) {
        c.a[0] = kStageAnimations;
        SetLow16(c.d[1], Mem16(c, kStageAnimations + Ram16(kStage)));
        c.a[0] += static_cast<uint32_t>(static_cast<int16_t>(c.d[1]));
        SetAnimation(c);
    }
    void SetStageAnimation(M68K &c, uint32_t number) {
        c.d[0] = number;
        SetStageAnimation(c);
    }

    // 015030. A frame of the animation of the piece at A6
    void Animate(M68K &c) {
        const uint32_t piece = c.a[6] & 0xFFFFFF;
        SetRam8(piece + kTimer, Ram8(piece + kTimer) - 1);
        if (Ram8(piece + kTimer)) {
            return;
        }
        c.a[0] = Ram32(piece + kFrame);
        SetLow8(c.d[0], Mem8(c, c.a[0] + kFrameEnd));
        if ((c.d[0] & 0x80)) {
            c.a[0] = Mem32(c, c.a[0] + 8);
        } else if (c.d[0] & 0x40) {
            SetRam8(piece + kOver, 0xFF);
            return;
        } else {
            c.a[0] += 8;
        }
        SetFrame(c);
    }

    // 0170A6. The record at A6 is given back
    void GiveBack(M68K &c) {
        const uint32_t piece = c.a[6] & 0xFFFFFF;
        c.d[0] = 0;
        for (uint32_t at = 0; at < 16; at += 4) {
            SetRam32(piece + at, 0);
        }
        SetRam8(piece + 0x08, 0x10);
        SetRam8(piece + 0x0D, 1);
        c.a[0] = Ram32(Var(c, kFreePieces)) - 2;
        c.Write(c.a[0], 1, c.a[6] & 0xFFFF);
        SetRam32(Var(c, kFreePieces), c.a[0]);
        SetRam8(Var(c, kFreePiecesCount), Ram8(Var(c, kFreePiecesCount)) + 1);
    }

    bool Frozen(M68K &c) {
        return Ram8(Var(c, kFrozen)) != 0;
    }
    // What a script begins with: its step, and (as the game's own way to the step's code leaves it) the
    // distance of that code from the script's table of steps
    int Step(M68K &c, const uint16_t distances[4]) {
        const int step = Ram8((c.a[6] & 0xFFFFFF) + kStep);
        SetLow8(c.d[0], step);
        SetLow16(c.d[1], distances[(step >> 1) & 3]);
        return step;
    }
    void PieceDrawn(M68K &c) {
        Call(c, 0x015696); // (picture.cpp)
    }

    // --- every stage ---

    // 07D354 (script 0). A piece that does nothing but go through its animation, drawn while the camera
    // is to its side of a place
    void Plain(M68K &c) {
        static constexpr uint16_t kDistances[4] = {0x08, 0x2A, 0x3C, 0x3C};
        const uint32_t piece = c.a[6] & 0xFFFFFF;
        switch (Step(c, kDistances)) {
        case 0:
            SetRam8(piece + kStep, 2);
            SetStageAnimation(c, Ram8(piece + kWhich));
            break;
        case 2:
            if (!Frozen(c)) {
                Animate(c);
            }
            Call(c, 0x0154CC);
            break;
        default:
            GiveBack(c);
            break;
        }
    }

    // --- Feast of the Damned ---

    // 07D3A2 (script 1). As a plain piece, but a frame of its animation with an event is shown for up to
    // three frames longer, by chance (the candles' flames)
    void Flickering(M68K &c) {
        static constexpr uint16_t kDistances[4] = {0x08, 0x18, 0x42, 0x42};
        const uint32_t piece = c.a[6] & 0xFFFFFF;
        switch (Step(c, kDistances)) {
        case 0:
            SetRam8(piece + kStep, 2);
            SetStageAnimation(c, Ram8(piece + kWhich));
            break;
        case 2:
            if (!Frozen(c)) {
                if (Ram8(piece + kEvent)) {
                    SetRam8(piece + kEvent, 0);
                    Call(c, kRandom);
                    SetLow16(c.d[0], c.d[0] & 3);
                    SetRam16(piece + kTimer, static_cast<uint16_t>(Ram16(piece + kTimer) + c.d[0]));
                }
                Animate(c);
            }
            Call(c, 0x0154CC);
            break;
        default:
            GiveBack(c);
            break;
        }
    }

    // 07D3F6 (script 2). The dancers. One record is two pieces by turns, by which side of a place the
    // camera is: the couples dancing on the left, who go through six dances in turn, and those on the
    // right. (They share a part of the board's memory for objects' pictures, so only one can be shown.)
    constexpr uint16_t kDancersChange = 0x1C0;
    constexpr uint16_t kDancersLeft[2] = {0x180, 0x30}, kDancersRight[2] = {0x398, 0x70}; // where each is, across and up
    constexpr uint8_t kDances = 6, kDancersRightAnimation = 10;

    void Dance(M68K &c, uint32_t number) {
        c.d[0] = number;
        SetRam8((c.a[6] & 0xFFFFFF) + kTurn, number);
        SetStageAnimation(c);
    }
    // In a wide picture (widescreen.h) both can be in it at once, so there the record is the left ones for
    // good and a second record, made from it, is the right ones: kWideHalf says which a record is. With
    // the board's picture again, the second is given back and the first goes by the camera as before.
    constexpr uint32_t kWideHalf = 0x27; // byte (the script's own, 0 on the board)
    enum : uint8_t { kByTurns = 0, kLeftForGood = 1, kRightForGood = 2 };

    void DancersLeft(M68K &c, uint32_t piece) {
        SetRam8(piece + kHalf, 0);
        SetRam32(piece + kFrame, Ram32(piece + kKeptFrame2));
        SetRam16(piece + kX, kDancersLeft[0]);
        SetRam16(piece + kY, kDancersLeft[1]);
    }
    void DancersRight(M68K &c, uint32_t piece) {
        SetRam8(piece + kHalf, 1);
        SetRam32(piece + kKeptFrame2, Ram32(piece + kFrame));
        SetRam32(piece + kFrame, Ram32(piece + kKeptFrame));
        SetRam16(piece + kX, kDancersRight[0]);
        SetRam16(piece + kY, kDancersRight[1]);
    }
    // (a frame of the ones on the left: their dances in turn)
    void DancersDance(M68K &c, uint32_t piece) {
        if (!Frozen(c)) {
            if (Ram8(piece + kOver) & 0x80) {
                SetLow8(c.d[0], Ram8(piece + kTurn) + 1);
                Dance(c, (c.d[0] & 0xFF) == kDances + 1 ? 1 : c.d[0]);
            }
            Animate(c);
        }
        PieceDrawn(c);
    }
    // A frame of the dancers in a wide picture; false when the board's own rules are to go on
    bool DancersWide(M68K &c, uint32_t piece) {
        const uint8_t half = Ram8(piece + kWideHalf);
        if (!WidePlay()) {
            if (half == kRightForGood) {
                GiveBack(c);
                return true;
            }
            SetRam8(piece + kWideHalf, kByTurns);
            return false;
        }
        if (half == kByTurns) {
            if (!Ram8(Var(c, kFreePiecesCount))) {
                return false; // (no record to spare: by turns, as on the board)
            }
            Call(c, kTakePiece);
            const uint32_t made = c.a[4] & 0xFFFFFF;
            for (uint32_t at = 0; at < kPieceSize; at++) {
                SetRam8(made + at, Ram8(piece + at));
            }
            if (!Ram8(made + kHalf)) {
                DancersRight(c, made);
            }
            SetRam8(made + kWideHalf, kRightForGood);
            if (Ram8(piece + kHalf)) {
                DancersLeft(c, piece);
            }
            SetRam8(piece + kWideHalf, kLeftForGood);
        }
        if (Ram8(piece + kWideHalf) == kLeftForGood) {
            DancersDance(c, piece);
        } else {
            PieceDrawn(c);
        }
        return true;
    }

    void Dancers(M68K &c) {
        static constexpr uint16_t kDistances[4] = {0x08, 0x36, 0xB2, 0xB2};
        const uint32_t piece = c.a[6] & 0xFFFFFF;
        const bool cameraRight = Ram16(kCameraX) >= kDancersChange;
        switch (Step(c, kDistances)) {
        case 0:
            SetRam8(piece + kStep, 2);
            SetRam8(piece + kHalf, 1);
            Dance(c, 1);
            SetRam32(piece + kKeptFrame2, Ram32(piece + kFrame));
            SetStageAnimation(c, kDancersRightAnimation);
            SetRam32(piece + kKeptFrame, Ram32(piece + kFrame));
            break;
        case 2:
            if (DancersWide(c, piece)) {
                break;
            }
            if (!Ram8(piece + kHalf)) {
                if (cameraRight) {
                    DancersRight(c, piece); // (and they are drawn from the next frame)
                } else {
                    DancersDance(c, piece);
                }
            } else if (!cameraRight) {
                DancersLeft(c, piece);
            } else {
                PieceDrawn(c);
            }
            break;
        default:
            GiveBack(c);
            break;
        }
    }

    // 07D4BA (script 3). The lady under the bird cage, who fans herself. Her fan arm is one picture in
    // several sets of colours, and which of them shows is a matter of the colours: at a frame of her
    // animation with an event, the set of colours for that pose is copied in. She is there only while the
    // camera is far enough right for her to be in the picture.
    constexpr uint16_t kLadyFrom = 0x170;
    constexpr uint32_t kStageColours = 0x38C298, kLadyColours = 0x90C3C0;
    constexpr int32_t kColoursOf = 0x140; // from A5, a word: which of the long table's entries is this stage's
    constexpr uint16_t kLadyPoses[6] = {0x0000, 0x0100, 0x1120, 0x1100, 0x1140, 0x1160}; // by the event, in twos

    void Lady(M68K &c) {
        static constexpr uint16_t kDistances[4] = {0x08, 0x14, 0x68, 0x68};
        const uint32_t piece = c.a[6] & 0xFFFFFF;
        switch (Step(c, kDistances)) {
        case 0:
            SetRam8(piece + kStep, 2);
            SetStageAnimation(c, 0x18);
            break;
        case 2:
            // (a wide picture's right edge is further right: widescreen.h)
            if (Ram16(kCameraX) + WideMargin() < kLadyFrom) {
                break;
            }
            if (!Frozen(c)) {
                Animate(c);
                SetLow8(c.d[0], Ram8(piece + kEvent));
                if (c.d[0] & 0xFF) {
                    SetRam8(piece + kEvent, 0);
                    SetLow16(c.d[0], static_cast<int8_t>(c.d[0]));
                    SetLow16(c.d[1], Ram16(Var(c, kColoursOf)));
                    c.a[0] = Mem32(c, kStageColours + static_cast<uint32_t>(static_cast<int16_t>(c.d[1])));
                    c.a[0] += static_cast<uint32_t>(static_cast<int16_t>(kLadyPoses[((c.d[0] & 0xFFFF) >> 1) % 6]));
                    c.a[1] = kLadyColours;
                    c.d[7] = 0;
                    Call(c, kCopyColours);
                }
            }
            PieceDrawn(c);
            break;
        default:
            GiveBack(c);
            break;
        }
    }

    // 07D534 (script 4). A piece that, at a frame of its animation with the event 1, lets go another
    // piece (script 5) from where it is
    constexpr uint32_t kLetGo = 0x01010500; // the new record's first four bytes: in use, drawn, script 5

    void Thrower(M68K &c) {
        static constexpr uint16_t kDistances[4] = {0x08, 0x14, 0x56, 0x56};
        const uint32_t piece = c.a[6] & 0xFFFFFF;
        switch (Step(c, kDistances)) {
        case 0:
            SetRam8(piece + kStep, 2);
            SetStageAnimation(c, 0);
            break;
        case 2:
            if (!Frozen(c)) {
                if (Ram8(piece + kEvent) == 1) {
                    SetRam8(piece + kEvent, 0);
                    const bool any = Ram8(Var(c, kFreePiecesCount)) != 0;
                    Call(c, kTakePiece);
                    if (any) {
                        const uint32_t made = c.a[4] & 0xFFFFFF;
                        SetRam32(made, kLetGo);
                        SetRam32(made + kPlaces, Ram32(piece + kPlaces));
                        SetRam8(made + kIsRiding, Ram8(piece + kIsRiding));
                        SetRam16(made + kRow, Ram16(piece + kRow));
                        SetRam16(made + kMaker, static_cast<uint16_t>(c.a[6]));
                    }
                }
                Animate(c);
            }
            PieceDrawn(c);
            break;
        default:
            GiveBack(c);
            break;
        }
    }

    // --- scripts that do nothing while the camera is the other side of a place ---
    //
    // A piece that is in the board's picture only with the camera to one side of a place has, in several
    // scripts, a frame that begins by looking at the camera: the other side of the place, and nothing of
    // the frame is done: no animation, no movement, nothing drawn (the lady above is one). The test is all
    // that is written again of these: the rest of the frame is the game's own code still, from where the
    // test ends. In a wide picture the place is 64 further off, and a piece that is itself near the
    // picture is not out whatever the camera's place (it may ride something slower than the camera).
    bool OutOfView(M68K &c, uint16_t place, bool rightOfPlace, bool placedByFrame = false) {
        const uint16_t camera = Ram16(kCameraX);
        const int side = WideMargin();
        if (rightOfPlace ? camera + side >= place : static_cast<int>(camera) - side < place) {
            return false;
        }
        // (a piece whose frame works its place out from the camera's has no place to go by while out)
        int across = 0;
        return !(side && !placedByFrame && AcrossInPicture(c, c.a[6] & 0xFFFFFF, across) && across >= -192 && across < 0x180 + 192);
    }
    // (kAfter: where the game's code goes on when the piece is not out)
    template <uint16_t kPlace, bool kRightOfPlace, uint32_t kAfter, bool kPlacedByFrame = false>
    void WhileInView(M68K &c) {
        if (!OutOfView(c, kPlace, kRightOfPlace, kPlacedByFrame)) {
            Call(c, kAfter);
        }
    }
    // ... and the same for the two scripts whose frame takes the camera's place into D1 first
    template <uint16_t kPlace, uint32_t kAfter>
    void WhileLeftOf(M68K &c) {
        SetLow16(c.d[1], Ram16(kCameraX));
        if (!OutOfView(c, kPlace, false, true)) {
            Call(c, kAfter);
        }
    }

    // 07F26A (script 26, Vanity Paradise's floating things): the place is the record's own
    void FloaterFrame(M68K &c) {
        const uint16_t place = Ram16((c.a[6] & 0xFFFFFF) + 0x2A);
        SetLow16(c.d[0], place);
        // (the game's test is "the camera is right of the place": one more than the others')
        if (!OutOfView(c, static_cast<uint16_t>(place + 1), true)) {
            Call(c, HookAt(0x07F274));
        }
    }

    // 07DF4C (script 10, Concrete Cave): a thing that goes left in jumps while the camera is left of a
    // place; right of it, it is put back at its start
    void RunnerFrame(M68K &c) {
        Call(c, OutOfView(c, 0x1BF, false) ? 0x07DF82 : HookAt(0x07DF54));
    }

    // --- other pieces that are two by turns ---
    //
    // Three more scripts have a record that is one thing left of a place of the camera's and another right
    // of it, as the dancers are: a still picture each side. In a wide picture they are done as the
    // dancers: the record is the left one for good, and a second made from it the right one.
    struct Turns {
        uint16_t change;                        // the camera's place where the record changes
        uint32_t which;                         // the record's byte that says which it is now (1: the right one)
        void (*become)(uint32_t piece, bool right);
    };
    void TurnsFrame(M68K &c, const Turns &turns) {
        const uint32_t piece = c.a[6] & 0xFFFFFF;
        const uint8_t half = Ram8(piece + kWideHalf);
        if (!WidePlay() && half == kRightForGood) {
            GiveBack(c);
            return;
        }
        if (WidePlay() && half == kByTurns && Ram8(Var(c, kFreePiecesCount))) {
            Call(c, kTakePiece);
            const uint32_t made = c.a[4] & 0xFFFFFF;
            if (Ram8(piece + turns.which)) {
                turns.become(piece, false);
            }
            for (uint32_t at = 0; at < kPieceSize; at++) {
                SetRam8(made + at, Ram8(piece + at));
            }
            turns.become(made, true);
            SetRam8(made + kWideHalf, kRightForGood);
            SetRam8(piece + kWideHalf, kLeftForGood);
        } else if (!WidePlay()) {
            SetRam8(piece + kWideHalf, kByTurns);
        }
        if (Ram8(piece + kWideHalf) == kByTurns) {
            // (the board's rule)
            const bool cameraRight = Ram16(kCameraX) >= turns.change;
            if (cameraRight != (Ram8(piece + turns.which) != 0)) {
                turns.become(piece, cameraRight);
            }
        }
        PieceDrawn(c);
    }
    void Become(uint32_t piece, bool right, uint32_t frame, uint16_t x, uint16_t y) {
        SetRam8(piece + kHalf, right);
        SetRam32(piece + kFrame, Ram32(piece + frame));
        SetRam16(piece + kX, x);
        SetRam16(piece + kY, y);
    }
    // 07DE78 (script 8, Concrete Cave)
    constexpr Turns kTurns8 = {0x170, kHalf, [](uint32_t piece, bool right) {
                                   right ? Become(piece, true, kKeptFrame2, 0x32C, 0xC0) : Become(piece, false, kKeptFrame, 0x150, 0x54);
                               }};
    // 080A20 (script 57, Abaraya): the right one is drawn further back
    constexpr Turns kTurns57 = {0x170, kHalf, [](uint32_t piece, bool right) {
                                    right ? Become(piece, true, kKeptFrame2, 0x398, 0xFFFE) : Become(piece, false, kKeptFrame, 0x121, 0);
                                    SetRam8(piece + kDepth, right ? 0 : 2);
                                }};
    // 0809A6 (script 56, Tower of Arrogance): one picture, turned about and at another place
    constexpr Turns kTurns56 = {0x1C0, kFacesRight, [](uint32_t piece, bool right) {
                                    SetRam8(piece + kFacesRight, right);
                                    SetRam16(piece + kX, Ram16(piece + (right ? 0x42 : 0x40)));
                                }};
    template <const Turns &turns>
    void ByTurns(M68K &c) {
        TurnsFrame(c, turns);
    }

    // The scripts, by a record's kScript: where each is in the game's program
    constexpr uint32_t kScripts[58] = {
        0x07D354, 0x07D3A2, 0x07D3F6, 0x07D4BA, 0x07D534, 0x07D59C, 0x07D710, 0x07DDD8, 0x07DE32, 0x07DECE, 0x07DF1E, 0x07DF9C,
        0x07E10E, 0x07E162, 0x07E20E, 0x07E5A8, 0x07E62C, 0x07EA82, 0x07EBEA, 0x07ED72, 0x07EDDE, 0x07EE90, 0x07EEEC, 0x07EF7C,
        0x07F052, 0x07F1A4, 0x07F202, 0x07F2F0, 0x07F438, 0x07F79E, 0x07F870, 0x07F928, 0x07F9E4, 0x07FA82, 0x07FB02, 0x07FB5E,
        0x07FBC4, 0x07FC0C, 0x07FCF0, 0x07FE06, 0x07FE5A, 0x07FF76, 0x07FFB4, 0x07FFDE, 0x080094, 0x0800EC, 0x080378, 0x0803E8,
        0x080462, 0x0804D8, 0x0805A4, 0x0806FA, 0x080768, 0x0807C0, 0x08083A, 0x08087C, 0x080970, 0x0809E4,
    };

    // For finding pieces that a wide picture should show and the game does not draw (VSAV_PIECES=1): for
    // every piece whose own place is inside the wide picture, a line saying whether its script put anything
    // on the frame's lists (to be counted up by whoever reads them: a step of the game's that draws
    // nothing is not a find).
    unsigned OnLists(M68K &c) {
        unsigned n = 0;
        for (int list = 0; list < 15; list++) {
            n += Ram8(Var(c, 0x79C5 + list));
        }
        return n;
    }
    void Seen(M68K &c, uint32_t piece, bool drawn) {
        int across = 0;
        const int side = WideMargin();
        if (!side || !Ram8(piece + kActive) || !AcrossInPicture(c, piece, across) || across < -side || across >= 0x180 + side) {
            return;
        }
        fprintf(stderr, "piece %d %d %d %d %d %d %d\n", Ram16(kStage) / 2 + 1, static_cast<int>((piece - kPieces) / kPieceSize),
                Ram8(piece + kScript), static_cast<int16_t>(Ram16(kCameraX)), Ram16(piece + kX), across, drawn);
    }

    // 07D240. A frame of the stage's pieces: every record in use runs its script
    void AllPieces(M68K &c) {
        static const bool check = getenv("VSAV_PIECES") != nullptr;
        c.a[6] = c.a[5] + (kPieces - 0xFF8000);
        SetRam8(Var(c, kPiecesLeft), kPieceRecords);
        do {
            const uint32_t piece = c.a[6] & 0xFFFFFF;
            if (Ram8(piece + kActive)) {
                c.a[0] = kScripts[Ram8(piece + kScript) % 58];
                c.d[0] = 0;
                const unsigned before = check ? OnLists(c) : 0;
                Call(c, c.a[0]);
                Spend(c, 90);
                if (check) {
                    Seen(c, piece, OnLists(c) != before);
                }
            }
            Spend(c, 60);
            c.a[6] += kPieceSize;
            SetRam8(Var(c, kPiecesLeft), Ram8(Var(c, kPiecesLeft)) - 1);
        } while (Ram8(Var(c, kPiecesLeft)));
    }
} // namespace

void RegisterPieces() {
    // (the last number is the time a call is charged: the game's own, as the check measured it)
    AddRewrite(0x015030, "Animate", Animate, kFreeFlags, 45);
    AddRewrite(0x015084, "SetAnimation", SetAnimation, kFreeFlags, 78);
    AddRewrite(0x07D372, "SetStageAnimation", [](M68K &c) { SetStageAnimation(c); }, kFreeFlags);
    AddRewrite(0x0170A6, "GiveBack", GiveBack, kFreeFlags, 158);
    AddRewrite(0x07D354, "PiecePlain", Plain, kFreeFlags, 130);
    AddRewrite(0x07D3A2, "PieceFlickering", Flickering, kFreeFlags, 177);
    AddRewrite(0x07D3F6, "PieceDancers", Dancers, kFreeFlags, 100);
    AddRewrite(0x07D4BA, "PieceLady", Lady, kFreeFlags, 166);
    AddRewrite(0x07D534, "PieceThrower", Thrower, kFreeFlags, 134);
    AddRewrite(0x07D240, "AllPieces", AllPieces, kFreeFlags);
    AddRewrite(0x07DE78, "Piece8ByTurns", ByTurns<kTurns8>, kFreeFlags, 60);
    AddRewrite(0x080A20, "Piece57ByTurns", ByTurns<kTurns57>, kFreeFlags, 60);
    AddRewrite(0x0809A6, "Piece56ByTurns", ByTurns<kTurns56>, kFreeFlags, 60);
    // (the frames that begin with a test of the camera: the script, the place, which side, where it goes on)
    AddRewrite(0x07F896, "Piece30InView", WhileInView<0x220, true, HookAt(0x07F89E)>, kFreeFlags, 30);
    AddRewrite(0x07FD4A, "Piece38InView", WhileInView<0x1E8, true, HookAt(0x07FD54)>, kFreeFlags, 30);
    AddRewrite(0x07FE26, "Piece39InView", WhileInView<0x1E0, true, HookAt(0x07FE2E)>, kFreeFlags, 30);
    AddRewrite(0x0808A6, "Piece55InView", WhileInView<0x1E8, true, HookAt(0x0808AE)>, kFreeFlags, 30);
    AddRewrite(0x07FB22, "Piece34InView", WhileInView<0x228, false, HookAt(0x07FB2A)>, kFreeFlags, 30);
    AddRewrite(0x0803B0, "Piece46InView", WhileLeftOf<0x278, HookAt(0x0803BA)>, kFreeFlags, 30);
    AddRewrite(0x080420, "Piece47InView", WhileLeftOf<0x1E0, HookAt(0x08042A)>, kFreeFlags, 30);
    AddRewrite(0x07F26A, "Piece26InView", FloaterFrame, kFreeFlags, 30);
    AddRewrite(0x07DF4C, "Piece10InView", RunnerFrame, kFreeFlags, 30);
}

} // namespace cps2recomp
