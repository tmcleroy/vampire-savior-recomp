// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy
//
// Vampire Savior: the placing of a stage's scroll screens written again as C++ (rewrites: lift.h). The
// stage's own plane is the camera (camera.cpp). The other two, the scroll screens of 8- and 32-pixel
// tiles, are placed from it each frame: each goes at a rate of the camera's travel from its start, so
// that what is further back goes by more slowly, or follows a row of the stage's row scroll table (the
// floor, whose rows each go at their own rate). Which rate is the stage's choice (the table at the end).
#include "stage.h"
#include "widescreen.h"

namespace cps2recomp {

using namespace vsav;

const Layer kLayer8 = {0x20, 0x01EEFA, 0x01EE22, 0x01EE58, 0x01EF16, 0x01EFA0, 0x22, 0x24, 0x1FF};
const Layer kLayer16 = {0x10, 0x01F31E, 0x01F280, 0x01F2B6, 0x01F33A, 0x01F3C4, 0x26, 0x28, 0x3FF};
const Layer kLayer32 = {0x20, 0x01F71C, 0x01F64E, 0x01F682, 0x01F73A, 0x01F7C2, 0x2A, 0x2C, 0x7FF};

int64_t WideLag(int64_t lag) {
    const int side = WideMargin();
    const int wide = static_cast<int16_t>(Ram16(kCameraRecord + kCameraEndRight) - Ram16(kCameraRecord + kLayerStartX));
    return side && wide > 0 ? lag * (wide + side) / wide : lag;
}

namespace {
    int16_t Signed(uint32_t word) {
        return static_cast<int16_t>(word);
    }

    void PlaceLayer(M68K &c, const Layer &layer, bool across) {
        const uint32_t record = c.a[6] & 0xFFFFFF;
        const uint32_t at = across ? kLayerX : kLayerY, going = across ? kLayerGoingX : kLayerGoingY;
        const uint32_t strip = across ? kLayerStripX : kLayerStripY;
        SetLow16(c.d[0], c.d[0] + Ram16(record + (across ? kLayerShakeX : kLayerShakeY)));
        const bool back = Signed(c.d[0]) < Signed(Ram16(record + (across ? kLayerBeforeX : kLayerBeforeY)));
        SetRam8(record + going, across ? (back ? 0x08 : 0x0C) : (back ? 0x04 : 0x00));
        SetRam16(record + at, static_cast<uint16_t>(c.d[0]));
        if (!Ram8(record + kLayerDrawnAsItGoes)) {
            return;
        }
        // (the strip's bit is kept turned over: alike, and a line has been crossed)
        SetLow8(c.d[1], Ram8(record + strip));
        SetLow8(c.d[0], (c.d[0] & layer.strip) ^ c.d[1]);
        if (c.d[0] & 0xFF) {
            return;
        }
        SetRam8(record + strip, Ram8(record + strip) ^ layer.strip);
        SetLow8(c.d[0], Ram8(record + going));
        Call(c, layer.goingTo);
        Call(c, layer.column);
        Call(c, layer.row);
        Call(c, across ? layer.drawsAcross : layer.drawsUp);
    }
} // namespace

void PlaceLayerAcross(M68K &c, const Layer &layer) {
    PlaceLayer(c, layer, true);
}
void PlaceLayerUp(M68K &c, const Layer &layer) {
    PlaceLayer(c, layer, false);
}

namespace {
    // How a plane goes as the camera does
    enum Rate {
        kStays,       // not at all
        kHalf,
        kFiveEighths,
        kThreeQuarters,
        kSevenEighths,
        kFifteenSixteenths,
        kWhole,       // as the camera
        kRow,         // as a row of the row scroll table (so many bytes on from the stage's own plane's row)
    };

