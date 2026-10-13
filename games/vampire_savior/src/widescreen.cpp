// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy
//
// Vampire Savior: widescreen. A fight is shown 512 columns wide where the board shows 384 (16:9 for 4:3, the
// height and the size of everything as they were): more of the stage on each side. Only the picture changes.
// The game runs as it does without it (a run's memory is the same frame for frame, which the check script
// tests), so the players can stand no further apart than on the board, and the panel stays where it was.
//
// A stage is three scroll screens and some objects, each moving at its own rate as the camera does, about the
// stage's middle. The board's picture is the 384 columns from the camera's place; the stage's own plane (the
// one the players stand in) is drawn for the camera's whole range and no further: 768 columns. So the wide
// picture is the board's with 64 more columns on each side while the camera is 64 or more from both of its
// ends, and nearer an end the whole view stays where it was 64 from it: everything on the stage is drawn that
// much to the side (video.h, Video::wide), the panel is not.
//
// The planes behind go with it: they and the stage's own pieces (objects that ride on them) keep the places
// they have against each other on the board. They move less than the camera, so the view shows up to 64
// columns of them that the board never does at any place of the camera; on every stage but one the game has
// those drawn (PROGRESS.md says what is seen where it has not).
//
// The scroll screens of 16- and 32-pixel tiles are whole pictures in the board's memory. The one of 8-pixel
// tiles is only 512 columns there, and on most stages its picture is wider: the game draws it into the
// map a 32-pixel strip at a time as the camera moves, from the stage's plan in the ROM, keeping up what the
// board shows. The wide picture's sides are read from that plan here instead (StagePlanTile).
#include "widescreen.h"

#include "game.h"
#include "machine.h"
#include "modes.h"
#include "vsav.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>

namespace cps2recomp {

using namespace vsav;

namespace {
    constexpr int kSide = Video::kMaxSide;

    // What the picture being made was asked to be: the columns added on each side, and how far to the right
    // the stage is drawn
    int g_side = 0, g_shift = 0;
    // The camera's place a frame ago. The picture a frame makes is the one the game worked out the frame
    // before from the camera as it then was: its scroll registers are written and its objects taken at the
    // start of the frame. (Not in a save state: after loading one the view can be a frame behind for a frame.)
    int g_cameraBefore = 0;

    bool OnStage(int camera) {
        return camera >= kCameraLeft && camera <= kCameraRight;
    }

    // Between a match's screens the game draws a curtain of red blocks across the board's picture, and only
    // that wide: while any of it is in the object list the picture is the board's. (Its objects: colour set
    // 3, tiles 2C600-2FDFF, which nothing else on a stage uses.)
    bool CurtainDrawn(const Machine &machine) {
        const uint8_t *list = machine.ObjectsToCome();
        for (int n = 0; n < 0x400; n++, list += 8) {
            const unsigned y = (list[2] << 8) | list[3], attributes = (list[6] << 8) | list[7];
            if (y >= 0x8000 || attributes >= 0xFF00) {
                break; // (the list's end)
            }
            const unsigned tile = ((list[4] << 8) | list[5]) + ((y & 0x6000) << 3);
            if ((attributes & 0x1F) == 3 && tile >= 0x2C600 && tile < 0x2FE00) {
                return true;
            }
        }
        return false;
    }

    // An object's place: the panel's objects stay, the stage's and the fighters' go with the view
    int MoveObject(uint32_t tile, uint16_t attributes) {
        return ThePanel(tile, attributes) ? 0 : g_shift;
    }

    Machine *g_machine = nullptr; // (for the picture's questions, which come while a frame runs)

    // The tile of the 8-pixel scroll screen's picture at a place in the stage, as the game's own routine
    // that draws a strip of it finds it (01EE22, 01EE58, 01EF16, 01F06E): the place's 256-pixel block from
    // the stage's plan, the block's 32-pixel cell, the cell's tile. `y` counts down the map as the board's
    // rows do (the game's own count goes up the stage, and it turns it over before this).
    void StagePlanTile(Machine &machine, uint32_t x, uint32_t y, uint16_t &code, uint16_t &attributes) {
        const uint8_t block = Ram8(kBackPlan + ((y & 0x100) >> 6) + ((x & 0x300) >> 8));
        const uint16_t cell = machine.Read16(kBackBlocks + block * 128u + ((x & 0xE0) >> 1) + ((y & 0xE0) >> 4));
        const uint32_t tile = kBackCells + cell * 64u + (((x >> 3) & 3) * 4 + ((y >> 3) & 3)) * 4;
        code = machine.Read16(tile);
        attributes = machine.Read16(tile + 2);
    }

