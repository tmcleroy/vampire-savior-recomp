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
    // the frame is done: no animation, no movement, nothing drawn (the lady above is one). In a wide picture the place is 64 further off, and a piece that is itself near the
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
    // --- pieces that work their own place out from the camera's ---
    //
    // Three scripts' pieces are in front of the stage or behind it by a rate of their own: each frame
    // their place is where they started and a part of the camera's travel from its start (the big thing
    // under the helicopter on the Tower of Arrogance is one). The camera's travel across is taken as
    // WideLag has it (stage.h), so that in a wide picture they keep step with the planes.
    int16_t TravelAcross() {
        return static_cast<int16_t>(WideLag(static_cast<int16_t>(Ram16(kCameraRecord + kLayerStartX) - Ram16(kCameraX))));
    }
    int16_t TravelUp() {
        return static_cast<int16_t>(Ram16(kCameraRecord + kLayerStartY) - Ram16(kCameraY));
    }
    constexpr uint32_t kStartedX = 0x40, kStartedY = 0x44; // words: where the piece started
    // (these pieces ride no scroll screen: their place is in the picture, counted from the board's left
    // edge. A wide picture's left edge is 64 left of that with the camera at its start and at the same
    // place with the camera at its left end: where the board's edge would be is that much to the side.)
    int16_t BoardEdge(uint32_t piece) {
        if (Ram8(piece + kIsRiding)) {
            return 0;
        }
        return static_cast<int16_t>(static_cast<int16_t>(Ram16(kCameraRecord + kLayerStartX) - Ram16(kCameraX)) - TravelAcross());
    }

    // 0803BA (script 46, Tower of Arrogance): a quarter of the camera's travel across the other way, and
    // all of its travel up
    void PlaceAQuarter(M68K &c) {
        const uint32_t piece = c.a[6] & 0xFFFFFF;
        SetRam16(piece + kX, static_cast<uint16_t>((TravelAcross() >> 2) + Ram16(piece + kStartedX) + BoardEdge(piece)));
        SetRam16(piece + kY, static_cast<uint16_t>(TravelUp() + Ram16(piece + kStartedY)));
        PieceDrawn(c);
    }
    // 08042A (script 47, Deserted Chateau): three quarters, both ways
    void PlaceThreeQuarters(M68K &c) {
        const uint32_t piece = c.a[6] & 0xFFFFFF;
        const int16_t across = TravelAcross(), up = TravelUp();
        SetRam16(piece + kX, static_cast<uint16_t>(across - (across >> 2) + Ram16(piece + kStartedX) + BoardEdge(piece)));
        SetRam16(piece + kY, static_cast<uint16_t>(up - (up >> 2) + Ram16(piece + kStartedY)));
        PieceDrawn(c);
    }
    // 080066 (script 43, Fetus of God): a sixteenth more than the camera's travel, both ways
    void PlaceASixteenthMore(M68K &c) {
        const uint32_t piece = c.a[6] & 0xFFFFFF;
        const int16_t across = TravelAcross(), up = TravelUp();
        SetRam16(piece + kX, static_cast<uint16_t>(across + (across >> 4) + Ram16(piece + kStartedX) + BoardEdge(piece)));
        SetLow16(c.d[1], up >> 4);
        SetLow16(c.d[0], up + (up >> 4) + Ram16(piece + kStartedY));
        SetRam16(piece + kY, static_cast<uint16_t>(c.d[0]));
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
    // --- more scripts, whole ---
    //
    // Small ones, most of them a plain piece with one thing of its own. `StepAt` is `Step` for a script
    // given by where it is and where its four steps are.
    struct Steps {
        uint32_t script, at[4];
    };
    int StepAt(M68K &c, const Steps &steps) {
        const int step = Ram8((c.a[6] & 0xFFFFFF) + kStep);
        SetLow8(c.d[0], step);
        SetLow16(c.d[1], steps.at[(step >> 1) & 3] - (steps.script + 0x0C));
        return step;
    }
    // The record that made the piece at A6, in A4
    uint32_t Maker(M68K &c) {
        c.a[4] = 0xFFFF0000u | Ram16((c.a[6] & 0xFFFFFF) + kMaker);
        return c.a[4] & 0xFFFFFF;
    }
    // A made-up number from 0 to 3 in D0's low byte or word (the game takes one or the other)
    void Chance(M68K &c, bool word) {
        Call(c, kRandom);
        word ? SetLow16(c.d[0], c.d[0] & 3) : SetLow8(c.d[0], c.d[0] & 3);
    }
    // 015036. The animation's next frame now, however long the one shown had left
    void NextFrame(M68K &c) {
        Call(c, 0x015036);
    }
    void StartAs(M68K &c, uint32_t animation) {
        SetRam8((c.a[6] & 0xFFFFFF) + kStep, Ram8((c.a[6] & 0xFFFFFF) + kStep) + 2);
        SetStageAnimation(c, animation);
    }
    void ByCamera(M68K &c) {
        Call(c, 0x0154CC); // (picture.cpp)
    }
    void IfInPicture(M68K &c) {
        Call(c, 0x01551A);
    }
    constexpr int32_t kRoundDecided = 0x107;     // from A5, a byte: not zero from a knock-out until the next round (the round is decided)
    constexpr int32_t kOddFrame = 0xB4;     // from A5, a byte: bit 0 turns over each frame (things drawn every other frame)
    constexpr uint32_t kCameraStep = 0x285; // the camera's record: the step of its own script (the train's start)
    constexpr uint32_t kSpare = 0x22;       // byte: the third of an animation frame's four bytes

    // 07FF76 (script 41, the train): goes through its animation once a round is decided (a knock-out), and is not there before
    void Script41(M68K &c) {
        static constexpr Steps kSteps = {0x07FF76, {0x07FF8A, 0x07FF9A, 0x07FFAE, 0x07FFAE}};
        const uint32_t piece = c.a[6] & 0xFFFFFF;
        switch (StepAt(c, kSteps)) {
        case 0:
            StartAs(c, Ram8(piece + kWhich));
            break;
        case 2:
            if (Ram8(Var(c, kRoundDecided))) {
                Animate(c);
                PieceDrawn(c);
            }
            break;
        default:
            GiveBack(c);
            break;
        }
    }

    // 07FBC4 (script 36, the train): there until the camera's own script has got to its step 6
    void Script36(M68K &c) {
        static constexpr Steps kSteps = {0x07FBC4, {0x07FBD8, 0x07FBE8, 0x07FC06, 0x07FC06}};
        const uint32_t piece = c.a[6] & 0xFFFFFF;
        switch (StepAt(c, kSteps)) {
        case 0:
            StartAs(c, Ram8(piece + kWhich));
            break;
        case 2:
            if (!Frozen(c)) {
                Animate(c);
                if (Ram8(kCameraRecord + (kCameraStep - 0x280)) == 6) {
                    SetRam8(piece + kStep, Ram8(piece + kStep) + 2);
                }
            }
            PieceDrawn(c);
            break;
        default:
            GiveBack(c);
            break;
        }
    }

    // 07FB5E (script 35, the train): one animation until then, the next after
    void Script35(M68K &c) {
        static constexpr Steps kSteps = {0x07FB5E, {0x07FB72, 0x07FB82, 0x07FBAC, 0x07FBBE}};
        const uint32_t piece = c.a[6] & 0xFFFFFF;
        switch (StepAt(c, kSteps)) {
        case 0:
            StartAs(c, Ram8(piece + kWhich));
            break;
        case 2:
            if (!Frozen(c)) {
                Animate(c);
                if (Ram8(kCameraRecord + (kCameraStep - 0x280)) == 6) {
                    SetLow8(c.d[0], Ram8(piece + kWhich) + 1);
                    StartAs(c, c.d[0] & 0xFF);
                }
            }
            ByCamera(c);
            break;
        case 4:
            if (!Frozen(c)) {
                Animate(c);
            }
            ByCamera(c);
            break;
        default:
            GiveBack(c);
            break;
        }
    }

    // 07DECE (script 9, Concrete Cave): at an event of its animation, one time in four, the animation again from its start
    void Script9(M68K &c) {
        static constexpr Steps kSteps = {0x07DECE, {0x07DEE2, 0x07DEEE, 0x07DF18, 0x07DF18}};
        const uint32_t piece = c.a[6] & 0xFFFFFF;
        switch (StepAt(c, kSteps)) {
        case 0:
            StartAs(c, 1);
            break;
        case 2:
            if (!Frozen(c)) {
                Animate(c);
                if (Ram8(piece + kEvent)) {
                    SetRam8(piece + kEvent, 0);
                    Chance(c, false);
                    if (!(c.d[0] & 0xFF)) {
                        SetStageAnimation(c, 1);
                    }
                }
            }
            ByCamera(c);
            break;
        default:
            GiveBack(c);
            break;
        }
    }

    // 07E10E (script 12, Concrete Cave): when its animation ends, another: number 2 one time in four, else 3
    void Script12(M68K &c) {
        static constexpr Steps kSteps = {0x07E10E, {0x07E122, 0x07E12E, 0x07E15C, 0x07E15C}};
        const uint32_t piece = c.a[6] & 0xFFFFFF;
        switch (StepAt(c, kSteps)) {
        case 0:
            StartAs(c, 3);
            break;
        case 2:
            if (!Frozen(c)) {
                Animate(c);
                if (Ram8(piece + kOver)) {
                    Chance(c, false);
                    SetStageAnimation(c, (c.d[0] & 0xFF) ? 3 : 2);
                }
            }
            ByCamera(c);
            break;
        default:
            GiveBack(c);
            break;
        }
    }

    // 080768 (script 52, Concrete Cave): waits for the event 2 of the piece that made it, then goes through its animation
    void Script52(M68K &c) {
        static constexpr Steps kSteps = {0x080768, {0x08077C, 0x080792, 0x0807A8, 0x0807BA}};
        const uint32_t piece = c.a[6] & 0xFFFFFF;
        switch (StepAt(c, kSteps)) {
        case 0:
            SetRam8(piece + 0x0D, 0xFF);
            SetRam8(piece + kDepth, 4);
            StartAs(c, 7);
            [[fallthrough]]; // (the game's own: the first frame follows at once)
        case 2:
            if (Ram8(Maker(c) + kEvent) == 2) {
                SetRam8(piece + kStep, Ram8(piece + kStep) + 2);
            }
            IfInPicture(c);
            break;
        case 4:
            if (!Frozen(c)) {
                Animate(c);
            }
            IfInPicture(c);
            break;
        default:
            GiveBack(c);
            break;
        }
    }

    // 0806FA (script 51, Concrete Cave): there while the spare byte of its maker's frame is not zero; at an
    // event, three times in four, another of three animations
    void Script51(M68K &c) {
        static constexpr Steps kSteps = {0x0806FA, {0x08070E, 0x080724, 0x080762, 0x080762}};
        const uint32_t piece = c.a[6] & 0xFFFFFF;
        switch (StepAt(c, kSteps)) {
        case 0:
            SetRam8(piece + 0x0D, 0xFF);
            SetRam8(piece + kDepth, 2);
            StartAs(c, 9);
            [[fallthrough]];
        case 2:
            if (!Ram8(Maker(c) + kSpare)) {
                break;
            }
            if (!Frozen(c)) {
                Animate(c);
                if (Ram8(piece + kEvent) & 0x80) {
                    SetRam8(piece + kEvent, 0);
                    Chance(c, false);
                    if (c.d[0] & 0xFF) {
                        SetStageAnimation(c, (c.d[0] & 0xFF) + 9);
                    }
                }
            }
            IfInPicture(c);
            break;
        default:
            GiveBack(c);
            break;
        }
    }

    // 07DDD8 (script 7, Feast of the Damned): its animation is by a word of its maker's (script 6's), and
    // changes when that does
    constexpr uint32_t kMakersChoice = 0x44, kChoiceTaken = 0x4C;
    void TakeChoice(M68K &c, uint32_t piece) {
        SetRam16(piece + kChoiceTaken, static_cast<uint16_t>(c.d[0]));
        SetStageAnimation(c, (c.d[0] + 0x0C + Ram8(piece + kWhich)) & 0xFF);
    }
    void Script7(M68K &c) {
        static constexpr Steps kSteps = {0x07DDD8, {0x07DDEC, 0x07DE0A, 0x07DE2C, 0x07DE2C}};
        const uint32_t piece = c.a[6] & 0xFFFFFF;
        switch (StepAt(c, kSteps)) {
        case 0:
            SetRam8(piece + kStep, 2);
            SetLow16(c.d[0], Ram16(Maker(c) + kMakersChoice));
            TakeChoice(c, piece);
            break;
        case 2:
            if (!Frozen(c)) {
                SetLow16(c.d[0], Ram16(Maker(c) + kMakersChoice));
                if ((c.d[0] & 0xFFFF) != Ram16(piece + kChoiceTaken)) {
                    TakeChoice(c, piece);
                }
                Animate(c);
            }
            ByCamera(c);
            break;
        default:
            GiveBack(c);
            break;
        }
    }

    // 07EE90 (script 21, Abaraya): its animation over, it waits (180 frames the first time, then 90) and
    // then, one time in four, goes through it again
    void Script21(M68K &c) {
        static constexpr Steps kSteps = {0x07EE90, {0x07EEA4, 0x07EEB6, 0x07EEE6, 0x07EEE6}};
        const uint32_t piece = c.a[6] & 0xFFFFFF;
        switch (StepAt(c, kSteps)) {
        case 0:
            SetRam16(piece + kTurn, 0xB4);
            StartAs(c, 1);
            break;
        case 2:
            if (!Frozen(c)) {
                bool goesOn = !Ram8(piece + kOver);
                if (!goesOn) {
                    SetRam16(piece + kTurn, static_cast<uint16_t>(Ram16(piece + kTurn) - 1));
                    if (Ram16(piece + kTurn) & 0x8000) {
                        SetRam16(piece + kTurn, 0x5A);
                        Chance(c, false);
                        goesOn = !(c.d[0] & 0xFF);
                    }
                }
                if (goesOn) {
                    Animate(c);
                }
            }
            ByCamera(c);
            break;
        default:
            GiveBack(c);
            break;
        }
    }

    // 07F1A4 (script 25, Vanity Paradise): as the candles of Feast of the Damned, and drawn every other frame
    void Script25(M68K &c) {
        static constexpr Steps kSteps = {0x07F1A4, {0x07F1B8, 0x07F1C8, 0x07F1FC, 0x07F1FC}};
        const uint32_t piece = c.a[6] & 0xFFFFFF;
        switch (StepAt(c, kSteps)) {
        case 0:
            StartAs(c, Ram8(piece + kWhich));
            break;
        case 2:
            if (!Frozen(c)) {
                if (Ram8(piece + kEvent)) {
                    SetRam8(piece + kEvent, 0);
                    Chance(c, true);
                    SetRam16(piece + kTimer, static_cast<uint16_t>(Ram16(piece + kTimer) + c.d[0]));
                }
                Animate(c);
            }
            if (Ram8(Var(c, kOddFrame)) & 1) {
                ByCamera(c);
            }
            break;
        default:
            GiveBack(c);
            break;
        }
    }

    // 080094 (script 44, Fetus of God): its animation's frames by their own time until a round is decided,
    // then one each frame
    void Script44(M68K &c) {
        static constexpr Steps kSteps = {0x080094, {0x0800A8, 0x0800B8, 0x0800D4, 0x0800E6}};
        const uint32_t piece = c.a[6] & 0xFFFFFF;
        switch (StepAt(c, kSteps)) {
        case 0:
            StartAs(c, Ram8(piece + kWhich));
            break;
        case 2:
            if (!Frozen(c)) {
                if (Ram8(Var(c, kRoundDecided))) {
                    SetRam8(piece + kStep, Ram8(piece + kStep) + 2);
                }
                Animate(c);
            }
            ByCamera(c);
            break;
        case 4:
            if (!Frozen(c)) {
                NextFrame(c);
            }
            ByCamera(c);
            break;
        default:
            GiveBack(c);
            break;
        }
    }

    // 07FA82 (script 33, Green Scream): takes its place on the stage from its maker and its animation by
    // a word of its maker's, changing when that does
    void TakeMakersWord(M68K &c, uint32_t piece) {
        SetRam16(piece + 0x40, static_cast<uint16_t>(c.d[0]));
        SetLow16(c.d[0], c.d[0] + 0x0A);
        SetStageAnimation(c);
    }
    void Script33(M68K &c) {
        static constexpr Steps kSteps = {0x07FA82, {0x07FA96, 0x07FADA, 0x07FAFC, 0x07FAFC}};
        const uint32_t piece = c.a[6] & 0xFFFFFF;
        switch (StepAt(c, kSteps)) {
        case 0: {
            SetRam8(piece + kStep, 2);
            SetRam8(piece + kDepth, 2);
            SetRam16(piece + kX, 0x258);
            SetRam16(piece + kY, 0x70);
            const uint32_t maker = Maker(c);
            SetRam8(piece + kFacesRight, Ram8(maker + kFacesRight));
            SetRam8(piece + kIsRiding, Ram8(maker + kIsRiding));
            SetRam16(piece + kRow, Ram16(maker + kRow));
            SetRam32(piece + kPlaces, Ram32(maker + kPlaces));
            SetLow16(c.d[0], Ram16(maker + 0x40));
            TakeMakersWord(c, piece);
            break;
        }
        case 2:
            if (!Frozen(c)) {
                Animate(c);
                SetLow16(c.d[0], Ram16(Maker(c) + 0x40));
                if ((c.d[0] & 0xFFFF) != Ram16(piece + 0x40)) {
                    TakeMakersWord(c, piece);
                }
            }
            PieceDrawn(c);
            break;
        default:
            GiveBack(c);
            break;
        }
    }

    // 0807C0 (script 53, Tower of Arrogance): goes where its maker is, with the animation the spare byte
    // of its maker's frame names; drawn every other frame
    void Script53(M68K &c) {
        static constexpr Steps kSteps = {0x0807C0, {0x0807D4, 0x0807F4, 0x080834, 0x080834}};
        const uint32_t piece = c.a[6] & 0xFFFFFF;
        switch (StepAt(c, kSteps)) {
        case 0:
            SetRam8(piece + 0x0D, 0xFF);
            SetRam8(piece + kDepth, 2);
            SetRam8(piece + kHalf, Ram8(Maker(c) + kSpare));
            StartAs(c, 0x18);
            [[fallthrough]];
        case 2:
            if (!(Ram8(Var(c, kOddFrame)) & 1)) {
                break;
            }
            if (!Frozen(c)) {
                const uint32_t maker = Maker(c);
                SetRam16(piece + kX, Ram16(maker + kX));
                SetRam16(piece + kY, Ram16(maker + kY));
                SetLow8(c.d[0], Ram8(maker + kSpare));
                if ((c.d[0] & 0xFF) != Ram8(piece + kHalf)) {
                    SetRam8(piece + kHalf, c.d[0]);
                    SetStageAnimation(c);
                }
                Animate(c);
            }
            ByCamera(c);
            break;
        default:
            GiveBack(c);
            break;
        }
    }

    // A step within a step: some scripts' frames go by the record's second step byte (kInner), through a
    // table of their own at `table`
    constexpr uint32_t kInner = 0x05;
    int InnerStep(M68K &c, uint32_t table, uint32_t first, uint32_t second) {
        const int step = Ram8((c.a[6] & 0xFFFFFF) + kInner);
        SetLow8(c.d[0], step);
        SetLow16(c.d[1], (step ? second : first) - table);
        return step;
    }
    void NextStep(uint32_t piece) {
        SetRam8(piece + kStep, Ram8(piece + kStep) + 2);
    }
    // A free record for a new piece, in A4; false when there is none
    bool Take(M68K &c) {
        const bool any = Ram8(Var(c, kFreePiecesCount)) != 0;
        Call(c, kTakePiece);
        return any;
    }
    constexpr uint32_t kMoveBySpeed = 0x015466; // the piece's place moved by its speed, its speed by its pull
    constexpr uint32_t kSpeedX = 0x40, kSpeedY = 0x44, kPullY = 0x4C;

    // 08083A (script 54, Vanity Paradise): goes where the piece that made it is
    void Script54(M68K &c) {
        const uint32_t piece = c.a[6] & 0xFFFFFF, maker = Maker(c);
        const int step = Ram8(piece + kStep);
        SetLow8(c.d[0], step);
        static constexpr uint32_t kAt[4] = {0x080852, 0x080864, 0x080876, 0x080876};
        SetLow16(c.d[1], kAt[(step >> 1) & 3] - 0x08084A);
        switch (step) {
        case 0:
            SetRam8(piece + 0x0D, 0xFF);
            SetLow8(c.d[0], Ram8(piece + kWhich));
            NextStep(piece);
            SetStageAnimation(c);
            [[fallthrough]];
        case 2:
            SetRam16(piece + kX, Ram16(maker + kX));
            SetRam16(piece + kY, Ram16(maker + kY));
            ByCamera(c);
            break;
        default:
            GiveBack(c);
            break;
        }
    }

    // 07EEEC (script 22, Abaraya): once a round is decided, a frame of its animation every frame, and at
    // each frame with an event it lets a piece go (script 23), told how many frames have gone by
    void Script22(M68K &c) {
        static constexpr Steps kSteps = {0x07EEEC, {0x07EF00, 0x07EF0C, 0x07EF22, 0x07EF76}};
        const uint32_t piece = c.a[6] & 0xFFFFFF;
        switch (StepAt(c, kSteps)) {
        case 0:
            StartAs(c, 7);
            break;
        case 2:
            if (Ram8(Var(c, kRoundDecided))) {
                NextStep(piece);
            }
            Animate(c);
            ByCamera(c);
            break;
        case 4:
            if (!Frozen(c)) {
                SetRam16(piece + kTurn, static_cast<uint16_t>(Ram16(piece + kTurn) + 1));
                if (Ram8(piece + kEvent) && Take(c)) {
                    const uint32_t made = c.a[4] & 0xFFFFFF;
                    SetRam32(made, 0x01001700);
                    SetRam16(made + kX, 0x258);
                    SetRam16(made + kY, 0xB0);
                    SetRam8(made + kIsRiding, 8);
                    SetRam16(made + kRow, 0xFFF4);
                    SetRam16(made + kPlaces + 2, 0xE000);
                    SetRam16(made + kPlaces, 0x2000);
                    SetRam16(made + kTurn, Ram16(piece + kTurn));
                }
                NextFrame(c);
            }
            ByCamera(c);
            break;
        default:
            GiveBack(c);
            break;
        }
    }

    // 07E162 (script 13, Tower of Arrogance): bobs up and down (its speed up or down changing by a pull
    // that turns about every 256 frames), and makes a piece that goes with it (script 53)
    void Script13(M68K &c) {
        static constexpr Steps kSteps = {0x07E162, {0x07E176, 0x07E1D6, 0x07E208, 0x07E208}};
        const uint32_t piece = c.a[6] & 0xFFFFFF;
        switch (StepAt(c, kSteps)) {
        case 0:
            NextStep(piece);
            SetRam16(piece + kTurn, 0xFF);
            SetRam32(piece + kSpeedY, 0xFFFFE000);
            SetRam32(piece + kPullY, 0x40);
            if (Take(c)) {
                const uint32_t made = c.a[4] & 0xFFFFFF;
                SetRam32(made, 0x01003500);
                SetRam16(made + kX, Ram16(piece + kX));
                SetRam16(made + kY, Ram16(piece + kY));
                SetRam8(made + kFacesRight, Ram8(piece + kFacesRight));
                SetRam32(made + kPlaces, Ram32(piece + kPlaces));
                SetRam8(made + kIsRiding, Ram8(piece + kIsRiding));
                SetRam16(made + kRow, Ram16(piece + kRow));
                SetRam32(made + 0x28, 0x02608000);
                SetRam16(made + kMaker, static_cast<uint16_t>(c.a[6]));
            }
            SetStageAnimation(c, 0x15);
            break;
        case 2:
            if (!Frozen(c)) {
                Animate(c);
                SetRam32(piece + kY, Ram32(piece + kY) + Ram32(piece + kSpeedY));
                c.d[0] = Ram32(piece + kPullY);
                SetRam32(piece + kSpeedY, Ram32(piece + kSpeedY) + c.d[0]);
                SetRam16(piece + kTurn, static_cast<uint16_t>(Ram16(piece + kTurn) - 1));
                if (Ram16(piece + kTurn) & 0x8000) {
                    SetRam16(piece + kTurn, 0xFF);
                    SetRam32(piece + kPullY, 0u - Ram32(piece + kPullY));
                }
            }
            ByCamera(c);
            break;
        default:
            GiveBack(c);
            break;
        }
    }

    // 07E5A8 (script 15, Red Thirst): nine animations in turn, each for its own time
    constexpr uint16_t kScript15Times[9] = {288, 288, 288, 288, 288, 288, 288, 384, 256};
    void Script15(M68K &c) {
        static constexpr Steps kSteps = {0x07E5A8, {0x07E5BC, 0x07E5D6, 0x07E604, 0x07E604}};
        const uint32_t piece = c.a[6] & 0xFFFFFF;
        switch (StepAt(c, kSteps)) {
        case 0:
            NextStep(piece);
            SetRam8(piece + kHalf, 0);
            SetStageAnimation(c, Ram8(piece + kWhich));
            break;
        case 2:
            if (!Frozen(c)) {
                SetRam16(piece + kTurn, static_cast<uint16_t>(Ram16(piece + kTurn) - 1));
                if (Ram16(piece + kTurn) & 0x8000) {
                    SetRam8(piece + kHalf, Ram8(piece + kHalf) + 1 >= 9 ? 0 : Ram8(piece + kHalf) + 1);
                    SetRam16(piece + kTurn, kScript15Times[Ram8(piece + kHalf)]);
                    SetStageAnimation(c, (Ram8(piece + kWhich) + Ram8(piece + kHalf)) & 0xFF);
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

    // 07F9E4 (script 32, Green Scream): makes a piece (script 33) and tells it, in a word, which of four
    // animations: another by chance at each event of its own; when the byte at 10B(A5) is set, the fifth
    // for good
    constexpr int32_t kScript32Ends = 0x10B; // from A5
    void Script32(M68K &c) {
        static constexpr Steps kSteps = {0x07F9E4, {0x07F9F8, 0x07FA1C, 0x07FA6A, 0x07FA7C}};
        const uint32_t piece = c.a[6] & 0xFFFFFF;
        switch (StepAt(c, kSteps)) {
        case 0:
            NextStep(piece);
            SetRam16(piece + 0x40, 0);
            if (Take(c)) {
                SetRam32(c.a[4] & 0xFFFFFF, 0x01002100);
                SetRam16((c.a[4] & 0xFFFFFF) + kMaker, static_cast<uint16_t>(c.a[6]));
            }
            SetStageAnimation(c, 5);
            break;
        case 2:
            if (!Frozen(c)) {
                if (Ram8(Var(c, kScript32Ends))) {
                    NextStep(piece);
                    SetRam16(piece + 0x40, 4);
                    SetStageAnimation(c, 9);
                } else {
                    Animate(c);
                    if (Ram8(piece + kEvent)) {
                        Chance(c, true);
                        if ((c.d[0] & 0xFFFF) != Ram16(piece + 0x40)) {
                            SetRam16(piece + 0x40, static_cast<uint16_t>(c.d[0]));
                            SetLow16(c.d[0], c.d[0] + 5);
                            SetStageAnimation(c);
                        }
                    }
                }
            }
            PieceDrawn(c);
            break;
        case 4:
            if (!Frozen(c)) {
                Animate(c);
            }
            PieceDrawn(c);
            break;
        default:
            GiveBack(c);
            break;
        }
    }

    // 07F79E (script 29, War Agony): a thing let go by another piece (script 28). It waits, not drawn,
    // until its maker's byte kHalf is set; then it goes by its speed for as long as that stays set; then
    // it is put back where it started, to wait again.
    void Script29(M68K &c) {
        static constexpr Steps kSteps = {0x07F79E, {0x07F7B2, 0x07F808, 0x07F86A, 0x07F86A}};
        static constexpr uint32_t kKept[6][2] = {{0x10, 0x50}, {0x14, 0x54}, {0x40, 0x58}, {0x44, 0x5C}, {0x48, 0x60}, {0x4C, 0x64}};
        const uint32_t piece = c.a[6] & 0xFFFFFF;
        switch (StepAt(c, kSteps)) {
        case 0: {
            NextStep(piece);
            const uint32_t maker = Maker(c);
            SetRam8(piece + kFacesRight, Ram8(maker + kFacesRight));
            SetRam8(piece + kDepth, Ram8(maker + kDepth));
            SetRam32(piece + kPlaces, Ram32(maker + kPlaces));
            SetRam8(piece + kIsRiding, Ram8(maker + kIsRiding));
            SetRam16(piece + kRow, Ram16(maker + kRow));
            for (const auto &kept : kKept) {
                SetRam32(piece + kept[1], Ram32(piece + kept[0]));
            }
            SetStageAnimation(c, Ram8(piece + kWhich));
            break;
        }
        case 2: {
            const uint32_t maker = Maker(c);
            if (InnerStep(c, 0x07F818, 0x07F81C, 0x07F828) == 0) {
                if (Ram8(maker + kHalf)) {
                    SetRam8(piece + kInner, 2);
                }
                break;
            }
            if (!Frozen(c)) {
                if (!Ram8(maker + kHalf)) {
                    SetRam8(piece + kInner, 0);
                    for (const auto &kept : kKept) {
                        SetRam32(piece + kept[0], Ram32(piece + kept[1]));
                    }
                    break;
                }
                Call(c, kMoveBySpeed);
            }
            PieceDrawn(c);
            break;
        }
        default:
            GiveBack(c);
            break;
        }
    }

    // 07F870 (script 30, Forever Torment): there only with the camera right of a place. Every 180 frames,
    // one time in four, it shakes (a pixel up and across, then back, every fourth frame) through its
    // animation's events
    void Script30(M68K &c) {
        static constexpr Steps kSteps = {0x07F870, {0x07F884, 0x07F896, 0x07F922, 0x07F922}};
        const uint32_t piece = c.a[6] & 0xFFFFFF;
        switch (StepAt(c, kSteps)) {
        case 0:
            SetRam16(piece + kTurn, 0xB4);
            StartAs(c, 0x0E);
            break;
        case 2:
            if (OutOfView(c, 0x220, true)) {
                break;
            }
            if (!Frozen(c)) {
                if (InnerStep(c, 0x07F8B8, 0x07F8BC, 0x07F8EA) == 0) {
                    SetRam16(piece + kTurn, static_cast<uint16_t>(Ram16(piece + kTurn) - 1));
                    if (Ram16(piece + kTurn) & 0x8000) {
                        SetRam16(piece + kTurn, 0xB4);
                        Chance(c, true);
                        if (!(c.d[0] & 0xFFFF)) {
                            SetRam8(piece + kInner, 2);
                            SetRam8(piece + kHalf, 0);
                            SetRam8(piece + 0x40, 2);
                            SetRam16(piece + 0x44, 1);
                        }
                    }
                } else {
                    SetRam8(piece + 0x40, Ram8(piece + 0x40) - 1);
                    if (Ram8(piece + 0x40)) {
                        SetLow16(c.d[0], Ram16(piece + 0x44));
                        SetRam16(piece + kX, static_cast<uint16_t>(Ram16(piece + kX) + c.d[0]));
                        SetRam16(piece + kY, static_cast<uint16_t>(Ram16(piece + kY) + c.d[0]));
                        SetRam8(piece + 0x40, 4);
                        SetRam16(piece + 0x44, static_cast<uint16_t>(0u - Ram16(piece + 0x44)));
                    }
                    Animate(c);
                    if (Ram8(piece + kEvent)) {
                        SetRam8(piece + kEvent, 0);
                        SetRam8(piece + kHalf, Ram8(piece + kHalf) - 1);
                        if (Ram8(piece + kHalf) & 0x80) {
                            SetRam8(piece + kInner, 0);
                        }
                    }
                }
            }
            PieceDrawn(c);
            break;
        default:
            GiveBack(c);
            break;
        }
    }

    // 07F928 (script 31, Forever Torment): walks to and fro within 128 of where it started, turning
    // about at each end, for a time; then goes through another animation standing; then walks again for
    // a time made up anew
    void Script31(M68K &c) {
        static constexpr Steps kSteps = {0x07F928, {0x07F93C, 0x07F95C, 0x07F9DE, 0x07F9DE}};
        const uint32_t piece = c.a[6] & 0xFFFFFF;
        switch (StepAt(c, kSteps)) {
        case 0:
            SetRam16(piece + kTurn, 0x60);
            SetRam32(piece + kSpeedX, 0xFFFDC000);
            SetRam16(piece + 0x48, Ram16(piece + kX));
            StartAs(c, 0x0F);
            break;
        case 2:
            if (!Frozen(c)) {
                if (InnerStep(c, 0x07F97A, 0x07F97E, 0x07F9B2) == 0) {
                    SetRam16(piece + kTurn, static_cast<uint16_t>(Ram16(piece + kTurn) - 1));
                    if (Ram16(piece + kTurn) & 0x8000) {
                        SetRam8(piece + kInner, 2);
                        SetStageAnimation(c, 0x10);
                    } else {
                        c.d[0] = Ram32(piece + kSpeedX);
                        SetRam32(piece + kX, Ram32(piece + kX) + c.d[0]);
                        SetLow16(c.d[0], Ram16(piece + kX) - Ram16(piece + 0x48));
                        if ((c.d[0] & 0xFFFF) > 0x80) {
                            SetRam32(piece + kSpeedX, 0u - Ram32(piece + kSpeedX));
                            SetRam8(piece + kFacesRight, Ram8(piece + kFacesRight) ^ 1);
                        }
                    }
                } else if (Ram8(piece + kOver) & 0x80) {
                    c.d[0] = 0;
                    SetRam8(piece + kEvent, 0);
                    SetRam8(piece + kInner, 0);
                    Call(c, kRandom);
                    SetLow16(c.d[0], (c.d[0] & 0x7F) + 0x40);
                    SetRam16(piece + kTurn, static_cast<uint16_t>(c.d[0]));
                    SetStageAnimation(c, 0x0F);
                }
                Animate(c);
            }
            ByCamera(c);
            break;
        default:
            GiveBack(c);
            break;
        }
    }

    // 07FE06 (script 39, the train): there only with the camera right of a place; goes through its
    // animation once and is then done; drawn every other frame
    void Script39(M68K &c) {
        static constexpr Steps kSteps = {0x07FE06, {0x07FE1A, 0x07FE26, 0x07FE54, 0x07FE54}};
        const uint32_t piece = c.a[6] & 0xFFFFFF;
        switch (StepAt(c, kSteps)) {
        case 0:
            StartAs(c, 0x22);
            break;
        case 2:
            if (OutOfView(c, 0x1E0, true)) {
                break;
            }
            if (!Frozen(c)) {
                Animate(c);
                if (Ram8(piece + kOver) & 0x80) {
                    NextStep(piece);
                }
            }
            if (Ram8(Var(c, kOddFrame)) & 1) {
                PieceDrawn(c);
            }
            break;
        default:
            GiveBack(c);
            break;
        }
    }

    // 07FB02 (script 34, Green Scream): there only with the camera left of a place; at an event of its
    // animation, one time in four, it goes to the animation the event names
    void Script34(M68K &c) {
        static constexpr Steps kSteps = {0x07FB02, {0x07FB16, 0x07FB22, 0x07FB58, 0x07FB58}};
        const uint32_t piece = c.a[6] & 0xFFFFFF;
        switch (StepAt(c, kSteps)) {
        case 0:
            StartAs(c, 0x10);
            break;
        case 2:
            if (OutOfView(c, 0x228, false)) {
                break;
            }
            if (!Frozen(c)) {
                Animate(c);
                SetLow8(c.d[6], Ram8(piece + kEvent));
                if (c.d[6] & 0xFF) {
                    Chance(c, true);
                    if (!(c.d[0] & 0xFFFF)) {
                        SetLow8(c.d[0], c.d[6]);
                        SetStageAnimation(c);
                    }
                }
            }
            ByCamera(c);
            break;
        default:
            GiveBack(c);
            break;
        }
    }

    // 07DF1E (script 10, Concrete Cave): with the camera left of a place it goes left 48 at each end of
    // its animation, as far as 192; with the camera right of the place it is put back at its start
    void Script10(M68K &c) {
        static constexpr Steps kSteps = {0x07DF1E, {0x07DF32, 0x07DF4C, 0x07DF96, 0x07DF96}};
        const uint32_t piece = c.a[6] & 0xFFFFFF;
        switch (StepAt(c, kSteps)) {
        case 0:
            StartAs(c, 6);
            SetRam32(piece + kKeptFrame, Ram32(piece + kFrame));
            SetRam32(piece + kKeptFrame2, Ram32(piece + kTimer));
            break;
        case 2:
            if (OutOfView(c, 0x1BF, false)) {
                SetRam16(piece + kX, 0x188);
                SetRam32(piece + kFrame, Ram32(piece + kKeptFrame));
                SetRam32(piece + kTimer, Ram32(piece + kKeptFrame2));
                break;
            }
            if (!Frozen(c)) {
                Animate(c);
                if (Ram8(piece + kOver)) {
                    SetRam16(piece + kX, static_cast<uint16_t>(Ram16(piece + kX) - 0x30));
                    if (Ram16(piece + kX) < 0xC0) {
                        SetRam16(piece + kX, 0xC0);
                        break; // (and it is not drawn that frame)
                    }
                }
            }
            PieceDrawn(c);
            break;
        default:
            GiveBack(c);
            break;
        }
    }

    // 080378, 0803E8 (scripts 46 and 47): the pieces that work their place out from the camera's (above),
    // there only with the camera left of a place
    void SelfPlaced(M68K &c, const Steps &steps, uint32_t animation, uint16_t place, void (*put)(M68K &)) {
        const uint32_t piece = c.a[6] & 0xFFFFFF;
        switch (StepAt(c, steps)) {
        case 0:
            SetRam32(piece + kStartedX, Ram32(piece + kX));
            SetRam32(piece + kStartedY, Ram32(piece + kY));
            StartAs(c, animation);
            break;
        case 2:
            if (!Frozen(c)) {
                Animate(c);
            }
            SetLow16(c.d[1], Ram16(kCameraX));
            if (!OutOfView(c, place, false, true)) {
                put(c);
            }
            break;
        default:
            GiveBack(c);
            break;
        }
    }
    void Script46(M68K &c) {
        static constexpr Steps kSteps = {0x080378, {0x08038C, 0x0803A4, 0x0803E2, 0x0803E2}};
        SelfPlaced(c, kSteps, 0x10, 0x278, PlaceAQuarter);
    }
    void Script47(M68K &c) {
        static constexpr Steps kSteps = {0x0803E8, {0x0803FC, 0x080414, 0x08045C, 0x08045C}};
        SelfPlaced(c, kSteps, 3, 0x1E0, PlaceThreeQuarters);
    }

    // 07DE32, 0809E4, 080970 (scripts 8, 57 and 56): the pieces that are two by turns (above), whole
    constexpr int32_t kScript8Other = 0x90; // from A5, a word: at 8, the right one's picture is another
    void Script8(M68K &c) {
        static constexpr Steps kSteps = {0x07DE32, {0x07DE46, 0x07DE78, 0x07DEC8, 0x07DEC8}};
        const uint32_t piece = c.a[6] & 0xFFFFFF;
        switch (StepAt(c, kSteps)) {
        case 0:
            SetRam8(piece + kHalf, 1);
            StartAs(c, 0);
            SetRam32(piece + kKeptFrame, Ram32(piece + kFrame));
            SetStageAnimation(c, Ram16(Var(c, kScript8Other)) == 8 ? 5 : 4);
            SetRam32(piece + kKeptFrame2, Ram32(piece + kFrame));
            break;
        case 2:
            TurnsFrame(c, kTurns8);
            break;
        default:
            GiveBack(c);
            break;
        }
    }
    void Script57(M68K &c) {
        static constexpr Steps kSteps = {0x0809E4, {0x0809F8, 0x080A20, 0x080A7A, 0x080A7A}};
        const uint32_t piece = c.a[6] & 0xFFFFFF;
        switch (StepAt(c, kSteps)) {
        case 0:
            SetRam8(piece + kHalf, 1);
            StartAs(c, 0);
            SetRam32(piece + kKeptFrame, Ram32(piece + kFrame));
            SetStageAnimation(c, 2);
            SetRam32(piece + kKeptFrame2, Ram32(piece + kFrame));
            break;
        case 2:
            TurnsFrame(c, kTurns57);
            break;
        default:
            GiveBack(c);
            break;
        }
    }
    void Script56(M68K &c) {
        static constexpr Steps kSteps = {0x080970, {0x080984, 0x0809A6, 0x0809DE, 0x0809DE}};
        const uint32_t piece = c.a[6] & 0xFFFFFF;
        switch (StepAt(c, kSteps)) {
        case 0:
            SetRam16(piece + 0x40, Ram16(piece + kX));
            SetRam16(piece + 0x42, Ram16(piece + kY)); // (the other place across is given as the height)
            SetRam16(piece + kY, 0x38);
            StartAs(c, Ram8(piece + kWhich));
            break;
        case 2:
            TurnsFrame(c, kTurns56);
            break;
        default:
            GiveBack(c);
            break;
        }
    }

    template <size_t kCount>
    int InnerStepOf(M68K &c, uint32_t table, const uint32_t (&at)[kCount]) {
        const int step = Ram8((c.a[6] & 0xFFFFFF) + kInner);
        SetLow8(c.d[0], step);
        SetLow16(c.d[1], at[static_cast<size_t>(step >> 1) % kCount] - table);
        return step;
    }
    // One less in a byte of the record; true when that takes it below nothing
    bool CountedDown(uint32_t at) {
        SetRam8(at, Ram8(at) - 1);
        return (Ram8(at) & 0x80) != 0;
    }

    // 07D59C (script 5, Feast of the Damned): the thing the thrower lets go. It flies off to the left and
    // up, more steeply as it goes, turns, comes back across from the left, waits for the event 2 of the
    // piece that made it, and goes on a little before it is done.
    void Script5(M68K &c) {
        static constexpr Steps kSteps = {0x07D59C, {0x07D5B0, 0x07D600, 0x07D70A, 0x07D70A}};
        static constexpr uint32_t kInnerAt[6] = {0x07D62A, 0x07D656, 0x07D68A, 0x07D6D4, 0x07D6E6, 0x07D6FE};
        constexpr uint32_t kPullX = 0x48;
        const uint32_t piece = c.a[6] & 0xFFFFFF;
        switch (StepAt(c, kSteps)) {
        case 0:
            SetRam16(piece + kX, 0x238);
            SetRam16(piece + kY, 0x80);
            SetRam8(piece + kFacesRight, 1);
            SetRam8(piece + kDepth, 2);
            SetRam8(piece + kTurn, 0x1F);
            SetRam8(piece + kTurn + 1, 0x0C);
            SetRam32(piece + kSpeedX, 0xFFFD0000);
            SetRam32(piece + kPullX, 0x1000);
            SetRam32(piece + kSpeedY, 0x20000);
            SetRam32(piece + kPullY, 0xFFFFD800);
            StartAs(c, 0x0B);
            break;
        case 2:
            if (!Frozen(c)) {
                switch (InnerStepOf(c, 0x07D61E, kInnerAt)) {
                case 0: // (off to the left and up)
                    Call(c, kMoveBySpeed);
                    if (CountedDown(piece + kTurn + 1)) {
                        SetRam8(piece + kTurn + 1, 0x0C);
                        SetRam32(piece + kSpeedY, Ram32(piece + kSpeedY) + 0x20000);
                    }
                    if (CountedDown(piece + kTurn)) {
                        SetRam8(piece + kInner, 2);
                        SetRam8(piece + kTurn, 0x1F);
                    }
                    break;
                case 2: // (turning)
                    Call(c, kMoveBySpeed);
                    if (!(Ram16(piece + kSpeedX) & 0x8000)) {
                        SetRam8(piece + kFacesRight, 0);
                    }
                    if (CountedDown(piece + kTurn)) {
                        SetRam8(piece + kInner, 4);
                        SetRam8(piece + kTurn, 0x3F);
                        SetRam32(piece + kPullX, 0x6000);
                        SetRam32(piece + kPullY, 0x3000);
                    }
                    break;
                case 4: // (out of sight; then put at the left, high up, to come back)
                    Call(c, kMoveBySpeed);
                    if (CountedDown(piece + kTurn)) {
                        SetRam8(piece + kInner, 6);
                        SetRam8(piece + kTurn, 0x3F);
                        SetRam8(piece + kFacesRight, 0);
                        SetRam16(piece + kX, 0xA8);
                        SetRam16(piece + kY, 0x138);
                        SetRam32(piece + kSpeedX, 0xC0000);
                        SetRam32(piece + kSpeedY, 0xFFFA0000);
                        SetRam32(piece + kPullX, 0xFFFFD000);
                        SetRam32(piece + kPullY, 0x1800);
                    }
                    break;
                case 6:
                    if (Ram8(Maker(c) + kEvent) == 2) {
                        SetRam8(piece + kInner, 8);
                    }
                    break;
                case 8:
                    Call(c, kMoveBySpeed);
                    if (CountedDown(piece + kTurn)) {
                        SetRam8(piece + kInner, 10);
                        SetRam8(piece + kTurn, 0x14);
                    }
                    break;
                default:
                    if (CountedDown(piece + kTurn)) {
                        NextStep(piece);
                    }
                    break;
                }
                Animate(c);
            }
            IfInPicture(c);
            break;
        default:
            GiveBack(c);
            break;
        }
    }

    // 07F202 (script 26, Vanity Paradise): the floating things. Each goes to and fro across (turning
    // about every 128 frames) and up and down by two tables of sines; four kinds, by kWhich
    constexpr uint32_t kSinesA = 0x0B7640, kSinesB = 0x0B7940;
    struct Floater {
        uint8_t animation;
        uint32_t speed;
        uint8_t turnsIn, phaseB, phaseA;
    };
    constexpr Floater kFloaters[4] = {{0x18, 0x00010000, 0x00, 0x08, 0x00}, {0x18, 0x0000C000, 0x40, 0x0C, 0x08},
                                      {0x19, 0xFFFF4000, 0x20, 0x00, 0x10}, {0x1A, 0x00010000, 0x60, 0x14, 0x18}};
    void Script26(M68K &c) {
        static constexpr Steps kSteps = {0x07F202, {0x07F216, 0x07F26A, 0x07F2EA, 0x07F2EA}};
        const uint32_t piece = c.a[6] & 0xFFFFFF;
        switch (StepAt(c, kSteps)) {
        case 0: {
            NextStep(piece);
            SetRam16(piece + 0x50, Ram16(piece + kY));
            const Floater &kind = kFloaters[Ram8(piece + kWhich) & 3];
            SetLow8(c.d[6], kind.animation);
            SetRam32(piece + kSpeedX, kind.speed);
            SetRam8(piece + kTurn, kind.turnsIn);
            SetRam8(piece + kTurn + 1, kind.phaseB);
            SetRam8(piece + kHalf, kind.phaseA);
            SetStageAnimation(c, kind.animation);
            break;
        }
        case 2: {
            // (there only with the camera right of the record's own place)
            const uint16_t place = Ram16(piece + 0x2A);
            SetLow16(c.d[0], place);
            if (OutOfView(c, static_cast<uint16_t>(place + 1), true)) {
                break;
            }
            if (!Frozen(c)) {
                c.d[0] = Ram32(piece + kSpeedX);
                SetRam32(piece + kX, Ram32(piece + kX) + c.d[0]);
                if (CountedDown(piece + kTurn)) {
                    SetRam8(piece + kTurn, 0x7F);
                    SetRam32(piece + kSpeedX, 0u - Ram32(piece + kSpeedX));
                    SetRam8(piece + kFacesRight, Ram8(piece + kFacesRight) ^ 1);
                }
                auto sine = [&](uint32_t table, uint32_t phase) {
                    SetRam8(piece + phase, (Ram8(piece + phase) + 1) & 0x1F);
                    return static_cast<int16_t>(Mem16(c, table + 2 + Ram8(piece + phase) * 4u)) >> 8;
                };
                c.a[0] = kSinesB;
                const int a = sine(kSinesA, kHalf), b = sine(kSinesB, kTurn + 1);
                SetLow16(c.d[1], b);
                SetLow16(c.d[0], a + Ram16(piece + 0x50) + b);
                SetRam16(piece + kY, static_cast<uint16_t>(c.d[0]));
                Animate(c);
            }
            PieceDrawn(c);
            break;
        }
        default:
            GiveBack(c);
            break;
        }
    }

    // 07FC0C (script 37, the train): bounces. It falls back to the height it started at and is thrown up
    // again, by the next of four throws; before the byte at 12C(A5) is 1 with one animation, after with
    // the next. (The kind 3 starts at once; the others sixteen frames later.)
    constexpr int32_t kScript37Changes = 0x12C; // from A5
    constexpr uint32_t kScript37Throws[4][2] = {{0x10000, 0xFFFFE000}, {0x10000, 0xFFFFD000}, {0x10000, 0xFFFFD000}, {0x10000, 0xFFFFD000}};
    void Thrown37(M68K &c, uint32_t piece) {
        c.d[0] = Ram8(piece + kTurn + 1);
        SetRam32(piece + kSpeedY, kScript37Throws[(c.d[0] >> 3) & 3][0]);
        SetRam32(piece + kPullY, kScript37Throws[(c.d[0] >> 3) & 3][1]);
    }
    void Script37(M68K &c) {
        static constexpr Steps kSteps = {0x07FC0C, {0x07FC20, 0x07FC56, 0x07FC6E, 0x07FCB6}};
        const uint32_t piece = c.a[6] & 0xFFFFFF;
        const int step = StepAt(c, kSteps);
        switch (step) {
        case 0:
            NextStep(piece);
            SetRam16(piece + kY + 2, 0);
            SetRam8(piece + kTurn + 1, 0);
            SetRam16(piece + 0x40, Ram16(piece + kY));
            Thrown37(c, piece);
            SetRam8(piece + kTurn, Ram8(piece + kWhich) == 3 ? 0 : 0x10);
            SetStageAnimation(c, Ram8(piece + kWhich));
            break;
        case 2:
        case 4:
            if (step == 2 && Ram8(Var(c, kScript37Changes)) == 1) {
                NextStep(piece);
                SetLow8(c.d[0], Ram8(piece + kWhich) + 1);
                SetStageAnimation(c);
            }
            if (!Frozen(c) && CountedDown(piece + kTurn)) {
                SetRam8(piece + kTurn, 0);
                SetRam32(piece + kY, Ram32(piece + kY) + Ram32(piece + kSpeedY));
                SetRam32(piece + kSpeedY, Ram32(piece + kSpeedY) + Ram32(piece + kPullY));
                c.d[0] = Ram32(piece + kPullY);
                SetLow16(c.d[0], Ram16(piece + 0x40));
                if (static_cast<int16_t>(Ram16(piece + 0x40)) >= static_cast<int16_t>(Ram16(piece + kY))) {
                    SetRam16(piece + kY, Ram16(piece + 0x40));
                    SetRam16(piece + kY + 2, 0);
                    SetRam8(piece + kTurn + 1, (Ram8(piece + kTurn + 1) + 8) & 0x18);
                    Thrown37(c, piece);
                }
            }
            ByCamera(c);
            break;
        default:
            GiveBack(c);
            break;
        }
    }

    // 07FFDE (script 43, Fetus of God): the pieces in front that work their place out from the camera's
    // (above). When a player's health is below 48 (and the byte at 10E(A5) is not set) they change: from
    // then, each event of the animation holds its frame for three frames only.
    constexpr int32_t kScript43Never = 0x10E; // from A5
    void Script43(M68K &c) {
        static constexpr Steps kSteps = {0x07FFDE, {0x07FFF2, 0x08000E, 0x08003C, 0x080060}};
        const uint32_t piece = c.a[6] & 0xFFFFFF;
        switch (StepAt(c, kSteps)) {
        case 0:
            SetRam32(piece + kStartedX, Ram32(piece + kX));
            SetRam32(piece + kStartedY, Ram32(piece + kY));
            StartAs(c, Ram8(piece + kWhich));
            break;
        case 2:
            if (!Frozen(c)) {
                Animate(c);
                if (!Ram8(Var(c, kScript43Never)) && (Ram16(Player(0) + kHealth) < 0x30 || Ram16(Player(1) + kHealth) < 0x30)) {
                    NextStep(piece);
                }
            }
            PlaceASixteenthMore(c);
            ByCamera(c);
            break;
        case 4:
            if (!Frozen(c)) {
                Animate(c);
                if (Ram8(piece + kEvent)) {
                    SetRam8(piece + kEvent, 0);
                    SetRam8(piece + kTimer, 3);
                }
            }
            PlaceASixteenthMore(c);
            ByCamera(c);
            break;
        default:
            GiveBack(c);
            break;
        }
    }

    // 080462 (script 48, Abaraya): as scripts 46 and 47, by all but a thirty-second of the camera's
    // travel, and only while its own place is 496 or more: once the camera has taken it below that it is
    // neither moved nor drawn again. (As the game has it, in a wide picture too.)
    void Script48(M68K &c) {
        static constexpr Steps kSteps = {0x080462, {0x080476, 0x08048E, 0x0804D2, 0x0804D2}};
        const uint32_t piece = c.a[6] & 0xFFFFFF;
        switch (StepAt(c, kSteps)) {
        case 0:
            SetRam32(piece + kStartedX, Ram32(piece + kX));
            SetRam32(piece + kStartedY, Ram32(piece + kY));
            StartAs(c, 3);
            break;
        case 2: {
            if (!Frozen(c)) {
                Animate(c);
            }
            SetLow16(c.d[1], Ram16(kCameraX));
            if (Ram16(piece + kX) < 0x1F0) {
                break;
            }
            const int16_t across = static_cast<int16_t>(Ram16(kCameraRecord + kLayerStartX) - Ram16(kCameraX));
            SetRam16(piece + kX, static_cast<uint16_t>(across - (across >> 5) + Ram16(piece + kStartedX)));
            SetRam16(piece + kY, static_cast<uint16_t>(TravelUp() + Ram16(piece + kStartedY)));
            PieceDrawn(c);
            break;
        }
        default:
            GiveBack(c);
            break;
        }
    }

    // 08087C (script 55, the train): goes with the piece that made it, at a place from its own by the
    // spare byte of its maker's frame; there only with the camera right of a place; drawn every other frame
    constexpr int8_t kScript55Places[31][2] = {{-2, 32}, {-2, 28}, {-4, 28}, {0, 28},  {0, 28},  {0, 28},  {-16, 26}, {-16, 26},
                                               {-2, 32}, {2, 38},  {2, 42},  {3, 42},  {-2, 32}, {-2, 28}, {-2, 28},  {-2, 28},
                                               {-2, 28}, {-2, 28}, {-2, 28}, {-2, 28}, {-4, 30}, {-16, 30}, {-2, 30}, {-2, 30},
                                               {-2, 30}, {-2, 30}, {-2, 30}, {-2, 30}, {-2, 30}, {-2, 30}, {-2, 30}};
    void Script55(M68K &c) {
        static constexpr Steps kSteps = {0x08087C, {0x080890, 0x0808A6, 0x08096A, 0x08096A}};
        const uint32_t piece = c.a[6] & 0xFFFFFF;
        switch (StepAt(c, kSteps)) {
        case 0:
            SetRam8(piece + kDepth, 2);
            SetRam8(piece + 0x0D, 0xFF);
            StartAs(c, 0x21);
            [[fallthrough]];
        case 2:
            if (OutOfView(c, 0x1E8, true)) {
                break;
            }
            if (!Frozen(c)) {
                c.d[0] = Ram8(Maker(c) + kSpare) * 4u;
                const int8_t *place = kScript55Places[(c.d[0] / 4) % 31];
                SetLow16(c.d[1], place[0] + Ram16(piece + kStartedX));
                SetLow16(c.d[2], place[1] + Ram16(piece + kStartedY));
                SetRam16(piece + kX, static_cast<uint16_t>(c.d[1]));
                SetRam16(piece + kY, static_cast<uint16_t>(c.d[2]));
                Animate(c);
            }
            if (Ram8(Var(c, kOddFrame)) & 1) {
                PieceDrawn(c);
            }
            break;
        default:
            GiveBack(c);
            break;
        }
    }

    // A piece made by the one at A6, on the same scroll screen and row
    void LikeMaker(M68K &c, uint32_t made, uint32_t piece) {
        SetRam8(made + kIsRiding, Ram8(piece + kIsRiding));
        SetRam16(made + kRow, Ram16(piece + kRow));
        SetRam32(made + kPlaces, Ram32(piece + kPlaces));
        (void)c;
    }

    // 07FCF0 (script 38, the train): there only with the camera right of a place. Eight animations in
    // turn; an event of a frame above nothing copies in a set of colours (by the event, and the game's
    // word for which colours a stage has), and one below nothing lets a piece go (script 39). It makes a
    // piece that goes with it (script 55).
    constexpr uint32_t kScript38Colours[10] = {0, 0, 0x3A0B20, 0x3A2640, 0x3A1660, 0x3A3180, 0x3A1680, 0x3A31A0, 0x3A16A0, 0x3A31C0};
    void Script38(M68K &c) {
        static constexpr Steps kSteps = {0x07FCF0, {0x07FD04, 0x07FD4A, 0x07FE00, 0x07FE00}};
        const uint32_t piece = c.a[6] & 0xFFFFFF;
        switch (StepAt(c, kSteps)) {
        case 0:
            NextStep(piece);
            SetRam8(piece + kHalf, 0);
            if (Take(c)) {
                const uint32_t made = c.a[4] & 0xFFFFFF;
                SetRam32(made, 0x01003700);
                SetRam16(made + kStartedX, Ram16(piece + kX));
                SetRam16(made + kStartedY, Ram16(piece + kY));
                LikeMaker(c, made, piece);
                SetRam16(made + kMaker, static_cast<uint16_t>(c.a[6]));
            }
            SetStageAnimation(c, 0x19 + Ram8(piece + kHalf));
            break;
        case 2:
            if (OutOfView(c, 0x1E8, true)) {
                break;
            }
            if (!Frozen(c)) {
                if (Ram8(piece + kOver) & 0x80) {
                    SetRam8(piece + kHalf, (Ram8(piece + kHalf) + 1) & 7);
                    SetStageAnimation(c, 0x19 + Ram8(piece + kHalf));
                } else {
                    c.d[0] = Ram8(piece + kEvent);
                    if (!c.d[0]) {
                        Animate(c);
                    } else if (c.d[0] & 0x80) {
                        SetRam8(piece + kEvent, 0);
                        if (Take(c)) {
                            const uint32_t made = c.a[4] & 0xFFFFFF;
                            SetRam32(made, 0x01002700);
                            SetRam16(made + kX, 0x370);
                            SetRam16(made + kY, 0x78);
                            SetRam8(made + kDepth, 2);
                            LikeMaker(c, made, piece);
                            Animate(c);
                        }
                    } else {
                        SetRam8(piece + kEvent, 0);
                        SetLow16(c.d[0], c.d[0] + Ram16(Var(c, kColoursOf)));
                        c.a[0] = kScript38Colours[((c.d[0] & 0xFFFF) >> 2) % 10];
                        c.a[1] = 0x90C3A0;
                        c.d[7] = 0;
                        Call(c, kCopyColours);
                    }
                }
            }
            PieceDrawn(c);
            break;
        default:
            GiveBack(c);
            break;
        }
    }

    // 07DF9C (script 11, Concrete Cave): a piece with two others it makes (scripts 51 and 52). Awake only
    // with the camera right of a place. At an event of its animation it looks at the first of the two: if
    // that one's event is set it goes through another animation (13) and then a third (14); otherwise,
    // one time in four, its own again from the start. In the third it waits on two longs of memory that
    // the game reads from below its own record (a slip for the players' states, by the look of it; kept).
    void Script11(M68K &c) {
        static constexpr Steps kSteps = {0x07DF9C, {0x07DFB0, 0x07E040, 0x07E108, 0x07E108}};
        static constexpr uint32_t kInnerAt[3] = {0x07E05E, 0x07E0A2, 0x07E0BA};
        constexpr uint16_t kAwakeFrom = 0x1D0;
        const uint32_t piece = c.a[6] & 0xFFFFFF;
        switch (StepAt(c, kSteps)) {
        case 0:
            NextStep(piece);
            if (Ram8(Var(c, kFreePiecesCount)) >= 2) {
                for (int n = 0; n < 2; n++) {
                    Call(c, kTakePiece);
                    const uint32_t made = c.a[4] & 0xFFFFFF;
                    SetRam32(made, n == 0 ? 0x01003300 : 0x01003400);
                    SetRam16(made + kX, n == 0 ? Ram16(piece + kX) : 0x373);
                    SetRam16(made + kY, n == 0 ? Ram16(piece + kY) : 0x37);
                    SetRam8(made + kFacesRight, Ram8(piece + kFacesRight));
                    LikeMaker(c, made, piece);
                    SetRam32(made + 0x28, 0x800001C0);
                    SetRam16(made + kMaker, static_cast<uint16_t>(c.a[6]));
                    if (n == 0) {
                        SetRam16(piece + 0x40, static_cast<uint16_t>(c.a[4]));
                    }
                }
            }
            SetStageAnimation(c, 8);
            break;
        case 2:
            if (!Frozen(c)) {
                switch (InnerStepOf(c, 0x07E058, kInnerAt)) {
                case 0:
                    if (OutOfView(c, kAwakeFrom, true)) {
                        break;
                    }
                    Animate(c);
                    if (!Ram8(piece + kEvent)) {
                        break;
                    }
                    SetRam8(piece + kEvent, 0);
                    c.a[4] = 0xFFFF0000u | Ram16(piece + 0x40);
                    if (Ram8((c.a[4] & 0xFFFFFF) + kEvent)) {
                        SetRam8(piece + kInner, 2);
                        SetStageAnimation(c, 0x0D);
                    } else {
                        Chance(c, false);
                        if (!(c.d[0] & 0xFF)) {
                            SetStageAnimation(c, 8);
                        }
                    }
                    break;
                case 2:
                    if (Ram8(piece + kOver) & 0x80) {
                        SetRam8(piece + kInner, 4);
                        SetStageAnimation(c, 0x0E);
                    } else {
                        Animate(c);
                    }
                    break;
                default:
                    if (OutOfView(c, kAwakeFrom, true)) {
                        SetRam8(piece + kInner, 0);
                        SetRam8(piece + kEvent, 0);
                        SetStageAnimation(c, 8);
                        break;
                    }
                    c.d[1] = 0xFFFFFF00;
                    c.d[2] = 0x02001200;
                    c.d[0] = Ram32(piece - 0x7BFC) & c.d[1];
                    if (c.d[0] != c.d[2]) {
                        c.d[0] = Ram32(piece - 0x77FC) & c.d[1];
                    }
                    if (c.d[0] == c.d[2]) {
                        SetRam8(piece + kInner, 2);
                        SetStageAnimation(c, 0x0F);
                    } else {
                        Animate(c);
                    }
                    break;
                }
            }
            ByCamera(c);
            break;
        default:
            GiveBack(c);
            break;
        }
    }

    // 07F052 (script 24, Abaraya): a piece whose animation is chosen by its step within the frame (the
    // step halved, and ten). At a frame with its spare byte set it takes its next step: from the first,
    // one time in thirty-two the second; or the last, when both players' health is below 48 and one of
    // them is in a certain state; from the others, the step the frame's event names (from the second and
    // third, when there is an event, only one time in four). The last step, with the camera left of 264,
    // copies in a set of colours and goes back to the first, for good.
    constexpr uint32_t kScript24Colours[2] = {0x3A1420, 0x3A2F40};
    constexpr uint32_t kPlayerState = 0x02001200; // a player's kState but for its last byte
    void Script24(M68K &c) {
        static constexpr Steps kSteps = {0x07F052, {0x07F066, 0x07F07C, 0x07F19E, 0x07F19E}};
        static constexpr uint32_t kInnerAt[6] = {0x07F0A6, 0x07F126, 0x07F126, 0x07F150, 0x07F150, 0x07F168};
        const uint32_t piece = c.a[6] & 0xFFFFFF;
        // (the animation for the step within the frame that D0's low byte names)
        auto become = [&] {
            SetRam8(piece + kInner, c.d[0]);
            SetLow8(c.d[0], ((c.d[0] & 0xFF) >> 1) + 0x0A);
            SetStageAnimation(c);
        };
        switch (StepAt(c, kSteps)) {
        case 0:
            NextStep(piece);
            c.d[0] = 0;
            SetRam8(piece + kHalf, 0);
            SetLow8(c.d[0], 0x0A);
            SetStageAnimation(c);
            break;
        case 2:
            if (!Frozen(c)) {
                const int inner = InnerStepOf(c, 0x07F09A, kInnerAt);
                if (inner == 10) {
                    if (Ram16(kCameraX) < 0x108) {
                        SetRam8(piece + kHalf, 0xFF);
                        SetLow16(c.d[0], Ram16(Var(c, kColoursOf)));
                        c.a[0] = kScript24Colours[((c.d[0] & 0xFFFF) >> 2) & 1];
                        c.a[1] = 0x90C2E0;
                        c.d[7] = 0;
                        Call(c, kCopyColours);
                        c.d[0] = 0;
                        become();
                    }
                } else if (Ram8(piece + kSpare)) {
                    SetRam8(piece + kSpare, 0);
                    if (inner == 0) {
                        Call(c, kRandom);
                        SetLow16(c.d[0], c.d[0] & 0x1F);
                        SetLow8(c.d[0], (c.d[0] & 0xFFFF) == 31 ? 2 : 0);
                        if (!Ram8(piece + kHalf) && !Ram8(Var(c, kScript43Never)) && Ram16(Player(0) + kHealth) < 0x30 &&
                            Ram16(Player(1) + kHealth) < 0x30) {
                            c.d[1] = 0xFFFFFF00;
                            c.d[2] = kPlayerState;
                            c.d[3] = Ram32(Player(0) + kState) & c.d[1];
                            if (c.d[3] != c.d[2]) {
                                c.d[3] = Ram32(Player(1) + kState) & c.d[1];
                            }
                            if (c.d[3] == c.d[2]) {
                                c.d[0] = 0x0A;
                            }
                        }
                        become();
                    } else if (inner >= 6) {
                        SetLow8(c.d[0], Ram8(piece + kEvent));
                        become();
                    } else {
                        bool goes = true;
                        if (Ram8(piece + kEvent)) {
                            Chance(c, true);
                            goes = !(c.d[0] & 0xFFFF);
                        }
                        if (goes) {
                            SetLow8(c.d[0], Ram8(piece + kEvent));
                            become();
                        }
                    }
                }
                Animate(c);
            }
            ByCamera(c);
            break;
        default:
            GiveBack(c);
            break;
        }
    }

    // A word of the game's program as the 68000 reads it when it reads by its program counter (the
    // deciphered program; reads through an address register get the ROM as it is)
    uint16_t ProgramWord(M68K &c, uint32_t address) {
        return static_cast<uint16_t>((c.opcodes[address] << 8) | c.opcodes[address + 1]);
    }

    // --- pieces that fly a path ---
    //
    // A path is legs in the ROM, each: a word of frames, where the leg starts (two words), and six longs:
    // speed across and up, what is taken off each every frame (twice over: the first two are added to,
    // the second two are what they are added to by). A word below nothing ends the path.
    constexpr uint32_t kLegLeft = 0x26, kPathStart = 0x58, kPathAt = 0x5C;
    constexpr uint32_t kLegFields[6] = {0x40, 0x44, 0x48, 0x50, 0x4C, 0x54};

    // 07E672. The leg at A0 is begun by the piece at A6
    void TakeLeg(M68K &c) {
        const uint32_t piece = c.a[6] & 0xFFFFFF;
        SetRam16(piece + kLegLeft, Mem16(c, c.a[0]));
        SetRam32(piece + kX, static_cast<uint32_t>(Mem16(c, c.a[0] + 2)) << 16);
        SetRam32(piece + kY, static_cast<uint32_t>(Mem16(c, c.a[0] + 4)) << 16);
        c.a[0] += 6;
        for (const uint32_t field : kLegFields) {
            SetRam32(piece + field, Mem32(c, c.a[0]));
            c.a[0] += 4;
        }
        SetRam32(piece + kPathAt, c.a[0]);
    }

    // 07E77E. A frame of the leg: the piece moves, and faces the way it is going across
    void FlyLeg(M68K &c) {
        const uint32_t piece = c.a[6] & 0xFFFFFF;
        SetRam8(piece + kFacesRight, 0);
        SetRam32(piece + 0x48, Ram32(piece + 0x48) - Ram32(piece + 0x50));
        c.d[0] = Ram32(piece + 0x40) + Ram32(piece + 0x48);
        if (c.d[0] & 0x80000000) {
            SetRam8(piece + kFacesRight, 1);
        }
        SetRam32(piece + kX, Ram32(piece + kX) + c.d[0]);
        SetRam32(piece + 0x4C, Ram32(piece + 0x4C) - Ram32(piece + 0x54));
        c.d[0] = Ram32(piece + 0x44) + Ram32(piece + 0x4C);
        SetRam32(piece + kY, Ram32(piece + kY) + c.d[0]);
    }

    // 07EA82 (script 17, Deserted Chateau): waits for a round to be decided, then flies its path's legs one
    // after another (three paths, by kWhich), and at the path's end goes through a last animation and is done
    constexpr uint8_t kScript17Waits[4] = {10, 40, 70, 90};
    constexpr uint32_t kScript17Paths[3] = {0x07EB48, 0x07EB88, 0x07EBAA};
    void Script17(M68K &c) {
        static constexpr Steps kSteps = {0x07EA82, {0x07EA96, 0x07EAD8, 0x07EB42, 0x07EB42}};
        static constexpr uint32_t kInnerAt[3] = {0x07EAF6, 0x07EB0A, 0x07EB36};
        const uint32_t piece = c.a[6] & 0xFFFFFF;
        switch (StepAt(c, kSteps)) {
        case 0:
            StartAs(c, 0x0B);
            Chance(c, true);
            SetRam8(piece + kTimer, kScript17Waits[c.d[0] & 3]);
            c.d[0] = Ram8(piece + kWhich) * 4u;
            c.a[0] = kScript17Paths[Ram8(piece + kWhich) % 3];
            SetRam32(piece + kPathStart, c.a[0]);
            TakeLeg(c);
            break;
        case 2:
            switch (InnerStepOf(c, 0x07EAF0, kInnerAt)) {
            case 0:
                if (Ram8(Var(c, kRoundDecided))) {
                    SetRam8(piece + kInner, 2);
                    SetStageAnimation(c, 9);
                }
                break;
            case 2:
                SetRam16(piece + kLegLeft, static_cast<uint16_t>(Ram16(piece + kLegLeft) - 1));
                if (!Ram16(piece + kLegLeft)) {
                    c.a[0] = Ram32(piece + kPathAt);
                    if (!(Mem16(c, c.a[0]) & 0x8000)) {
                        TakeLeg(c);
                        break;
                    }
                    SetRam8(piece + kInner, 4);
                    SetStageAnimation(c, 0x0A);
                }
                FlyLeg(c);
                break;
            default:
                if (Ram8(piece + kOver) & 0x80) {
                    NextStep(piece);
                }
                break;
            }
            Animate(c);
            IfInPicture(c);
            break;
        default:
            GiveBack(c);
            break;
        }
    }

    // 07E62C (script 16, Deserted Chateau): flies its path's legs one after another (four paths, by
    // kWhich); at the path's end it sits where the next leg starts for a time (one of four by chance, for
    // its kind) and then flies on, or from the path's start again. Once a round is decided, the first sit
    // ends at once.
    constexpr uint32_t kScript16Paths[4] = {0x07E7B4, 0x07E832, 0x07E8B0, 0x07E9A8};
    constexpr uint16_t kScript16Sits[5][4] = {{180, 360, 900, 1260}, {200, 400, 1000, 1400}, {210, 420, 1050, 1470},
                                              {240, 480, 1200, 1680}, {190, 380, 950, 1330}};
    void Script16(M68K &c) {
        static constexpr Steps kSteps = {0x07E62C, {0x07E640, 0x07E6A4, 0x07E778, 0x07E778}};
        const uint32_t piece = c.a[6] & 0xFFFFFF;
        switch (StepAt(c, kSteps)) {
        case 0:
            NextStep(piece);
            SetRam8(piece + kHalf, 0);
            c.a[0] = kScript16Paths[Ram8(piece + kWhich) & 3];
            SetRam32(piece + kPathStart, c.a[0]);
            TakeLeg(c);
            SetStageAnimation(c, 9);
            break;
        case 2: {
            const int inner = Ram8(piece + kInner);
            SetLow8(c.d[0], inner);
            SetLow16(c.d[0], (inner ? 0x07E746 : 0x07E6C0) - 0x07E6BC); // (the game's way to the step, through D0 here)
            if (inner == 0) {
                // (flying)
                SetRam16(piece + kLegLeft, static_cast<uint16_t>(Ram16(piece + kLegLeft) - 1));
                if (Ram16(piece + kLegLeft)) {
                    FlyLeg(c);
                } else {
                    c.a[0] = Ram32(piece + kPathAt);
                    if (!(Mem16(c, c.a[0]) & 0x8000)) {
                        TakeLeg(c);
                        Animate(c);
                        IfInPicture(c);
                        break;
                    }
                    c.a[0] += 2;
                    if (Mem16(c, c.a[0]) & 0x8000) {
                        c.a[0] = Ram32(piece + kPathStart);
                    }
                    SetRam8(piece + kInner, 2);
                    SetRam32(piece + kPathAt, c.a[0]);
                    c.d[2] = Ram8(piece + kWhich) * 8u;
                    Call(c, kRandom);
                    SetLow16(c.d[0], c.d[0] & 6);
                    SetLow16(c.d[2], c.d[2] + c.d[0]);
                    SetRam16(piece + kLegLeft, kScript16Sits[((c.d[2] & 0xFFFF) >> 3) % 5][((c.d[2] & 0xFFFF) >> 1) & 3]);
                    SetRam16(piece + kX, Mem16(c, c.a[0] + 2));
                    SetRam16(piece + kY, Mem16(c, c.a[0] + 4));
                    SetStageAnimation(c, 0x0B);
                }
            } else {
                // (sitting)
                SetRam16(piece + kLegLeft, static_cast<uint16_t>(Ram16(piece + kLegLeft) - 1));
                if (!Ram16(piece + kLegLeft)) {
                    c.a[0] = Ram32(piece + kPathAt);
                    TakeLeg(c);
                    SetRam8(piece + kInner, 0);
                    SetStageAnimation(c, 9);
                }
                if (Ram8(Var(c, kRoundDecided)) && !Ram8(piece + kHalf)) {
                    SetRam16(piece + kLegLeft, 1);
                    SetRam8(piece + kHalf, 0xFF);
                }
            }
            Animate(c);
            IfInPicture(c);
            break;
        }
        default:
            GiveBack(c);
            break;
        }
    }

    // 07EBEA (script 18, Deserted Chateau): when a round is decided it makes the stage's other pieces (two
    // from another list of records, and four of script 20), and from then lets pieces go (script 19) from
    // six places by lists in the ROM (which place, and how long till the next; at a list's end another
    // of four by chance), and every 43 frames one more from one of two places
    constexpr uint32_t kTakeOther = 0x016FFC;  // a free record of another list in A4
    constexpr int32_t kOtherCount = 0x79C3;    // from A5, a byte: how many that list has
    constexpr uint32_t kScript18First = 0x07ED42, kScript18ListEnds = 0x0036A4;
    constexpr uint32_t kScript18Lists[4] = {0x07ED32, 0x07ED44, 0x07ED52, 0x07ED64};
    constexpr uint16_t kScript18Places[6][2] = {{523, 47}, {555, 94}, {604, 98}, {660, 72}, {684, 103}, {701, 49}};
    void Script18(M68K &c) {
        static constexpr Steps kSteps = {0x07EBEA, {0x07EBFE, 0x07EC0A, 0x07EC6E, 0x07ED2C}};
        const uint32_t piece = c.a[6] & 0xFFFFFF;
        auto takeOther = [&] {
            const bool any = Ram8(Var(c, kOtherCount)) != 0;
            Call(c, kTakeOther);
            return any;
        };
        switch (StepAt(c, kSteps)) {
        case 0:
            StartAs(c, 0);
            break;
        case 2:
            if (Ram8(Var(c, kRoundDecided))) {
                NextStep(piece);
                SetRam16(piece + kTurn, 1);
                SetRam16(piece + 0x40, 1);
                SetRam32(piece + 0x44, kScript18First);
                if (takeOther()) {
                    SetRam32(c.a[4] & 0xFFFFFF, 0x01000006);
                    if (takeOther()) {
                        SetRam32(c.a[4] & 0xFFFFFF, 0x01000007);
                        c.d[6] = 3;
                        for (int n = 3; n >= 0 && Take(c); n--) {
                            SetRam32(c.a[4] & 0xFFFFFF, 0x01001400);
                            SetRam8((c.a[4] & 0xFFFFFF) + kWhich, n);
                            SetLow16(c.d[6], n - 1);
                        }
                    }
                }
            }
            if (!Frozen(c)) {
                Animate(c);
            }
            ByCamera(c);
            break;
        case 4:
            if (!Frozen(c)) {
                SetRam16(piece + kTurn, static_cast<uint16_t>(Ram16(piece + kTurn) - 1));
                if (!Ram16(piece + kTurn)) {
                    c.a[2] = Ram32(piece + 0x44);
                    SetLow16(c.d[4], Mem16(c, c.a[2]));
                    c.a[2] += 2;
                    if (c.d[4] & 0x8000) {
                        Call(c, kScript18ListEnds);
                        Call(c, kRandom);
                        SetLow16(c.d[0], c.d[0] & 0x0C);
                        c.a[2] = kScript18Lists[(c.d[0] >> 2) & 3];
                        SetLow16(c.d[4], Mem16(c, c.a[2]));
                        c.a[2] += 2;
                    }
                    if (Take(c)) {
                        const uint32_t made = c.a[4] & 0xFFFFFF;
                        SetRam32(made, 0x01001300);
                        SetRam16(piece + kTurn, Mem16(c, c.a[2]));
                        c.a[2] += 2;
                        SetLow16(c.d[4], c.d[4] * 4);
                        // (the lists name a seventh place, which the table has not got: the game takes the
                        // words that follow the table in its program, and so is this done)
                        const uint32_t place = (c.d[4] & 0xFFFF) >> 2;
                        SetRam16(made + kX, place < 6 ? kScript18Places[place][0] : ProgramWord(c, 0x07ED14 + place * 4));
                        SetRam16(made + kY, place < 6 ? kScript18Places[place][1] : ProgramWord(c, 0x07ED16 + place * 4));
                        SetRam32(piece + 0x44, c.a[2]);
                    }
                }
                SetRam16(piece + 0x40, static_cast<uint16_t>(Ram16(piece + 0x40) - 1));
                if (!Ram16(piece + 0x40)) {
                    SetRam16(piece + 0x40, 0x2B);
                    if (Take(c)) {
                        const uint32_t made = c.a[4] & 0xFFFFFF;
                        SetRam32(made, 0x01001301);
                        SetRam16(made + kX, 0x1C7);
                        SetRam16(made + kY, 0x76);
                        Call(c, kRandom);
                        if (!(c.d[0] & 0x80)) {
                            SetRam16(made + kX, 0x26A);
                            SetRam16(made + kY, 0x4B);
                        }
                    }
                }
                Animate(c);
            }
            ByCamera(c);
            break;
        default:
            GiveBack(c);
            break;
        }
    }

    // 07F2F0 (script 27, Vanity Paradise): a thing that floats up and down about a height (70), pulled back
    // towards it; each time it is past the height and still being pulled away, the pull is made up anew
    // (and its speed nudged by a number the game takes from whatever its D3 held: kept). Its picture is
    // one of twelve in turn (a new one every fifteen frames) and another on every other frame. It makes
    // two pieces that go with it (script 54).
    void Script27(M68K &c) {
        static constexpr Steps kSteps = {0x07F2F0, {0x07F304, 0x07F3A2, 0x07F432, 0x07F432}};
        constexpr uint16_t kHighest = 0x4E, kHeight = 0x46;
        const uint32_t piece = c.a[6] & 0xFFFFFF;
        switch (StepAt(c, kSteps)) {
        case 0:
            NextStep(piece);
            SetRam8(piece + kHalf, 0);
            SetRam32(piece + kSpeedY, 0x600);
            SetRam32(piece + kPullY, 0xFFFFFFC0);
            SetRam8(piece + kTurn, 0x0E);
            SetRam8(piece + kTurn + 1, 9);
            for (int n = 0; n < 2 && Take(c); n++) {
                const uint32_t made = c.a[4] & 0xFFFFFF;
                SetRam32(made, n == 0 ? 0x01003616 : 0x01003617);
                SetRam8(made + kDepth, n == 0 ? 2 : 4);
                SetRam8(made + kFacesRight, Ram8(piece + kFacesRight));
                LikeMaker(c, made, piece);
                SetRam32(made + 0x28, 0x02788000);
                SetRam16(made + kMaker, static_cast<uint16_t>(c.a[6]));
            }
            SetStageAnimation(c, 9);
            break;
        case 2:
            c.d[1] = Ram32(piece + kSpeedY);
            SetRam32(piece + kY, Ram32(piece + kY) + c.d[1]);
            c.d[2] = Ram32(piece + kPullY);
            SetRam32(piece + kSpeedY, Ram32(piece + kSpeedY) + c.d[2]);
            if (Ram16(piece + kY) > kHighest) {
                SetRam32(piece + kY, Ram32(piece + kY) - c.d[1]);
            }
            c.d[1] = (static_cast<uint32_t>(static_cast<uint16_t>(Ram16(piece + kY) - kHeight)) << 16) | (c.d[1] >> 16);
            c.d[1] ^= c.d[2];
            if (!(c.d[1] & 0x80000000)) {
                Call(c, kRandom);
                c.d[3] &= 3;
                SetLow16(c.d[3], c.d[3] << 6);
                Call(c, kRandom);
                SetLow16(c.d[0], c.d[0] & 1);
                if (!(c.d[0] & 0xFFFF)) {
                    c.d[3] = 0u - c.d[3];
                    c.d[2] = 0u - c.d[2];
                }
                SetRam32(piece + kSpeedY, Ram32(piece + kSpeedY) + c.d[3]);
                SetRam32(piece + kPullY, c.d[2]);
            }
            if (!Frozen(c) && CountedDown(piece + kTurn)) {
                SetRam8(piece + kTurn, 0x0E);
                SetRam8(piece + kTurn + 1, Ram8(piece + kTurn + 1) + 1 >= 0x15 ? 9 : Ram8(piece + kTurn + 1) + 1);
            }
            SetStageAnimation(c, (Ram8(Var(c, kOddFrame)) & 1) ? Ram8(piece + kTurn + 1) : 0x15);
            ByCamera(c);
            break;
        default:
            GiveBack(c);
            break;
        }
    }

    // 07FE5A (script 40, the train): a piece whose animation's events change something of the scroll
    // screens' tiles (the game's own code still: 01520A, by a table at 357CA4). Until a round is decided,
    // every 180 frames one of two animations; then a last one, and a piece let go (script 41).
    constexpr uint32_t kScript40Tiles = 0x357CA4, kChangeTiles = 0x01520A;
    void Script40Tiles(M68K &c, uint32_t piece) {
        SetLow8(c.d[1], Ram8(piece + kEvent));
        if ((c.d[1] & 0xFF) == Ram8(piece + kHalf)) {
            return;
        }
        SetRam8(piece + kHalf, c.d[1]);
        SetLow16(c.d[1], static_cast<int8_t>(c.d[1]) << 4);
        c.a[0] = kScript40Tiles;
        SetLow16(c.d[0], Mem16(c, c.a[0] + 0x10) + c.d[1]);
        c.a[0] += static_cast<uint32_t>(static_cast<int16_t>(c.d[0]));
        c.a[0] = Mem32(c, c.a[0] + 4);
        c.d[0] = (static_cast<uint32_t>(Ram16(Var(c, 0x1A))) << 8) + 0x3764;
        c.a[2] = c.d[0];
        c.d[2] = 1;
        Call(c, kChangeTiles);
    }
    void Script40(M68K &c) {
        static constexpr Steps kSteps = {0x07FE5A, {0x07FE6E, 0x07FE84, 0x07FF1A, 0x07FF30}};
        const uint32_t piece = c.a[6] & 0xFFFFFF;
        switch (StepAt(c, kSteps)) {
        case 0:
            SetRam8(piece + kHalf, 0xFF);
            SetRam16(piece + kTurn, 0xB4);
            StartAs(c, 0x16);
            break;
        case 2:
            if (!Frozen(c)) {
                if (InnerStep(c, 0x07FEA6, 0x07FEAA, 0x07FECA) == 0) {
                    SetRam16(piece + kTurn, static_cast<uint16_t>(Ram16(piece + kTurn) - 1));
                    if (Ram16(piece + kTurn) & 0x8000) {
                        SetRam8(piece + kInner, 2);
                        Call(c, kRandom);
                        SetLow16(c.d[0], c.d[0] & 1);
                        SetLow8(c.d[0], c.d[0] + 0x16);
                        SetStageAnimation(c);
                    }
                } else if (Ram8(piece + kOver) & 0x80) {
                    SetRam8(piece + kInner, 0);
                }
                if (Ram8(Var(c, kRoundDecided))) {
                    NextStep(piece);
                    if (Take(c)) {
                        const uint32_t made = c.a[4] & 0xFFFFFF;
                        SetRam32(made, 0x01000029);
                        SetRam8(made + kDepth, 2);
                        SetRam16(made + kX, Ram16(piece + kX));
                        SetRam16(made + kY, Ram16(piece + kY));
                        LikeMaker(c, made, piece);
                    }
                    SetStageAnimation(c, 0x18);
                }
            }
            Script40Tiles(c, piece);
            Animate(c);
            ByCamera(c);
            break;
        case 4:
            if (!Frozen(c)) {
                Script40Tiles(c, piece);
                Animate(c);
            }
            ByCamera(c);
            break;
        default:
            GiveBack(c);
            break;
        }
    }

    // --- pieces with colours of their own that change ---
    //
    // A record can run up to three animations of colours beside its own (eight bytes each, from +50):
    // sets of colours copied into the stage's as the animation says. The game's own code does that still
    // (it writes the board's colours): begun at 09192A (which animation in D0, of the table at A0, for the
    // eight bytes at A4), and a frame of it at 09190A.
    void ColoursBegin(M68K &c, uint32_t slot, uint32_t table, uint32_t number) {
        SetLow16(c.d[0], number);
        c.a[4] = c.a[6] + slot;
        c.a[0] = table;
        Call(c, 0x09192A);
    }
    void ColoursFrame(M68K &c, uint32_t slot) {
        c.a[4] = c.a[6] + slot;
        Call(c, 0x09190A);
    }

    // 0800EC (script 45, Fetus of God): at each end of its animation another of four by chance, with that
    // one's colours; when a round is decided, a last one, and a piece let go (script 20)
    constexpr uint8_t kScript45Next[32] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 3, 3, 2, 2, 2, 2, 2, 3, 3, 3};
    constexpr uint32_t kScript45Colours = 0x0801F2;
    void Script45(M68K &c) {
        static constexpr Steps kSteps = {0x0800EC, {0x080100, 0x08011C, 0x0801CE, 0x0801EC}};
        const uint32_t piece = c.a[6] & 0xFFFFFF;
        switch (StepAt(c, kSteps)) {
        case 0:
            StartAs(c, 0x0F);
            c.d[0] = 0;
            ColoursBegin(c, 0x50, kScript45Colours, 0);
            break;
        case 2:
            if (!Frozen(c)) {
                if (Ram8(piece + kOver) & 0x80) {
                    c.d[6] = 0;
                    Call(c, kRandom);
                    SetLow16(c.d[0], c.d[0] & 0x1F);
                    SetLow8(c.d[6], kScript45Next[c.d[0] & 0x1F]);
                    SetLow16(c.d[0], c.d[6] + 0x0F);
                    SetStageAnimation(c);
                    ColoursBegin(c, 0x50, kScript45Colours, c.d[6] & 0xFFFF);
                }
                if (Ram8(Var(c, kRoundDecided))) {
                    NextStep(piece);
                    SetStageAnimation(c, 0x13);
                    c.d[0] = 4;
                    ColoursBegin(c, 0x50, kScript45Colours, 4);
                    if (Take(c)) {
                        const uint32_t made = c.a[4] & 0xFFFFFF;
                        SetRam32(made, 0x01000014);
                        SetRam16(made + kX, 0x228);
                        SetRam16(made + kY, 0xA0);
                        SetRam8(made + kDepth, 6);
                        SetRam8(made + kIsRiding, Ram8(piece + kIsRiding));
                        SetRam16(made + kRow, Ram16(piece + kRow));
                        SetRam16(made + kPlaces, 0x2000);
                        SetRam16(made + kPlaces + 2, 0xA000);
                    }
                }
                ColoursFrame(c, 0x50);
                Animate(c);
            }
            PieceDrawn(c);
            break;
        case 4:
            if (!Frozen(c)) {
                ColoursFrame(c, 0x50);
                Animate(c);
            }
            PieceDrawn(c);
            break;
        default:
            GiveBack(c);
            break;
        }
    }

    // 07D710 (script 6, Feast of the Damned): the light of the hall. It has no picture of its own to
    // speak of: it runs three animations of the stage's colours, by a list of its own in the ROM (one of
    // four lists by chance: pairs of a colours' number and how long; a number below nothing ends the
    // list, and then its picture is the other of two and another list is taken). It makes two pieces
    // (script 7), which take the colours' number from it.
    constexpr uint32_t kScript6Lists = 0x07D87C;
    constexpr uint32_t kScript6Colours[3][2] = {{0x50, 0x07D8C4}, {0x58, 0x07D8D4}, {0x60, 0x07D8DC}};
    void Script6List(M68K &c) {
        Call(c, kRandom);
        SetLow16(c.d[0], c.d[0] & 6);
        c.a[3] = kScript6Lists;
        SetLow16(c.d[0], Mem16(c, c.a[3] + (c.d[0] & 0xFFFF)));
        c.a[3] += static_cast<uint32_t>(static_cast<int16_t>(c.d[0]));
    }
    void Script6Colours(M68K &c, uint32_t piece) {
        c.d[0] = Ram8(piece + kHalf) * 4u;
        ColoursBegin(c, kScript6Colours[0][0], kScript6Colours[0][1], c.d[0] + Ram16(piece + kMakersChoice));
        ColoursBegin(c, kScript6Colours[1][0], kScript6Colours[1][1], Ram16(piece + kMakersChoice));
        ColoursBegin(c, kScript6Colours[2][0], kScript6Colours[2][1], Ram16(piece + kMakersChoice));
    }
    void Script6(M68K &c) {
        static constexpr Steps kSteps = {0x07D710, {0x07D724, 0x07D7C6, 0x07D820, 0x07D820}};
        constexpr uint32_t kListLeft = 0x46, kListAt = 0x40;
        const uint32_t piece = c.a[6] & 0xFFFFFF;
        auto nextOfList = [&] {
            SetRam16(piece + kListLeft, Mem16(c, c.a[3]));
            c.a[3] += 2;
            SetRam32(piece + kListAt, c.a[3]);
        };
        switch (StepAt(c, kSteps)) {
        case 0:
            NextStep(piece);
            for (int n = 0; n < 2 && Take(c); n++) {
                const uint32_t made = c.a[4] & 0xFFFFFF;
                SetRam32(made, n == 0 ? 0x01010704 : 0x01010700);
                SetRam8(made + kDepth, 4);
                SetRam8(made + kIsRiding, 0x0C);
                SetRam16(made + kX, n == 0 ? 0x1EE : 0x2C7);
                SetRam16(made + kY, n == 0 ? 0x84 : 0x83);
                SetRam16(made + kPlaces, 0x2000);
                SetRam16(made + kPlaces + 2, n == 0 ? 0x6000 : 0xA000);
                SetRam32(made + 0x28, n == 0 ? 0x02208000 : 0x80008000);
                SetRam16(made + kMaker, static_cast<uint16_t>(c.a[6]));
            }
            c.d[0] = 0;
            SetRam8(piece + kHalf, 0);
            Script6List(c);
            SetRam16(piece + kMakersChoice, Mem16(c, c.a[3]));
            c.a[3] += 2;
            nextOfList();
            SetStageAnimation(c, 0x19);
            Script6Colours(c, piece);
            break;
        case 2:
            if (!Frozen(c)) {
                SetRam16(piece + kListLeft, static_cast<uint16_t>(Ram16(piece + kListLeft) - 1));
                if (Ram16(piece + kListLeft) & 0x8000) {
                    c.a[3] = Ram32(piece + kListAt);
                    SetRam16(piece + kMakersChoice, Mem16(c, c.a[3]));
                    c.a[3] += 2;
                    if (Ram16(piece + kMakersChoice) & 0x8000) {
                        SetRam8(piece + kHalf, Ram8(piece + kHalf) ^ 1);
                        Animate(c);
                        Script6List(c);
                        SetRam16(piece + kMakersChoice, Mem16(c, c.a[3]));
                        c.a[3] += 2;
                    }
                    nextOfList();
                    Script6Colours(c, piece);
                }
                for (const auto &colours : kScript6Colours) {
                    ColoursFrame(c, colours[0]);
                }
            }
            ByCamera(c);
            break;
        default:
            GiveBack(c);
            break;
        }
    }

    // 07F438 (script 28, War Agony): the thing that crosses the stage from the right every so often,
    // with three pieces it makes that go with it (script 29). It waits (900 or 964 frames), sets off to
    // the left gathering speed; from 640 its colours' two animations run; left of 416 it looks for their
    // ends and starts two more; at 192 it is put back at the right to wait again.
    constexpr uint32_t kScript28Colours = 0x07F63A;
    void Script28Waits(M68K &c, uint32_t piece) {
        SetRam16(piece + kX, 0x400);
        SetRam16(piece + kY, 0xE6);
        SetLow16(c.d[2], 0x384);
        Call(c, kRandom);
        if (c.d[0] & 1) {
            SetLow16(c.d[2], 0x3C4);
        }
        SetRam16(piece + kTurn, static_cast<uint16_t>(c.d[2]));
        c.d[0] = 0;
        SetRam8(piece + kHalf, 0);
        SetRam32(piece + 0x48, 0);
        SetRam32(piece + kSpeedY, 0);
        SetRam32(piece + kSpeedX, 0xFFFF0000);
        SetRam32(piece + kPullY, 0xFFFFFFF0);
    }
    void Script28(M68K &c) {
        static constexpr Steps kSteps = {0x07F438, {0x07F44C, 0x07F546, 0x07F634, 0x07F634}};
        static constexpr uint32_t kInnerAt[4] = {0x07F55A, 0x07F56A, 0x07F5A8, 0x07F5DA};
        const uint32_t piece = c.a[6] & 0xFFFFFF;
        switch (StepAt(c, kSteps)) {
        case 0:
            NextStep(piece);
            SetLow8(c.d[0], 3);
            SetStageAnimation(c);
            c.d[6] = 0;
            // (three records are taken unless there are exactly three: the game's own test, kept)
            if (Ram8(Var(c, kFreePiecesCount)) != 3) {
                static constexpr uint16_t kAt[3][2] = {{0x400, 0xDE}, {0x3D0, 0xD6}, {0x3D0, 0xCE}};
                for (uint32_t n = 0; n < 3; n++) {
                    Call(c, kTakePiece);
                    const uint32_t made = c.a[4] & 0xFFFFFF;
                    SetRam32(made, 0x01001D04 + n);
                    SetRam16(made + kX, kAt[n][0]);
                    SetRam16(made + kY, kAt[n][1]);
                    SetRam32(made + kSpeedX, 0xFFFF1000 + n * 0x1000);
                    SetRam32(made + 0x48, 0);
                    SetRam32(made + kSpeedY, 0);
                    SetRam32(made + kPullY, 0xFFFFFFF0);
                    SetRam16(made + kMaker, static_cast<uint16_t>(c.a[6]));
                }
            }
            Script28Waits(c, piece);
            break;
        case 2:
            switch (InnerStepOf(c, 0x07F552, kInnerAt)) {
            case 0:
                SetRam16(piece + kTurn, static_cast<uint16_t>(Ram16(piece + kTurn) - 1));
                if (Ram16(piece + kTurn) & 0x8000) {
                    SetRam8(piece + kInner, 2);
                    SetRam8(piece + kHalf, 0xFF);
                }
                return; // (waiting: not drawn)
            case 2:
                if (!Frozen(c)) {
                    if (Ram16(piece + kX) <= 0x280) {
                        SetRam8(piece + kInner, 4);
                        c.d[0] = 0;
                        ColoursBegin(c, 0x50, kScript28Colours, 0);
                        c.d[0] = 2;
                        ColoursBegin(c, 0x58, kScript28Colours, 2);
                    }
                    Call(c, kMoveBySpeed);
                }
                break;
            case 4:
                if (!Frozen(c)) {
                    ColoursFrame(c, 0x50);
                    ColoursFrame(c, 0x58);
                    if (Ram16(piece + kX) <= 0x1A0) {
                        SetRam8(piece + kInner, 6);
                    }
                    Call(c, kMoveBySpeed);
                }
                break;
            default:
                if (!Frozen(c)) {
                    for (uint32_t n = 0; n < 2; n++) {
                        c.a[4] = c.a[6] + 0x50 + n * 8;
                        if (Ram8((c.a[4] & 0xFFFFFF) + 7)) {
                            c.d[0] = 1 + n * 2;
                            ColoursBegin(c, 0x50 + n * 8, kScript28Colours, 1 + n * 2);
                        }
                        Call(c, 0x09190A);
                    }
                    if (Ram16(piece + kX) <= 0xC0) {
                        SetRam8(piece + kInner, 0);
                        Script28Waits(c, piece);
                        return;
                    }
                    Call(c, kMoveBySpeed);
                }
                break;
            }
            IfInPicture(c);
            break;
        default:
            GiveBack(c);
            break;
        }
    }

    // 07E20E (script 14, Tower of Arrogance): the lights in the sky. One record is two lights, shown on
    // alternate frames: each has its own eight... thirty-two bytes of the record (from +40 and +60) with
    // its animation's frame, its place, and where it is in a list in the ROM of moves (a byte for double
    // speed, a byte for which way of a table of ways, a word of frames; a byte below nothing ends the
    // list, and the light starts again from its first place).
    constexpr uint32_t kScript14Lists = 0x07E4E0, kScript14Places = 0x07E360, kScript14Ways = 0x07E3E0;
    constexpr uint32_t kLightList = 0x08, kLightDouble = 0x10, kLightWay = 0x12, kLightLeft = 0x14, kLightX = 0x18, kLightY = 0x1C;
    void LightKeepsFrame(M68K &c) {
        const uint32_t piece = c.a[6] & 0xFFFFFF, light = c.a[4] & 0xFFFFFF;
        SetRam32(light, Ram32(piece + kFrame));
        SetRam32(light + 4, Ram32(piece + kTimer));
    }
    void LightPlaced(M68K &c) {
        const uint32_t light = c.a[4] & 0xFFFFFF;
        SetLow16(c.d[0], c.d[6]);
        c.a[1] = kScript14Places;
        SetLow16(c.d[0], c.d[0] << 3);
        const uint32_t at = c.a[1] + static_cast<uint32_t>(static_cast<int16_t>(c.d[0]));
        SetRam32(light + kLightX, Mem32(c, at));
        SetRam32(light + kLightY, Mem32(c, at + 4));
    }
    void LightAnimation(M68K &c) {
        SetLow16(c.d[0], c.d[6]);
        SetStageAnimation(c);
    }
    void LightNextMove(M68K &c) {
        const uint32_t piece = c.a[6] & 0xFFFFFF, light = c.a[4] & 0xFFFFFF;
        c.a[3] = Ram32(light + kLightList);
        if (Mem8(c, c.a[3]) & 0x80) {
            c.a[3] = Ram32(piece + 0x30);
            LightPlaced(c);
            LightAnimation(c);
            LightKeepsFrame(c);
        }
        SetRam8(light + kLightDouble, Mem8(c, c.a[3]));
        SetRam8(light + kLightWay, Mem8(c, c.a[3] + 1));
        SetRam16(light + kLightLeft, Mem16(c, c.a[3] + 2));
        c.a[3] += 4;
        SetRam32(light + kLightList, c.a[3]);
    }
    void LightMoves(M68K &c) {
        const uint32_t light = c.a[4] & 0xFFFFFF;
        c.a[1] = kScript14Ways;
        c.d[0] = Ram8(light + kLightWay);
        SetLow8(c.d[1], Ram8(light + kLightDouble));
        for (uint32_t n = 0; n < 2; n++) {
            c.d[2] = Mem32(c, c.a[1] + c.d[0] + n * 4);
            if (c.d[1] & 0xFF) {
                SetLow16(c.d[2], c.d[2] * 2); // (the low half only: the game's own)
            }
            SetRam32(light + (n ? kLightY : kLightX), Ram32(light + (n ? kLightY : kLightX)) + c.d[2]);
        }
    }
    void Script14(M68K &c) {
        static constexpr Steps kSteps = {0x07E20E, {0x07E222, 0x07E2A8, 0x07E2F6, 0x07E2F6}};
        const uint32_t piece = c.a[6] & 0xFFFFFF;
        auto listOf = [&](uint32_t number) { // (the lists' table has each list's distance from the table)
            SetLow16(c.d[1], Mem16(c, kScript14Lists + number * 2));
            c.a[1] = kScript14Lists + static_cast<uint32_t>(static_cast<int16_t>(c.d[1]));
            return c.a[1];
        };
        switch (StepAt(c, kSteps)) {
        case 0:
            NextStep(piece);
            c.d[6] = Ram16(piece + kY);
            SetLow8(c.d[6], c.d[6] + Ram8(piece + kWhich));
            c.a[0] = kScript14Lists;
            c.d[0] = Ram8(piece + kWhich) * 2u;
            SetRam32(piece + 0x30, listOf(Ram8(piece + kWhich)));
            SetLow16(c.d[0], c.d[6] * 2);
            SetRam32(piece + 0x40 + kLightList, listOf(c.d[6] & 0xFFFF));
            SetLow16(c.d[0], c.d[0] + 8);
            SetRam32(piece + 0x60 + kLightList, listOf((c.d[6] & 0xFFFF) + 4));
            LightAnimation(c);
            c.a[4] = c.a[6] + 0x40;
            LightNextMove(c);
            LightPlaced(c);
            LightKeepsFrame(c);
            SetRam16(piece + kX, Ram16(piece + 0x40 + kLightX));
            SetRam16(piece + kY, Ram16(piece + 0x40 + kLightY));
            SetLow16(c.d[6], c.d[6] + 4);
            LightAnimation(c);
            c.a[4] = c.a[6] + 0x60;
            LightNextMove(c);
            LightPlaced(c);
            LightKeepsFrame(c);
            break;
        case 2:
            if (!Frozen(c)) {
                c.a[4] = c.a[6] + ((Ram8(Var(c, kOddFrame)) & 1) ? 0x60 : 0x40);
                const uint32_t light = c.a[4] & 0xFFFFFF;
                SetRam16(light + kLightLeft, static_cast<uint16_t>(Ram16(light + kLightLeft) - 1));
                if (Ram16(light + kLightLeft) & 0x8000) {
                    c.d[6] = Ram8(piece + kWhich);
                    LightNextMove(c);
                }
                LightMoves(c);
                SetRam32(piece + kFrame, Ram32(light));
                SetRam32(piece + kTimer, Ram32(light + 4));
                Animate(c);
                LightKeepsFrame(c);
                SetRam16(piece + kX, Ram16(light + kLightX));
                SetRam16(piece + kY, Ram16(light + kLightY));
            }
            PieceDrawn(c);
            break;
        default:
            GiveBack(c);
            break;
        }
    }

    // --- pieces let go by other pieces ---

    // 07ED72 (script 19, Deserted Chateau: let go by script 18): one of two animations by chance; a frame
    // with an event above nothing takes it up 3, one below nothing ends it
    void Script19(M68K &c) {
        static constexpr Steps kSteps = {0x07ED72, {0x07ED86, 0x07EDBA, 0x07EDD8, 0x07EDD8}};
        const uint32_t piece = c.a[6] & 0xFFFFFF;
        switch (StepAt(c, kSteps)) {
        case 0:
            NextStep(piece);
            SetRam16(piece + kPlaces + 2, 0x6000);
            SetRam16(piece + kPlaces, 0x2000);
            SetRam8(piece + kFacesRight, 0);
            SetRam8(piece + kDepth, 6);
            SetRam8(piece + kIsRiding, 8);
            c.d[6] = 7;
            Call(c, kRandom);
            if (!(c.d[0] & 0x80)) {
                c.d[6] = 0x0C;
            }
            SetLow8(c.d[0], c.d[6]);
            SetStageAnimation(c);
            break;
        case 2:
            Animate(c);
            if (Ram8(piece + kEvent) & 0x80) {
                NextStep(piece);
                break;
            }
            if (Ram8(piece + kEvent)) {
                SetRam16(piece + kY, static_cast<uint16_t>(Ram16(piece + kY) + 3));
            }
            PieceDrawn(c);
            break;
        default:
            GiveBack(c);
            break;
        }
    }

    // 07EDDE (script 20, Deserted Chateau and Fetus of God: made by scripts 18 and 45): at a place of four
    // by kWhich; one of two animations by chance, begun again after a wait made up anew each time
    void Script20(M68K &c) {
        static constexpr Steps kSteps = {0x07EDDE, {0x07EDF2, 0x07EE64, 0x07EE8A, 0x07EE8A}};
        const uint32_t piece = c.a[6] & 0xFFFFFF;
        switch (StepAt(c, kSteps)) {
        case 0:
            NextStep(piece);
            SetRam16(piece + kPlaces + 2, 0x6000);
            SetRam16(piece + kPlaces, 0x2000);
            Call(c, kRandom);
            SetLow16(c.d[0], c.d[0] & 0x3F);
            SetRam16(piece + kTurn, static_cast<uint16_t>(c.d[0]));
            SetRam8(piece + kDepth, 6);
            SetRam8(piece + kIsRiding, 8);
            SetRam16(piece + kRow, 0x48);
            SetRam8(piece + kFacesRight, Ram8(piece + kWhich) >> 1);
            c.d[0] = Ram8(piece + kWhich) * 4u;
            SetRam16(piece + kX, ProgramWord(c, 0x07EE54 + c.d[0]));
            SetRam16(piece + kY, ProgramWord(c, 0x07EE56 + c.d[0]));
            c.d[6] = 8;
            Call(c, kRandom);
            if (!(c.d[0] & 0x80)) {
                c.d[6] = 0x0D;
            }
            SetLow8(c.d[0], c.d[6]);
            SetStageAnimation(c);
            break;
        case 2:
            SetRam16(piece + kTurn, static_cast<uint16_t>(Ram16(piece + kTurn) - 1));
            if ((Ram16(piece + kTurn) & 0x8000) && (Ram8(piece + kOver) & 0x80)) {
                Call(c, kRandom);
                SetLow16(c.d[0], c.d[0] & 0x3F);
                SetRam16(piece + kTurn, static_cast<uint16_t>(c.d[0]));
            }
            Animate(c);
            PieceDrawn(c);
            break;
        default:
            GiveBack(c);
            break;
        }
    }

    // 07EF7C (script 23, Abaraya: let go by script 22): thrown up and to one side, by the count its maker
    // gave it (which side and how far forward by its low two bits, how hard across by the high byte), and
    // falls; done when its animation ends
    void Script23(M68K &c) {
        static constexpr Steps kSteps = {0x07EF7C, {0x07EF90, 0x07F030, 0x07F04C, 0x07F04C}};
        constexpr uint32_t kPullX = 0x48;
        const uint32_t piece = c.a[6] & 0xFFFFFF;
        switch (StepAt(c, kSteps)) {
        case 0:
            NextStep(piece);
            SetRam32(piece + kSpeedX, 0);
            SetRam32(piece + kSpeedY, 0x18000);
            SetRam32(piece + kPullY, 0xFFFFF400);
            SetLow16(c.d[0], (Ram16(piece + kTurn) & 3) * 2);
            SetRam8(piece + kFacesRight, c.opcodes[0x07EFE8 + (c.d[0] & 0xFFFF)]);
            SetRam8(piece + kDepth, c.opcodes[0x07EFE9 + (c.d[0] & 0xFFFF)]);
            SetLow8(c.d[0], Ram8(piece + kTurn));
            SetLow16(c.d[0], (c.d[0] & 0x0F) << 2);
            SetRam32(piece + kPullX, (static_cast<uint32_t>(ProgramWord(c, 0x07EFF0 + (c.d[0] & 0xFFFF))) << 16) |
                                         ProgramWord(c, 0x07EFF2 + (c.d[0] & 0xFFFF)));
            if (!Ram8(piece + kFacesRight)) {
                SetRam32(piece + kSpeedX, 0u - Ram32(piece + kSpeedX));
                SetRam32(piece + kPullX, 0u - Ram32(piece + kPullX));
            }
            SetStageAnimation(c, 8);
            break;
        case 2:
            if (Ram8(piece + kOver)) {
                NextStep(piece);
            }
            Call(c, kMoveBySpeed);
            Animate(c);
            PieceDrawn(c);
            break;
        default:
            GiveBack(c);
            break;
        }
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
    AddRewrite(0x07E672, "TakeLeg", TakeLeg, kFreeFlags, 180);
    AddRewrite(0x07E77E, "FlyLeg", FlyLeg, kFreeFlags, 180);
    AddRewrite(0x07EA82, "Script17", Script17, kFreeFlags, 200);
    AddRewrite(0x07E62C, "Script16", Script16, kFreeFlags, 200);
    AddRewrite(0x07F2F0, "Script27", Script27, kFreeFlags, 250);
    AddRewrite(0x07ED72, "Script19", Script19, kFreeFlags, 150);
    AddRewrite(0x07EDDE, "Script20", Script20, kFreeFlags, 150);
    AddRewrite(0x07EF7C, "Script23", Script23, kFreeFlags, 200);
    AddRewrite(0x07F438, "Script28", Script28, kFreeFlags, 200);
    AddRewrite(0x07E20E, "Script14", Script14, kFreeFlags, 400);
    AddRewrite(0x07FE5A, "Script40", Script40, kFreeFlags, 200);
    AddRewrite(0x07EBEA, "Script18", Script18, kFreeFlags, 200);
    AddRewrite(0x0800EC, "Script45", Script45, kFreeFlags, 150);
    AddRewrite(0x07D710, "Script6", Script6, kFreeFlags, 200);
    AddRewrite(0x07F052, "Script24", Script24, kFreeFlags, 150);
    AddRewrite(0x07FCF0, "Script38", Script38, kFreeFlags, 150);
    AddRewrite(0x07DF9C, "Script11", Script11, kFreeFlags, 150);
    AddRewrite(0x07D59C, "Script5", Script5, kFreeFlags, 200);
    AddRewrite(0x07F202, "Script26", Script26, kFreeFlags, 250);
    AddRewrite(0x07FC0C, "Script37", Script37, kFreeFlags, 150);
    AddRewrite(0x07FFDE, "Script43", Script43, kFreeFlags, 200);
    AddRewrite(0x080462, "Script48", Script48, kFreeFlags, 150);
    AddRewrite(0x08087C, "Script55", Script55, kFreeFlags, 150);
    AddRewrite(0x07DF1E, "Script10", Script10, kFreeFlags, 120);
    AddRewrite(0x080378, "Script46", Script46, kFreeFlags, 150);
    AddRewrite(0x0803E8, "Script47", Script47, kFreeFlags, 180);
    AddRewrite(0x07DE32, "Script8", Script8, kFreeFlags, 100);
    AddRewrite(0x0809E4, "Script57", Script57, kFreeFlags, 100);
    AddRewrite(0x080970, "Script56", Script56, kFreeFlags, 100);
    AddRewrite(0x08083A, "Script54", Script54, kFreeFlags, 120);
    AddRewrite(0x07EEEC, "Script22", Script22, kFreeFlags, 120);
    AddRewrite(0x07E162, "Script13", Script13, kFreeFlags, 150);
    AddRewrite(0x07E5A8, "Script15", Script15, kFreeFlags, 120);
    AddRewrite(0x07F9E4, "Script32", Script32, kFreeFlags, 120);
    AddRewrite(0x07F79E, "Script29", Script29, kFreeFlags, 120);
    AddRewrite(0x07F870, "Script30", Script30, kFreeFlags, 150);
    AddRewrite(0x07F928, "Script31", Script31, kFreeFlags, 150);
    AddRewrite(0x07FE06, "Script39", Script39, kFreeFlags, 120);
    AddRewrite(0x07FB02, "Script34", Script34, kFreeFlags, 120);
    AddRewrite(0x07FF76, "Script41", Script41, kFreeFlags, 120);
    AddRewrite(0x07FBC4, "Script36", Script36, kFreeFlags, 120);
    AddRewrite(0x07FB5E, "Script35", Script35, kFreeFlags, 120);
    AddRewrite(0x07DECE, "Script9", Script9, kFreeFlags, 120);
    AddRewrite(0x07E10E, "Script12", Script12, kFreeFlags, 120);
    AddRewrite(0x080768, "Script52", Script52, kFreeFlags, 120);
    AddRewrite(0x0806FA, "Script51", Script51, kFreeFlags, 120);
    AddRewrite(0x07DDD8, "Script7", Script7, kFreeFlags, 120);
    AddRewrite(0x07EE90, "Script21", Script21, kFreeFlags, 120);
    AddRewrite(0x07F1A4, "Script25", Script25, kFreeFlags, 120);
    AddRewrite(0x080094, "Script44", Script44, kFreeFlags, 120);
    AddRewrite(0x07FA82, "Script33", Script33, kFreeFlags, 120);
    AddRewrite(0x0807C0, "Script53", Script53, kFreeFlags, 120);
    // (the frames that begin with a test of the camera: the script, the place, which side, where it goes on)
}

} // namespace cps2recomp
