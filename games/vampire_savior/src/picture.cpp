// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy
//
// Vampire Savior: what is in the picture, written again as C++ (rewrites: lift.h). Everything the game
// draws as objects has a record (at A6 here), and each frame the record's own code ends by asking one of a
// few shared routines for it to be drawn. Those look at where the object is against the scroll screen it
// rides on, and put what is in the picture (and a margin about it) on one of the frame's lists of things
// to draw, by the object's kind and how far forward it is; what is not on a list is not drawn.
//
// In a wide picture (widescreen.h) the same is asked of the wide one: its 64 columns more on each side.
#include "stage.h"
#include "widescreen.h"

namespace cps2recomp {

namespace {
    using namespace vsav;
    constexpr uint16_t kOwnRow = 0x7B0, kMiddle = 0xC0; // (rows.cpp)
} // namespace

bool AcrossInPicture(M68K &c, uint32_t object, int &across) {
    const uint8_t rides = Ram8(object + kIsRiding);
    if (rides != 4 && rides != 8 && rides != 12) {
        return false;
    }
    across = static_cast<int16_t>(Ram16(object + kX) - Ram16(kLayerRecords[rides >> 2] + kLayerShownX));
    if (rides == 8 && Ram16(object + 0x24)) {
        const uint32_t row = Ram32(Var(c, kRowsTable)) + static_cast<uint32_t>(static_cast<int16_t>(kOwnRow - Ram16(object + 0x24) * 2));
        across -= static_cast<int16_t>(Mem16(c, row) - kMiddle);
    }
    return true;
}

namespace {
    // More of an object's record
    constexpr uint32_t kDrawn = 0x01;    // byte: 1 when the object is in the picture this frame
    constexpr uint32_t kKind = 0x08;     // byte: what it is, for which list it goes on (kIsThrown: 0 for a player)
    constexpr uint32_t kDepth = 0x09;    // byte: which of its kind's lists (how far forward it is drawn), in twos
    constexpr uint32_t kRow = 0x24;      // word: riding the stage's own plane: rows above the stage's own row (0: none)
    constexpr uint32_t kDone = 0x04;     // byte: its step; 6 has it done away with
    constexpr uint32_t kWhile = 0x28;    // long: a piece of the stage that is drawn only while the camera is to one side of a place
    // The frame's lists of records to draw: each a place that goes down as records are added (longs, from
    // A5) and how many (bytes). The first is all the things thrown, for the hit tests too (kThrownInUse).
    constexpr int32_t kLists = 0x7982, kListCounts = 0x79C5;
    enum { kListThrown = 0, kListThrownBehind = 1, kListThrownInFront = 2, kListEight = 3, kListTwelve = 4, kListPieces = 11 };
    constexpr int32_t kNothingDrawn = 0x118; // from A5, a byte: not zero: no piece of the stage is drawn
    // The board's picture, and the margins about it inside which a thing is still drawn
    constexpr int kMarginLeft = 0x40, kMarginTop = 0x40, kAcross = 0x200, kAcrossWide = 0x280, kUp = 0x180;
    constexpr int kThrownMarginLeft = 0x60, kThrownAcross = 0x240;
    constexpr int kPictureAcross = 0x180;

    // 01559E. The record of the scroll screen the object rides on, in A3
    void PlaneOf(M68K &c) {
        c.d[0] = Ram8((c.a[6] & 0xFFFFFF) + kIsRiding);
        c.a[3] = kLayerRecords[(c.d[0] >> 2) & 3];
    }

    // 015572. ... and in D0 how far its row of the stage's own plane is moved from the plane's place
    void PlaneAndRow(M68K &c) {
        const uint32_t object = c.a[6] & 0xFFFFFF;
        c.d[0] = 0;
        c.d[1] = Ram8(object + kIsRiding);
        if (c.d[1] == 8) {
            SetLow16(c.d[2], Ram16(object + kRow) * 2);
            c.a[3] = Ram32(Var(c, kRowsTable));
            SetLow16(c.d[0], Mem16(c, c.a[3] + static_cast<uint32_t>(static_cast<int16_t>(kOwnRow - c.d[2]))) - kMiddle);
        }
        c.a[3] = kLayerRecords[(c.d[1] >> 2) & 3];
    }

    // The record at A6 goes on a list of the frame's
    void AddToList(M68K &c, int list) {
        const uint32_t end = Var(c, kLists + 4 * list), count = Var(c, kListCounts + list);
        c.a[0] = Ram32(end) - 2;
        c.Write(c.a[0], 1, c.a[6] & 0xFFFF);
        SetRam32(end, c.a[0]);
        SetRam8(count, Ram8(count) + 1);
    }