    // The camera's travel from its start, at a rate, as the game works it out (D0; D1 is what it uses on the way)
    void AtRate(M68K &c, Rate rate) {
        const int16_t travel = Signed(c.d[0]);
        switch (rate) {
        case kHalf:
            SetLow16(c.d[0], travel >> 1);
            break;
        case kFiveEighths:
            SetLow16(c.d[1], travel << 2);
            SetLow16(c.d[0], Signed(travel + c.d[1]) >> 3);
            break;
        case kThreeQuarters:
        case kSevenEighths:
        case kFifteenSixteenths:
            SetLow16(c.d[1], travel >> (rate == kThreeQuarters ? 2 : rate == kSevenEighths ? 3 : 4));
            SetLow16(c.d[0], travel - c.d[1]);
            break;
        default:
            break;
        }
    }

    // 01DE40-01DECE, 01E75E-01E7DA. The plane at A6 is placed across from the camera
    void LayerAcross(M68K &c, const Layer &layer, Rate rate, int row = 0) {
        const uint32_t record = c.a[6] & 0xFFFFFF;
        SetLow16(c.d[0], Ram16(kCameraRecord + kLayerX) - Ram16(kCameraRecord + kLayerStartX));
        if (rate == kRow) {
            // (a row's entry is where the stage's middle is in the picture on that row: 192 at the start)
            c.a[0] = Ram32(Var(c, kRowsTable));
            SetLow16(c.d[1], Ram16(Var(c, kRowsAt)));
            SetLow16(c.d[0], c.d[0] + Mem16(c, c.a[0] + static_cast<uint32_t>(Signed(c.d[1]) + row)) - 0xC0);
        } else {
            SetLow16(c.d[0], c.d[0] + Ram16(Var(c, kRowsLead)));
            const int16_t travel = Signed(c.d[0]);
            AtRate(c, rate);
            if (WideMargin()) {
                SetLow16(c.d[0], travel - WideLag(travel - Signed(c.d[0]))); // (stage.h)
            }
        }
        SetLow16(c.d[0], c.d[0] + Ram16(record + kLayerStartX));
        PlaceLayerAcross(c, layer);
    }

    // 01DF3A-01DF64, 01E846-01E86A. ... and up
    void LayerUp(M68K &c, const Layer &layer, Rate rate) {
        const uint32_t record = c.a[6] & 0xFFFFFF;
        if (rate == kStays) {
            SetLow16(c.d[0], Ram16(record + kLayerStartY));
        } else {
            SetLow16(c.d[0], Ram16(kCameraRecord + kLayerY) - Ram16(kCameraRecord + kLayerStartY));
            AtRate(c, rate);
            SetLow16(c.d[0], c.d[0] + Ram16(record + kLayerStartY));
        }
        PlaceLayerUp(c, layer);
    }

    // 01DFC0, 01E4F0, 01E8C6. The frame's place of the layer at A6 is done: it is where things riding on
    // it are placed against, and what the scroll registers will be
    void LayerPlaced(M68K &c, const Layer &layer) {
        const uint32_t record = c.a[6] & 0xFFFFFF;
        const uint16_t x = Ram16(record + kLayerX), y = Ram16(record + kLayerY);
        SetRam16(record + kLayerShownX, x);
        SetRam16(record + kLayerShownY, y);
        SetLow16(c.d[0], (x + Ram16(record + kLayerNudgeX)) & layer.registerMask);
        SetRam16(Var(c, layer.registerX), static_cast<uint16_t>(c.d[0]));
        SetLow16(c.d[1], (y - Ram16(Var(c, kStageDip)) + Ram16(record + kLayerNudgeY)) & layer.registerMask);
        SetRam16(Var(c, layer.registerY), static_cast<uint16_t>(c.d[1]));
    }

