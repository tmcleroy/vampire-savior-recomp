// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy
//
// Vampire Savior: the making of a stage's row scroll table written again as C++ (rewrites: lift.h). The
// stage's own scroll screen (the one of 16-pixel tiles: the wall behind the players and the floor under
// them) is moved by its own amount on every line of the picture, from a table of a word a row that the
// game makes again each frame. What a row holds is where the stage's middle is in the picture on that row:
// 192 with the camera at its start, and so many times a step more or less than that as the row is further
// below or above the stage's own row. The step is the camera's travel from its start over a number of
// the stage's. So the floor's rows go faster the nearer they are, and the wall's go slower the higher up,
// down to a row above which everything goes at one rate. Ten stages have such a table; the last two have
// none.
#include "stage.h"

namespace cps2recomp {

namespace {
    using namespace vsav;

    // The table is made in four places in video RAM by turns (the board reads it a frame later)
    constexpr uint32_t kTables[4] = {0x90E000, 0x90E800, 0x90F000, 0x90F800};
    constexpr int16_t kOwnRow = 0x7B0;  // the stage's own row: bytes into a table (it is the same on every stage)
    constexpr uint16_t kMiddle = 0xC0;  // a row with the camera at its start

    // The row scroll table's record (kRowsTable, at A4 here)
    constexpr uint32_t kRowsStep = 0x04;                   // byte: 0 until the record is set up
    constexpr uint32_t kRowsRate = 0x0C;                   // long: a row's step (65536 a pixel)
    constexpr uint32_t kRowsCameraX = 0x10, kRowsCameraY = 0x14; // longs: the camera's place this frame
    constexpr uint32_t kRowsAbove = 0x18, kRowsBelow = 0x1C;     // longs: the steps added up, going up and down
    constexpr uint32_t kRowsOwn = 0x20;                    // word: kOwnRow
    constexpr uint32_t kRowsWave = 0x30, kRowsWaveWait = 0x31;   // bytes: where the wave is, and frames to its next step
    // While a super move's own backdrop is up, 112 rows of the table wave instead, by a table of sines
    constexpr int32_t kBackdrop = 0x133; // from A5, a byte
    constexpr uint32_t kWaveFrom = 0x470, kSines = 0x0B7740;
    constexpr int kWaveRows = 112;
    // The board's registers to be (from A5): the row scroll table's place, and the first row's number
    constexpr int32_t kRowsRegister = 0x1E, kFirstRowRegister = 0x2E;

    // A stage's table: the camera's travel over `divisor` is a row's step (0: over 128, and rounded down
    // where the others are rounded towards nothing). Below the stage's own row `floor` rows each a step
    // more than the one above, and `floorSame` more like the last. Above it `wall` rows each a step less,
    // then `wallSame` rows alike: at the rate of the last, of the row `wallSkips` further up, or (kOfRow) of
    // the row eight above the stage's own.
    struct Rows {
        int divisor, floor, floorSame, wall, wallSkips, wallSame;
    };
    constexpr int kOfRow = -1;
    constexpr Rows kRows[kStages] = {
        {0x68, 18, 22, 24, 8, 38},  // Feast of the Damned
        {0, 40, 0, 15, 0, 201},     // Concrete Cave
        {0x70, 40, 0, 72, 0, 0},    // Tower of Arrogance
        {0x50, 40, 0, 48, 0, 122},  // Red Thirst
        {0x70, 40, 0, 24, 8, 192},  // Deserted Chateau
        {0xC0, 40, 0, 1, 0, 215},   // Abaraya
        {0x70, 40, 0, 10, 0, 206},  // Vanity Paradise
        {0x48, 40, 0, 8, 0, 208},   // War Agony
        {0x60, 40, 0, 24, kOfRow, 192}, // Forever Torment
        {0x58, 40, 0, 8, 0, 208},   // Green Scream
        {},                         // Iron Horse, Iron Terror: no table
        {},                         // Fetus of God: none
    };

    void SetMem16(M68K &c, uint32_t address, uint32_t value) {
        c.Write(address, 1, value & 0xFFFF);
    }