    // 015696. A piece of the stage is drawn: on the pieces' list for its depth
    void PieceDrawn(M68K &c) {
        c.d[0] = Ram8((c.a[6] & 0xFFFFFF) + kDepth);
        SetLow16(c.d[1], 0x08 + c.d[0] * 8); // (what the game's own way to a list's code leaves there)
        AddToList(c, kListPieces + static_cast<int>(c.d[0] / 2));
    }

    // 015546. The object at A6 is drawn this frame
    void Drawn(M68K &c) {
        const uint32_t object = c.a[6] & 0xFFFFFF;
        SetRam8(object + kDrawn, 1);
        c.d[0] = Ram8(object + kKind);
        // (the game goes on to the code for the object's kind through A3)
        static constexpr uint32_t kThen[5] = {0x01555C, 0x0155BA, 0x0155FC, 0x01560C, 0x015698};
        c.a[3] = kThen[c.d[0] <= 0x10 ? c.d[0] >> 2 : 0];
        switch (c.d[0]) {
        case 0x04: // (a thing thrown)
            AddToList(c, Ram8(object + kDepth) ? kListThrownInFront : kListThrownBehind);
            AddToList(c, kListThrown);
            break;
        case 0x08:
            AddToList(c, kListEight);
            break;
        case 0x0C:
            SetLow8(c.d[0], Ram8(object + kDepth));
            SetLow16(c.d[1], 0x0E + (c.d[0] & 0xFF) * 8);
            AddToList(c, kListTwelve + static_cast<int>((c.d[0] & 0xFF) / 2));
            break;
        case 0x10:
            PieceDrawn(c);
            break;
        default:
            break;
        }
    }

    // How far the object is right of the left edge of the scroll screen it rides on, and below its top
    // edge... as the game reckons: plus a margin, so that one comparison says "outside"
    bool Outside(M68K &c, int marginLeft, int across) {
        const uint32_t object = c.a[6] & 0xFFFFFF, plane = c.a[3];
        const int side = WideMargin(); // (the wide picture is that much more on each side: widescreen.h)
        SetLow16(c.d[1], Ram16(object + kX) - Mem16(c, plane + kLayerShownX) + c.d[0] + marginLeft);
        if (static_cast<uint16_t>(c.d[1] + side) > across + 2 * side) {
            return true;
        }
        SetLow16(c.d[1], Ram16(object + kY) - Mem16(c, plane + kLayerShownY) + kMarginTop);
        return (c.d[1] & 0xFFFF) > kUp;
    }

    // 0154FE, 01551A. The object at A6 is drawn if it is in the picture: within a margin of it that is
    // wider for the first of these
    void DrawnIfIn(M68K &c, int across) {
        const uint32_t object = c.a[6] & 0xFFFFFF;
        if (!Ram8(object + kActive)) {
            return;
        }
        PlaneAndRow(c);
        if (Outside(c, kMarginLeft, across)) {
            SetRam8(object + kDrawn, 0);
        } else {
            Drawn(c);
        }
    }
    void DrawnIfInWide(M68K &c) {
        DrawnIfIn(c, kAcrossWide);
    }
    void DrawnIfInPicture(M68K &c) {
        DrawnIfIn(c, kAcross);
    }

    // 01549A. A thing thrown that has left the picture is done away with
    void ThrownGone(M68K &c) {
        PlaneAndRow(c);
        if (Outside(c, kThrownMarginLeft, kThrownAcross)) {
            SetRam8((c.a[6] & 0xFFFFFF) + kDone, 6);
        }
    }

    // Is the object within kNear of the board's picture, across, where it is drawn?
    constexpr int kNear = 192;
    bool NearPicture(M68K &c, uint32_t object) {
        int across = 0;
        return AcrossInPicture(c, object, across) && across >= -kNear && across < kPictureAcross + kNear;
    }

    // 0154CC. A piece of the stage that is drawn only while the camera is to one side of a place: left of
    // the long's first word; or, that word being below nothing, right of its second (both below nothing:
    // always). Such a piece is one of two that share a part of the stage's memory for objects' pictures.
    void PieceByCamera(M68K &c) {
        const uint32_t object = c.a[6] & 0xFFFFFF;
        if (Ram8(Var(c, kNothingDrawn)) || !Ram8(object + kActive)) {
            return;
        }
        const uint16_t camera = Ram16(kCameraX);
        const int side = WideMargin(); // (asked of the wide picture's edges: widescreen.h)
        const uint32_t places = Ram32(object + kWhile);
        const uint16_t leftOf = static_cast<uint16_t>(places >> 16), rightOf = static_cast<uint16_t>(places);
        bool drawn;
        if (!(places & 0x80000000)) {
            c.d[1] = (places << 16) | leftOf;
            drawn = leftOf >= static_cast<uint16_t>(camera - side);
        } else if (rightOf & 0x8000) {
            c.d[1] = (places << 16) | leftOf;
            drawn = true;
        } else {
            c.d[1] = places;
            drawn = rightOf < static_cast<uint16_t>(camera + side);
        }
        // (the places are for the board's picture and take a piece to go by as the camera does. One that
        // rides a plane or a row going more slowly is still in a wide picture when they say it has left:
        // there, a piece is drawn too while it is itself anywhere near the picture)
        if (!drawn && side && NearPicture(c, object)) {
            drawn = true;
        }
        if (drawn) {
            c.d[0] = 0;
            PieceDrawn(c);
        }
    }

