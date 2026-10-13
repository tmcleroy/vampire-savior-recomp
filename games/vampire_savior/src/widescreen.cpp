// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy
//
// Vampire Savior: widescreen. A fight is shown 512 columns wide where the board shows 384 (16:9 for 4:3, the
// height and the size of everything as they were): more of the stage on each side. The fight is played in
// that picture too (the second half of this file): the players can go to its edges, the camera stops where
// its edges come to the stage's ends, and what the game only keeps while it is in the picture is kept
// while it is in this one. With the feature off none of that is there and the game is the board's.
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

// --- the game played in the wide picture ---
//
// The players. The game keeps a player inside the board's picture: no further left than the camera's place
// less half the player's width, no further right than 384 on from it (028192, for one player; 02824C, for
// the two when one carries the other). Both read the half width from the player's record, and both come
// to the picture's edges through it and nothing else, so with 64 less in that word for as long as the
// routine runs, the edges it keeps the player inside are the wide picture's. It is put back where the
// routine returns (each has one place where it does).
namespace {
    constexpr uint32_t kKeepsIn = 0x028192, kKeepsInDone = 0x02824A;
    constexpr uint32_t kKeepsBothIn = 0x02824C, kKeepsBothInDone = 0x0282B6;
    constexpr uint32_t kHalfWidth = 0x132; // a player's record: half its width (a word)
    constexpr uint32_t kOther = 0x32;      // ... and the other player's record (a word: its address's low half)

    bool g_play = false;              // fights are played in the wide picture (WidePlay)
    uint32_t g_narrowed[2] = {0, 0};  // the records whose half width is 64 less just now

    void Narrow(int n, uint32_t player) {
        g_narrowed[n] = player;
        SetRam16(player + kHalfWidth, static_cast<uint16_t>(Ram16(player + kHalfWidth) - kSide));
    }
    bool Widen(M68K &) {
        for (uint32_t &player : g_narrowed) {
            if (player) {
                SetRam16(player + kHalfWidth, static_cast<uint16_t>(Ram16(player + kHalfWidth) + kSide));
                player = 0;
            }
        }
        return false;
    }
    bool KeepBothIn(M68K &c) {
        if (g_play && !g_narrowed[0] && !g_narrowed[1]) {
            const uint32_t player = c.a[6] & 0xFFFFFF;
            Narrow(0, player);
            Narrow(1, 0xFF0000u | Ram16(player + kOther));
        }
        return false;
    }
} // namespace

// The things on a stage. The game asks of each whether it is in the picture, in a few routines all its
// objects share, and what is not is not drawn, or is done away with. Each asks by one comparison, which is
// made here instead when fights are played in the wide picture. A thing thrown is asked the same question
// of a picture 64 wider on each side. A piece of the stage is not asked at all: it is always drawn. (The
// pieces ride on the planes behind, which move less than the camera, and some are wide: asked of a wider
// picture, some still came in late. The owner's idea, and nothing is lost by it: what is outside the
// picture is not seen, and a stage has few pieces.)
namespace {
    // (where the 68000 goes on after each of those comparisons: named so that there is translated code from
    // there, and the interpreter is not needed)
    [[maybe_unused]] constexpr uint32_t kGoesOnAt[] = {HookAt(0x0154B0), HookAt(0x015516), HookAt(0x015532)};
    // What `cmp.w` leaves in the flags, of `value` less `than`; the 68000 goes on after the instruction
    // (four bytes long, at `at`: a hook is not told where the 68000 is)
    bool Compared(M68K &c, uint32_t at, uint32_t value, uint32_t than) {
        const uint32_t a = value & 0xFFFF, b = than & 0xFFFF, difference = (a - b) & 0xFFFF;
        c.flagC = a < b;
        c.flagZ = difference == 0;
        c.flagN = difference & 0x8000;
        c.flagV = ((a ^ b) & (a ^ difference) & 0x8000) != 0;
        c.pc = at + 4;
        c.cycles += 8;
        return true;
    }
    // 0154AC: `cmpi.w #$240,d1`, where d1 is how far right of the picture's left edge a thing thrown is,
    // and a margin of 96: outside that it is done away with
    bool ThrownInPicture(M68K &c) {
        return g_play && Compared(c, 0x0154AC, c.d[1] + kSide, 0x240 + 2 * kSide);
    }
    // 015512, 01552E: `cmpi.w #width,d1` in the two routines that say whether a piece of the stage is drawn
    // (by how far across the picture it is; how far up is asked after). Always: as if it were at the edge.
    template <uint32_t kAt>
    bool PieceInPicture(M68K &c) {
        return g_play && Compared(c, kAt, 0, 1);
    }
    // 0154E2, 0154F4: `cmp.w camera,d1` for a piece that is only drawn while the camera is left of a place,
    // or right of one. Always: on to where the routine goes when it is (015696).
    bool PieceWhile(M68K &c) {
        if (!g_play) {
            return false;
        }
        c.pc = 0x015696;
        c.cycles += 18;
        return true;
    }
} // namespace