    template <const Layer &layer, Rate rate, int row = 0>
    void Across(M68K &c) {
        LayerAcross(c, layer, rate, row);
    }
    template <const Layer &layer, Rate rate>
    void Up(M68K &c) {
        LayerUp(c, layer, rate);
    }
    template <const Layer &layer, bool across>
    void Place(M68K &c) {
        PlaceLayer(c, layer, across);
    }
    template <const Layer &layer>
    void Placed(M68K &c) {
        LayerPlaced(c, layer);
    }
} // namespace

void RegisterPlanes() {
    // (the last number is the time a call is charged: the game's own, as the check measured it)
    AddRewrite(0x01DE40, "Back8AcrossHalf", Across<kLayer8, kHalf>, kFreeFlags, 186);
    AddRewrite(0x01DE56, "Back8AcrossThreeQuarters", Across<kLayer8, kThreeQuarters>, kFreeFlags, 160);
    AddRewrite(0x01DE6E, "Back8AcrossSevenEighths", Across<kLayer8, kSevenEighths>, kFreeFlags, 198);
    AddRewrite(0x01DE86, "Back8AcrossFifteenSixteenths", Across<kLayer8, kFifteenSixteenths>, kFreeFlags, 182);
    AddRewrite(0x01DE9E, "Back8AcrossWhole", Across<kLayer8, kWhole>, kFreeFlags, 178);
    AddRewrite(0x01DEB0, "Back8AcrossRowBefore", Across<kLayer8, kRow, -8>, kFreeFlags, 216);
    AddRewrite(0x01DECE, "Back8AcrossRowAfter", Across<kLayer8, kRow, 8>, kFreeFlags, 206);
    AddRewrite(0x01DEEA, "Back8PlaceAcross", Place<kLayer8, true>, kFreeFlags, 113);
    AddRewrite(0x01DF3A, "Back8UpStays", Up<kLayer8, kStays>, kFreeFlags, 142);
    AddRewrite(0x01DF40, "Back8UpHalf", Up<kLayer8, kHalf>, kFreeFlags, 138);
    AddRewrite(0x01DF50, "Back8UpThreeQuarters", Up<kLayer8, kThreeQuarters>, kFreeFlags, 184);
    AddRewrite(0x01DF64, "Back8UpWhole", Up<kLayer8, kWhole>, kFreeFlags, 150);
    AddRewrite(0x01DF70, "Back8PlaceUp", Place<kLayer8, false>, kFreeFlags, 113);
    AddRewrite(0x01DFC0, "Back8Placed", Placed<kLayer8>, kFreeFlags, 124);
    AddRewrite(0x01E4F0, "CameraPlaced", Placed<kLayer16>, kFreeFlags, 124);
    AddRewrite(0x01E75E, "Far32AcrossFiveEighths", Across<kLayer32, kFiveEighths>, kFreeFlags, 172);
    AddRewrite(0x01E77A, "Far32AcrossThreeQuarters", Across<kLayer32, kThreeQuarters>, kFreeFlags, 160);
    AddRewrite(0x01E792, "Far32AcrossSevenEighths", Across<kLayer32, kSevenEighths>, kFreeFlags, 162);
    AddRewrite(0x01E7AA, "Far32AcrossWhole", Across<kLayer32, kWhole>, kFreeFlags, 142);
    AddRewrite(0x01E7BC, "Far32AcrossRow16", Across<kLayer32, kRow, -0x10>, kFreeFlags, 180);
    AddRewrite(0x01E7DA, "Far32AcrossRow44", Across<kLayer32, kRow, -0x2C>, kFreeFlags, 170);
    AddRewrite(0x01E7F6, "Far32PlaceAcross", Place<kLayer32, true>, kFreeFlags, 85);
    AddRewrite(0x01E846, "Far32UpHalf", Up<kLayer32, kHalf>, kFreeFlags, 138);
    AddRewrite(0x01E856, "Far32UpThreeQuarters", Up<kLayer32, kThreeQuarters>, kFreeFlags, 148);
    AddRewrite(0x01E86A, "Far32UpWhole", Up<kLayer32, kWhole>, kFreeFlags, 120);
    AddRewrite(0x01E876, "Far32PlaceUp", Place<kLayer32, false>, kFreeFlags, 85);
    AddRewrite(0x01E8C6, "Far32Placed", Placed<kLayer32>, kFreeFlags, 124);
}

} // namespace cps2recomp