    // 01BBC2, 01BC3A. Where the object at A6 is in the picture, for drawing it: D4, up in its first word
    // (counted down from the picture's top, as the board has it) and across in its second (the board's
    // picture starts at 64). D2 is the object's height in the stage. The first is for the players and what
    // they throw: a player (kKind 0) has a nudge of its own each way, and only some objects dip with the
    // stage when it shakes.
    constexpr uint32_t kDips = 0x0D;                   // byte: not zero: the object goes down with the stage's dip
    constexpr uint32_t kNudgeX = 0x10C, kNudgeY = 0x10E; // words, a player's: added to its place in the picture
    constexpr int kPictureLeft = 0x40, kPictureTop = 0x100;

    void PlaceInPicture(M68K &c, bool ofPlayers) {
        const uint32_t object = c.a[6] & 0xFFFFFF;
        const bool player = ofPlayers && !Ram8(object + kKind);
        c.d[4] = Ram8(object + kIsRiding);
        c.d[0] = 0;
        if (c.d[4] == 8) {
            // (riding a row of the stage's own plane: how far that row is moved from the plane's place)
            SetLow16(c.d[1], Ram16(object + kRow));
            if (c.d[1] & 0xFFFF) {
                SetLow16(c.d[1], c.d[1] * 2);
                c.a[0] = Ram32(Var(c, kRowsTable));
                const int16_t row = static_cast<int16_t>(Ram16(Var(c, kRowsAt)) - c.d[1]);
                SetLow16(c.d[0], Mem16(c, c.a[0] + static_cast<uint32_t>(row)) - kMiddle);
            }
        }
        c.a[0] = kLayerRecords[(c.d[4] >> 2) & 3];
        uint16_t height = static_cast<uint16_t>(c.d[2]);
        if (!ofPlayers || Ram8(object + kDips)) {
            height += Ram16(Var(c, kStageDip));
        }
        if (player) {
            height += Ram16(object + kNudgeY);
        }
        const uint16_t y = static_cast<uint16_t>(kPictureTop - (height - Mem16(c, c.a[0] + kLayerShownY)));
        uint16_t x = Ram16(object + kX);
        if (player) {
            x += Ram16(object + kNudgeX);
        }
        x = static_cast<uint16_t>(x - Mem16(c, c.a[0] + kLayerShownX) + kPictureLeft - c.d[0]);
        c.d[4] = (static_cast<uint32_t>(y) << 16) | x;
    }
    void PlaceOfPlayers(M68K &c) {
        PlaceInPicture(c, true);
    }
    void PlaceOfPieces(M68K &c) {
        PlaceInPicture(c, false);
    }

    // 01BC88. ... and of an object that rides no scroll screen: its place is in the picture already
    void PlaceOfPanel(M68K &c) {
        const uint16_t y = static_cast<uint16_t>(kPictureTop - c.d[2]);
        c.d[4] = (static_cast<uint32_t>(y) << 16) | static_cast<uint16_t>(Ram16((c.a[6] & 0xFFFFFF) + kX) + kPictureLeft);
    }
} // namespace

void RegisterPicture() {
    AddRewrite(0x01BBC2, "PlaceOfPlayers", PlaceOfPlayers, kFreeFlags, 221);
    AddRewrite(0x01BC3A, "PlaceOfPieces", PlaceOfPieces, kFreeFlags, 203);
    AddRewrite(0x01BC88, "PlaceOfPanel", PlaceOfPanel, kFreeFlags, 40);
    // (the last number is the time a call is charged: the game's own, as the check measured it)
    AddRewrite(0x01559E, "PlaneOf", PlaneOf, kFreeFlags, 34);
    AddRewrite(0x015572, "PlaneAndRow", PlaneAndRow, kFreeFlags, 117);
    AddRewrite(0x015696, "PieceDrawn", PieceDrawn, kFreeFlags, 106);
    AddRewrite(0x015546, "Drawn", Drawn, kFreeFlags, 158);
    AddRewrite(0x0154FE, "DrawnIfInWide", DrawnIfInWide, kFreeFlags, 484);
    AddRewrite(0x01551A, "DrawnIfInPicture", DrawnIfInPicture, kFreeFlags, 370);
    AddRewrite(0x01549A, "ThrownGone", ThrownGone, kFreeFlags, 255);
    AddRewrite(0x0154CC, "PieceByCamera", PieceByCamera, kFreeFlags, 136);
}

} // namespace cps2recomp