bool WidePlay() {
    return g_play;
}
int WideMargin() {
    return g_play ? kSide : 0;
}

void WideKeepIn(M68K &c) {
    if (g_play && !g_narrowed[0] && !g_narrowed[1]) {
        Narrow(0, c.a[6] & 0xFFFFFF);
    }
}

// The camera. Its place is the board's picture's left edge, and the wide picture's is 64 left of that: so
// that the wide picture ends where the stage does, the camera stops 64 short of each of its ends.
void WideCameraEnds(uint32_t camera) {
    const uint16_t left = Ram16(camera + kCameraEndLeft), right = Ram16(camera + kCameraEndRight);
    // (only a stage's usual ends are moved, and moved back when the feature is switched off in a fight)
    if ((left == kCameraLeft || left == kCameraLeft + kSide) && (right == kCameraRight || right == kCameraRight - kSide)) {
        SetRam16(camera + kCameraEndLeft, static_cast<uint16_t>(kCameraLeft + (g_play ? kSide : 0)));
        SetRam16(camera + kCameraEndRight, static_cast<uint16_t>(kCameraRight - (g_play ? kSide : 0)));
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
    g_play = FeatureOn("widescreen");
    // (the modes that put their picture together from more than one machine's keep the board's)
    if (g_play && OnStage(camera) && OnStage(shown) && !CurtainDrawn(machine)) {
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

// A frame of another machine's (its memory is the one loaded now): as wide as the port's own this frame, and
// drawn by the same rules from what that machine is showing. (Its camera a frame ago is not kept, so near a
// stage's end its view could be a frame late; in the wide picture the camera does not go there.)
void WideForSpare(Machine &machine) {
    const int camera = static_cast<int16_t>(Ram16(kCameraX));
    g_shift = g_side && OnStage(camera) ? camera - std::clamp<int>(camera, kCameraLeft + kSide, kCameraRight - kSide) : 0;
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
    // (028192 itself is hooked in battle.cpp, which calls WideKeepIn)
    AddHook(kKeepsInDone, "KeepsInDone", Widen);
    AddHook(kKeepsBothIn, "KeepsBothIn", KeepBothIn);
    AddHook(kKeepsBothInDone, "KeepsBothInDone", Widen);
    AddHook(0x0154AC, "ThrownInPicture", ThrownInPicture);
    AddHook(0x015512, "PieceInPicture", PieceInPicture<0x015512>);
    AddHook(0x01552E, "PieceInPicture2", PieceInPicture<0x01552E>);
    AddHook(0x0154E2, "PieceWhileLeftOf", PieceWhile);
    AddHook(0x0154F4, "PieceWhileRightOf", PieceWhile);
    TheGame().widestPicture = [] { return Video::kWidth + (FeatureOn("widescreen") ? 2 * kSide : 0); };
    // (not in the Enhanced preset: it shows more of the stage than the board does, and a player who wants the
    // game as it is played in competition must not get that without asking)
    TheGame().features.push_back({"widescreen", "Display", "Widescreen",
                                  "Fights are shown and played in 16:9, with more of the stage on each side: the players can "
                                  "go to the wide picture's edges, so they can stand further apart than on the board. Off "
                                  "unless switched on here, in every preset: it changes how the game plays.",
                                  kFeatureFirstPage}); // (its switch is on the launcher's first page: game.cfg)
}

} // namespace cps2recomp