    // What the picture asks (Video::wide.tile) for a tile outside the board's own columns. The map's column
    // says where in the stage only within 512 pixels: the one meant is the one nearest to where that
    // column of the picture looks.
    bool SideTile(int which, int column, int row, int at, uint16_t &code, uint16_t &attributes) {
        if (which != 0 || !g_machine) {
            return false;
        }
        const int looksAt = static_cast<int16_t>(Ram16(kBackLayer + kLayerX)) - g_side - g_shift + at;
        const int x = column * 8 + ((looksAt - column * 8 + 256) & ~511);
        StagePlanTile(*g_machine, static_cast<uint32_t>(x), static_cast<uint32_t>(row) * 8, code, attributes);
        return true;
    }

    // For checking StagePlanTile (VSAV_WIDE_CHECK=1): the map as the game has drawn it, where the board
    // shows it, against the plan. Printed when a stage is left: how many tiles were compared and how many
    // differed (and every thousand frames of a stage that is not left).
    void CheckPlan(Machine &machine) {
        static const bool check = getenv("VSAV_WIDE_CHECK") != nullptr;
        static long compared = 0, differed = 0;
        static int stage = -1;
        if (!check) {
            return;
        }
        const int camera = static_cast<int16_t>(Ram16(kCameraX));
        if (!OnStage(camera) || Ram8(kBackLayer + kLayerDrawnAsItGoes) == 0) {
            if (compared) {
                fprintf(stderr, "widescreen: stage %d, %ld tiles of the plan against the map, %ld differ\n", stage, compared, differed);
            }
            compared = differed = 0;
            return;
        }
        stage = Ram16(kStage) / 2;
        if (machine.frame % 1000 == 0 && compared) {
            fprintf(stderr, "widescreen: stage %d, %ld tiles of the plan against the map, %ld differ\n", stage, compared, differed);
        }
        // (the board's columns but for a strip's width at each side, which the game may be about to draw)
        const int left = static_cast<int16_t>(Ram16(kBackLayer + kLayerX)) + 32;
        const int top = (machine.cpsA[7] + Video::kFirstLine) & 0x1FF;
        for (int x = left & ~7; x < left + Video::kWidth - 64; x += 8) {
            for (int y = top & ~7; y < top + Video::kHeight; y += 8) {
                const int column = (x >> 3) & 63, row = (y >> 3) & 63;
                const uint8_t *map = &machine.videoRam[(static_cast<uint32_t>(machine.cpsA[1]) * 256 & 0x3C000) +
                                                       static_cast<uint32_t>((row & 0x1F) + (column << 5) + ((row & 0x20) << 6)) * 4];
                uint16_t code = 0, attributes = 0;
                StagePlanTile(machine, static_cast<uint32_t>(x), static_cast<uint32_t>(y), code, attributes);
                compared++;
                differed += code != ((map[0] << 8) | map[1]) || attributes != ((map[2] << 8) | map[3]);
            }
        }
    }
} // namespace

int WideSide() {
    return g_side;
}
int WideStage() {
    return g_side + g_shift;
}

void WidescreenBeforeFrame(Machine &machine) {
    const int camera = static_cast<int16_t>(Ram16(kCameraX)), shown = g_cameraBefore;
    g_cameraBefore = camera;
    g_side = g_shift = 0;
    // (the modes that put their picture together from more than one machine's keep the board's)
    if (FeatureOn("widescreen") && OnStage(camera) && OnStage(shown) && !BattleMode() && !PictureIsTwoStages() &&
        !CurtainDrawn(machine)) {
        g_side = kSide;
        // (the view's left edge is 64 left of the camera, and no further left than the camera's own end:
        // nearer the end than that, the stage is drawn to the side by what is missing)
        g_shift = shown - std::clamp<int>(shown, kCameraLeft + kSide, kCameraRight - kSide);
    }
    Video::Wide &wide = machine.video.wide;
    wide.side = g_side;
    wide.scroll[0] = wide.scroll[1] = wide.scroll[2] = g_shift;
    wide.object = g_side ? MoveObject : nullptr;
    // (a stage whose 8-pixel scroll screen is a whole picture in the map needs no help: it goes round)
    g_machine = &machine;
    wide.tile = g_side && Ram8(kBackLayer + kLayerDrawnAsItGoes) ? SideTile : nullptr;
    CheckPlan(machine);
}

void RegisterWidescreen() {
    TheGame().widestPicture = [] { return Video::kWidth + (FeatureOn("widescreen") ? 2 * kSide : 0); };
    // (not in the Enhanced preset: it shows more of the stage than the board does, and a player who wants the
    // game as it is played in competition must not get that without asking)
    TheGame().features.push_back({"widescreen", "Display", "Widescreen",
                                  "Fights are shown in 16:9 with more of the stage on each side. Off unless switched on here, "
                                  "in every preset. The game itself runs exactly as before: the players can stand as far "
                                  "apart as on the board, and the bars stay where they were."});
}

} // namespace cps2recomp