    // 01FEA0-020236. The frame's table for a stage, at A3
    void MakeRows(M68K &c, const Rows &rows) {
        const uint32_t record = c.a[4] & 0xFFFFFF, table = c.a[3];
        const int32_t travel = static_cast<int32_t>(Ram32(record + kRowsCameraX) - Ram32(kCameraRecord + kLayerStartX));
        int32_t step;
        if (rows.divisor) {
            step = travel / rows.divisor;
            c.d[2] = static_cast<uint32_t>(rows.divisor);
            c.d[4] = (static_cast<uint32_t>(travel ^ rows.divisor) & 0xFFFF0000) | 0xFFFF;
            Spend(c, 1170); // (the game's own dividing, a bit at a time)
        } else {
            step = travel >> 7;
        }
        c.d[3] = static_cast<uint32_t>(step);
        SetRam32(record + kRowsRate, c.d[3]);
        SetRam32(record + kRowsAbove, 0);
        SetRam32(record + kRowsBelow, 0);

        const uint32_t own = table + static_cast<uint32_t>(static_cast<int16_t>(Ram16(record + kRowsOwn)));
        uint16_t row = 0;
        // (the floor, downwards from the stage's own row)
        uint32_t at = own;
        for (int n = 0; n < rows.floor; n++, at += 2) {
            row = static_cast<uint16_t>(kMiddle + Ram16(record + kRowsBelow));
            SetMem16(c, at, row);
            SetRam32(record + kRowsBelow, Ram32(record + kRowsBelow) + c.d[3]);
        }
        for (int n = 0; n < rows.floorSame; n++, at += 2) {
            SetMem16(c, at, row);
        }
        // (the wall, upwards. Its rows are slower than the camera: in a wide picture, slower again: stage.h)
        const uint32_t floorStep = c.d[3];
        c.d[3] = static_cast<uint32_t>(WideLag(static_cast<int32_t>(c.d[3])));
        at = own;
        for (int n = 0; n < rows.wall; n++) {
            SetRam32(record + kRowsAbove, Ram32(record + kRowsAbove) - c.d[3]);
            row = static_cast<uint16_t>(kMiddle + Ram16(record + kRowsAbove));
            SetMem16(c, at -= 2, row);
        }
        if (rows.wallSkips == kOfRow) {
            row = Mem16(c, own - 0x10);
        } else if (rows.wallSkips) {
            SetRam32(record + kRowsAbove, Ram32(record + kRowsAbove) - c.d[3] * static_cast<uint32_t>(rows.wallSkips));
            row = static_cast<uint16_t>(kMiddle + Ram16(record + kRowsAbove));
        }
        for (int n = 0; n < rows.wallSame; n++) {
            SetMem16(c, at -= 2, row);
        }
        c.d[3] = floorStep;
        SetLow16(c.d[0], row);
        c.d[1] = 0xFFFF;
        c.a[1] = at;
        Spend(c, 600 + static_cast<uint32_t>(62 * (rows.floor + rows.wall) + 19 * (rows.floorSame + rows.wallSame) + 34 * rows.wallSkips));
    }

    // 0202BE. A super move's backdrop: rows of the table wave
    void WaveRows(M68K &c) {
        const uint32_t record = c.a[4] & 0xFFFFFF;
        uint32_t at = Ram32(record) + kWaveFrom;
        c.a[0] = kSines;
        c.d[2] = kWaveRows - 1;
        SetLow8(c.d[0], Ram8(record + kRowsWave));
        SetRam8(record + kRowsWaveWait, Ram8(record + kRowsWaveWait) - 1);
        if (Ram8(record + kRowsWaveWait) & 0x80) {
            SetRam8(record + kRowsWaveWait, 3);
            SetLow16(c.d[0], (c.d[0] + 4) & 0x7C);
            SetRam8(record + kRowsWave, c.d[0]);
        }
        for (int n = 0; n < kWaveRows; n++, at += 2) {
            SetLow16(c.d[0], (c.d[0] + 4) & 0x7C);
            SetLow16(c.d[1], static_cast<uint16_t>((static_cast<int16_t>(Mem16(c, kSines + (c.d[0] & 0xFFFF))) >> 8) + kMiddle));
            SetMem16(c, at, c.d[1]);
        }
        SetLow16(c.d[2], 0xFFFF);
        c.a[3] = at;
        Spend(c, 74 * kWaveRows);
    }

    // 01FDFE. A frame of the row scroll table: the next of the four places is taken, the stage's table made
    // there from where the camera is, and the board's registers for it kept for the next picture.
    void RowsFrame(M68K &c) {
        c.a[4] = c.a[5] + static_cast<uint32_t>(kRowsTable);
        const uint32_t record = c.a[4] & 0xFFFFFF;
        c.a[3] = kTables[((Ram16(Var(c, kRowsRegister)) + 8) & 0x18) >> 3];
        SetRam32(record, c.a[3]);
        if (Ram8(record + kRowsStep) == 0) {
            SetRam8(record + kRowsStep, 2);
            SetRam8(record + kRowsWave, 0);
            SetRam8(record + kRowsWaveWait, 3);
            SetRam16(record + kRowsOwn, static_cast<uint16_t>(kOwnRow));
        }
        SetRam32(record + kRowsCameraX, Ram32(kCameraRecord + kLayerX));
        SetRam32(record + kRowsCameraY, Ram32(kCameraRecord + kLayerY));
        const Rows &rows = kRows[Ram16(kStage) / 2];
        if (rows.floor) {
            MakeRows(c, rows);
        } else {
            SetLow16(c.d[1], 0x0410); // (what the game's own way to the stage's code leaves there)
        }
        if (Ram8(Var(c, kBackdrop))) {
            WaveRows(c);
        }
        SetRam16(Var(c, kRowsRegister), static_cast<uint16_t>(Ram32(record) >> 8));
        c.d[0] = Ram32(record) >> 8;
        SetLow16(c.d[0], Ram8(Var(c, kBackdrop)) ? 0x100 : Ram16(record + kRowsCameraY));
        SetLow16(c.d[0], (c.d[0] - Ram16(Var(c, kStageDip))) & 0x3FF);
        SetRam16(Var(c, kFirstRowRegister), static_cast<uint16_t>(c.d[0]));
    }
} // namespace

void RegisterRows() {
    // (the time charged is near the game's own, which is long: it is added to as the table is made)
    AddRewrite(0x01FDFE, "RowsFrame", RowsFrame, kFreeFlags | kChecksVideo, 400);
}

} // namespace cps2recomp
