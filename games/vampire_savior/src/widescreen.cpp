// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy
//
// Vampire Savior: widescreen. A fight is shown 512 columns wide where the board shows 384 (16:9 for 4:3, the
// height and the size of everything as they were): more of the stage on each side. The fight is played in
// that picture too (the second half of this file): the players can go to its edges, the camera stops where
// its edges come to the stage's ends (camera.cpp), and what the game only keeps while it is in the picture
// is kept while it is in this one (picture.cpp). With the feature off none of that is there and the game is the board's.
//
// With "Invisible walls" (widescreen_walls) as well, the fight is shown in the wide picture but played as on the
// board: the second half of this file is left out (WidePlay is false, the margin 0), so the players are stopped
// where the board's picture ends, the camera goes as far as the board's, and what the game keeps or draws is what
// it does on the board. Only the picture is wider; near a stage's end the view is kept inside the stage, so the
// board's picture is then to one side of the wide one.
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

    // The camera's ends on the board are 256 and 640 on every stage but the last, Fetus of God, which is
    // wider: 160 and 736 (seen in the camera's record).
    constexpr int kWideStage = 11, kWideStageLeft = 160, kWideStageRight = 736;
    int CameraLeftEnd() {
        return Ram16(kStage) / 2 == kWideStage ? kWideStageLeft : kCameraLeft;
    }
    int CameraRightEnd() {
        return Ram16(kStage) / 2 == kWideStage ? kWideStageRight : kCameraRight;
    }
    bool OnStage(int camera) {
        return camera >= CameraLeftEnd() && camera <= CameraRightEnd();
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
    int MoveObject(uint32_t tile, uint16_t attributes, int) {
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

// --- the game played in the wide picture ---
//
// The players are kept inside the wide picture (edges.cpp), the camera stops where the wide picture's edges
// come to the stage's ends (camera.cpp), and what is drawn is what is in the wide picture (picture.cpp):
// each of those asks WideMargin.
namespace {
    bool g_play = false; // fights are played in the wide picture (WidePlay)
} // namespace

bool WidePlay() {
    return g_play;
}
int WideMargin() {
    return g_play ? kSide : 0;
}

// The camera. Its place is the board's picture's left edge, and the wide picture's is 64 left of that: so
// that the wide picture ends where the stage does, the camera stops 64 short of each of its ends.
void WideCameraEnds(uint32_t camera) {
    const uint16_t left = Ram16(camera + kCameraEndLeft), right = Ram16(camera + kCameraEndRight);
    // (only a stage's usual ends are moved, and moved back when the feature is switched off in a fight)
    const int boardLeft = CameraLeftEnd(), boardRight = CameraRightEnd();
    if ((left == boardLeft || left == boardLeft + kSide) && (right == boardRight || right == boardRight - kSide)) {
        SetRam16(camera + kCameraEndLeft, static_cast<uint16_t>(boardLeft + (g_play ? kSide : 0)));
        SetRam16(camera + kCameraEndRight, static_cast<uint16_t>(boardRight - (g_play ? kSide : 0)));
    }
}

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
    // (with invisible walls the picture is wide and the game is played as on the board)
    const bool shownWide = FeatureOn("widescreen");
    g_play = shownWide && !FeatureOn("widescreen_walls");
    // (the modes that put their picture together from more than one machine's keep the board's)
    if (shownWide && OnStage(camera) && OnStage(shown) && !CurtainDrawn(machine)) {
        g_side = kSide;
        // (the view's left edge is 64 left of the camera, and no further left than the camera's own end:
        // nearer the end than that, the stage is drawn to the side by what is missing. Played in the wide
        // picture the camera never goes there; with invisible walls it does, and the board's picture is
        // then to one side of the wide one)
        g_shift = shown - std::clamp<int>(shown, CameraLeftEnd() + kSide, CameraRightEnd() - kSide);
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

// A frame of another machine's (its memory is the one loaded now): as wide as the port's own this frame, and
// drawn by the same rules from what that machine is showing. (Its camera a frame ago is not kept, so near a
// stage's end its view could be a frame late; in the wide picture the camera does not go there.)
void WideForSpare(Machine &machine) {
    const int camera = static_cast<int16_t>(Ram16(kCameraX));
    g_shift = g_side && OnStage(camera) ? camera - std::clamp<int>(camera, CameraLeftEnd() + kSide, CameraRightEnd() - kSide) : 0;
    Video::Wide &wide = machine.video.wide;
    wide.side = g_side;
    wide.scroll[0] = wide.scroll[1] = wide.scroll[2] = g_shift;
    wide.object = g_side ? MoveObject : nullptr;
    wide.tile = g_side && Ram8(kBackLayer + kLayerDrawnAsItGoes) ? SideTile : nullptr;
}

void WideForPort(Machine &machine) {
    g_shift = machine.video.wide.scroll[1];
}

void RegisterWidescreen() {
    TheGame().widestPicture = [] { return Video::kWidth + (FeatureOn("widescreen") ? 2 * kSide : 0); };
    // (not in the Enhanced preset: it shows more of the stage than the board does, and a player who wants the
    // game as it is played in competition must not get that without asking)
    TheGame().features.push_back({"widescreen", "Display", "Widescreen",
                                  "Fights are shown and played in 16:9, with more of the stage on each side: the players can "
                                  "go to the wide picture's edges, so they can stand further apart than on the board. Off "
                                  "unless switched on here, in every preset: it changes how the game plays.",
                                  kFeatureFirstPage}); // (its switch is on the launcher's first page: game.cfg)
    TheGame().features.push_back({"widescreen_walls", "Display", "Invisible walls",
                                  "With widescreen: fights are still shown in 16:9, but played exactly as on the board. The "
                                  "players are stopped where the board's picture ends, by walls that are not seen, and the "
                                  "camera goes as far as the board's; near a stage's end the view stays on the stage, so the "
                                  "play area is then to one side of the picture. Nothing without widescreen.",
                                  0}); // (on the Display page, beside the picture's other options)
}

} // namespace cps2recomp
