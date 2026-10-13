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
// The same rule is kept for the planes behind, each by its own measure. A plane that moves less than the
// camera is drawn only as far as the board ever shows it: the 384 columns and as far as it travels. The view
// of it is kept inside that: it goes with the camera until 64 more columns on a side would show where
// nothing was drawn, and stops there (ViewShift), as the stage's own plane does at the stage's ends. A plane
// that travels less than 128 has not 512 columns the board shows; its view stays at its middle. A piece of
// the stage (an object) rides on one of the planes, and is drawn to the side with it: the game says which
// as it puts the piece in the object list (Rides).
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
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace cps2recomp {

using namespace vsav;

namespace {
    constexpr int kSide = Video::kMaxSide;

    constexpr int kCameraMiddle = (kCameraLeft + kCameraRight) / 2, kCameraTravel = kCameraRight - kCameraLeft;

    // How far each stage's planes behind travel while the camera goes from one end to the other (on the
    // board: 384): the scroll screen of 8-pixel tiles and the one of 32. (Seen in their records as the stage
    // viewer went across; the game works a plane's place out with shifts, a routine for each rate. The one
    // of 16 is the stage's own plane on every stage: its lines have rates of their own for the floor, which
    // are left alone.)
    constexpr int kTravel[kStages][2] = {{265, 265}, {318, 339}, {137, 137}, {77, 114}, {206, 309}, {359, 335},
                                         {306, 218}, {363, 298}, {400, 352}, {306, 262}, {384, 384}, {336, 288}};

    // What the picture being made was asked to be: the columns added on each side, and how far to the right
    // each scroll screen is drawn (g_shift: the stage's own plane, the one of 16-pixel tiles)
    int g_side = 0, g_shift = 0, g_shifts[3] = {0, 0, 0};
    bool g_play = false; // fights are played in the wide picture (WidePlay, and the second half of this file)

    // How far to the right a plane is drawn that travels `travel` across the camera's range, with the camera
    // at `camera`: nothing while 64 more columns of it on each side are columns the board shows too
    int ViewShift(int camera, int travel) {
        const double at = static_cast<double>(camera - kCameraMiddle) * travel / kCameraTravel; // from its middle
        const double most = std::max(travel / 2.0 - kSide, 0.0);
        return static_cast<int>(std::lround(at - std::clamp(at, -most, most)));
    }
    void SetShifts(int camera) {
        const unsigned stage = Ram16(kStage) / 2u;
        g_shift = g_shifts[1] = ViewShift(camera, kCameraTravel);
        g_shifts[0] = stage < kStages ? ViewShift(camera, kTravel[stage][0]) : g_shift;
        g_shifts[2] = stage < kStages ? ViewShift(camera, kTravel[stage][1]) : g_shift;
    }

    // Which plane each entry of the object list rides on. The game works out an object's place in the
    // picture in three routines, just before it puts the object's entries in the list at A2: 01BBC2 and
    // 01BC3A against the plane the object's record names (a byte, 4, 8 or 12: the scroll screen of 8-, 16-
    // or 32-pixel tiles), 01BC88 with no plane (the panel: its own place in the picture). A hook on each
    // notes it for the entries from A2 on. The list is made a frame before it is drawn, so the picture goes
    // by what was noted when its frame began (g_shown).
    constexpr uint32_t kPlacedOnPlane = 0x01BBC2, kPlacedOnPlane2 = 0x01BC3A, kPlacedInPicture = 0x01BC88;
    constexpr uint32_t kRidesOn = 0x0C; // an object's record: which plane
    enum : uint8_t { kRidesNotKnown, kRidesFirst, kRidesNone = 4 };
    WidePlanes g_planes, g_shown, g_portPlanes, g_portShown;
    int g_portShifts[3] = {0, 0, 0};

    Machine *g_machine = nullptr; // (the machine whose memory is loaded: for reading the list it is making)

    // A twin's entries are all in the list (the next object's begin at `end`): kept, for drawing the piece
    // when the record is the other one. An entry's place is across and down the picture; what stays the
    // same as the camera moves is that plus the plane's place across, and less its place up.
    void Filled(const uint8_t *list, int end) {
        if (g_planes.filling < 0) {
            return;
        }
        WidePlanes::Twin &twin = g_planes.twins[g_planes.filling];
        g_planes.filling = -1;
        twin.count = std::clamp(end - twin.at, 0, +WidePlanes::kTwinEntries);
        if (twin.is < 0 || twin.count == 0 || twin.plane < 1 || twin.plane > 3) {
            return;
        }
        WidePlanes::Twin::Piece &piece = twin.piece[twin.is];
        piece.count = 0;
        for (int n = 0; n < twin.count; n++) {
            const uint8_t *entry = list + (twin.at + n) * 8;
            const uint16_t x = static_cast<uint16_t>((entry[0] << 8) | entry[1]), y = static_cast<uint16_t>((entry[2] << 8) | entry[3]);
            const uint16_t attributes = static_cast<uint16_t>((entry[6] << 8) | entry[7]);
            if (y >= 0x8000 || attributes >= 0xFF00) {
                twin.count = n; // (the list's end)
                break;
            }
            uint16_t *kept = piece.entries[piece.count++];
            kept[0] = static_cast<uint16_t>((x & 0xFC00) | ((x + g_planes.planeX[twin.plane - 1]) & 0x3FF));
            kept[1] = static_cast<uint16_t>((y & 0xFC00) | ((y - g_planes.planeY[twin.plane - 1]) & 0x3FF));
            kept[2] = static_cast<uint16_t>((entry[4] << 8) | entry[5]);
            kept[3] = attributes;
        }
    }
    // (while the game makes the list it is the one the 68000 sees at 708000)
    const uint8_t *ListBeingMade() {
        static uint8_t copy[0x2000];
        for (uint32_t n = 0; n < 0x2000; n += 2) {
            const uint16_t word = g_machine->Read16(0x708000 + n);
            copy[n] = static_cast<uint8_t>(word >> 8);
            copy[n + 1] = static_cast<uint8_t>(word);
        }
        return copy;
    }

    void Rides(M68K &c, uint8_t rides) {
        const uint32_t at = c.a[2] & 0xFFFFFF;
        if (at < 0x708000 || at >= 0x70A000) {
            return; // (not the list)
        }
        const int entry = static_cast<int>((at & 0x1FFF) >> 3);
        if (g_planes.filling >= 0 && g_machine) {
            Filled(ListBeingMade(), entry);
        }
        if (entry <= g_planes.last) {
            // (an entry before the last object's: a new list is begun, and no twin is in it yet)
            for (WidePlanes::Twin &twin : g_planes.twins) {
                twin.is = -1;
            }
        }
        // (the entries since the last object's are that object's)
        for (int n = g_planes.last + 1; n < entry; n++) {
            g_planes.rides[n] = g_planes.rides[g_planes.last];
        }
        g_planes.rides[entry] = rides;
        g_planes.last = entry;
        if (rides >= kRidesFirst && rides < kRidesNone) {
            const uint32_t object = c.a[6] & 0xFFFFFF, plane = kBackLayer + (rides - kRidesFirst) * kLayerSize;
            g_planes.planeX[rides - kRidesFirst] = static_cast<int16_t>(Ram16(plane + kLayerShownX));
            g_planes.planeY[rides - kRidesFirst] = static_cast<int16_t>(Ram16(plane + kLayerShownY));
            for (int n = 0; n < WidePlanes::kTwins; n++) {
                WidePlanes::Twin &twin = g_planes.twins[n];
                if (twin.record == object) {
                    twin.is = Ram8(object + twin.stateAt) ? 1 : 0;
                    twin.at = entry;
                    twin.count = 0;
                    twin.plane = rides;
                    g_planes.filling = n;
                }
            }
        }
    }
    bool PlacedOnPlane(M68K &c) {
        if (!g_play) {
            return false;
        }
        const uint8_t plane = Ram8((c.a[6] & 0xFFFFFF) + kRidesOn);
        Rides(c, plane == 4 || plane == 8 || plane == 12 ? static_cast<uint8_t>(plane / 4) : kRidesNotKnown);
        return false;
    }
    bool PlacedInPicture(M68K &c) {
        if (g_play) {
            Rides(c, kRidesNone);
        }
        return false;
    }

    // --- a record that is one of two pieces ---
    //
    // Four of the stages' scripts keep two pieces of the stage in one record: it is the one on the left
    // while the camera is left of a place, and the one on the right while it is right of it (on the board
    // the two are never in the picture together). In the wide picture they are, so the one the record is
    // not is drawn from what the game last put in the list for it (ListTaken). The scripts, and the byte
    // of the record that says which piece it is:
    //   07D438 (+0A): Feast of the Damned's dancers, who move, and a piece on the right that does not.
    //                 There the record is kept the dancers (the two comparisons of the camera in the
    //                 script are answered "left of it") once the other piece has been seen.
    //   07DE78 (+0A), 0809A6 (+0B), 080A20 (+0A): two pieces that do not move.
    template <uint8_t kStateAt>
    bool TwinScript(M68K &c) {
        if (!g_play) {
            return false;
        }
        const uint32_t record = c.a[6] & 0xFFFFFF;
        WidePlanes::Twin *free = nullptr;
        for (WidePlanes::Twin &twin : g_planes.twins) {
            if (twin.record == record) {
                return false;
            }
            if (!twin.record && !free) {
                free = &twin;
            }
        }
        if (free) {
            *free = WidePlanes::Twin();
            free->record = record;
            free->stateAt = kStateAt;
        }
        return false;
    }
    // What the 68000 is left with after a comparison answered for it: lower (carry) or not; on after the
    // instruction, `length` bytes long at `at`
    bool Answered(M68K &c, uint32_t at, uint32_t length, bool lower) {
        c.flagC = c.flagN = lower;
        c.flagZ = c.flagV = false;
        c.pc = at + length;
        c.cycles += 8 + 2 * length;
        return true;
    }
    template <uint32_t kAt>
    bool KeepTheDancers(M68K &c) {
        if (!g_play) {
            return false;
        }
        const uint32_t record = c.a[6] & 0xFFFFFF;
        for (const WidePlanes::Twin &twin : g_planes.twins) {
            if (twin.record == record && twin.piece[1].count > 0) {
                return Answered(c, kAt, 6, true);
            }
        }
        return false;
    }
    // A piece of a stage that its script only draws, or only moves, while the camera is on one side of a
    // place (it is out of the board's picture on the other): in the wide picture, always. The comparison of
    // the camera with the place is answered the way that keeps the piece.
    template <uint32_t kAt, uint32_t kLength, bool kLower>
    bool KeepThePiece(M68K &c) {
        return g_play && Answered(c, kAt, kLength, kLower);
    }
    [[maybe_unused]] constexpr uint32_t kScriptsGoOnAt[] = {
        HookAt(0x07D444), HookAt(0x07D494), HookAt(0x07D4E0), HookAt(0x07DF52), HookAt(0x07E064), HookAt(0x07E0C0), HookAt(0x07F89C), HookAt(0x07FB28),
        HookAt(0x07FD50), HookAt(0x07FE2C), HookAt(0x0808AC), HookAt(0x0803B8), HookAt(0x080428), HookAt(0x07F272)};

    // When the board has taken the list: after each twin's entries, the other piece's, put where it is with
    // the planes where they were when the list was made. (From the last twin in the list back, so that the
    // places of those before it stay as noted; what each entry rides on moves along with the entries.)
    void ListTaken(uint16_t *list, int &last) {
        int order[WidePlanes::kTwins], twins = 0;
        for (int n = 0; n < WidePlanes::kTwins; n++) {
            const WidePlanes::Twin &twin = g_shown.twins[n];
            if (twin.record && twin.is >= 0 && twin.count > 0 && twin.piece[1 - twin.is].count > 0 && twin.plane >= 1 && twin.plane <= 3) {
                order[twins++] = n;
            }
        }
        std::sort(order, order + twins, [](int a, int b) { return g_shown.twins[a].at > g_shown.twins[b].at; });
        for (int k = 0; k < twins; k++) {
            const WidePlanes::Twin &twin = g_shown.twins[order[k]];
            const WidePlanes::Twin::Piece &other = twin.piece[1 - twin.is];
            const int at = twin.at + twin.count, entries = last / 4 + 1;
            if (at > entries || entries + other.count > 0x400) {
                continue;
            }
            memmove(list + (at + other.count) * 4, list + at * 4, static_cast<size_t>(entries - at) * 4 * sizeof(uint16_t));
            memmove(g_shown.rides + at + other.count, g_shown.rides + at, static_cast<size_t>(0x400 - at - other.count));
            for (int n = 0; n < other.count; n++) {
                uint16_t *entry = list + (at + n) * 4;
                entry[0] = static_cast<uint16_t>((other.entries[n][0] & 0xFC00) | ((other.entries[n][0] - g_shown.planeX[twin.plane - 1]) & 0x3FF));
                entry[1] = static_cast<uint16_t>((other.entries[n][1] & 0xFC00) | ((other.entries[n][1] + g_shown.planeY[twin.plane - 1]) & 0x3FF));
                entry[2] = other.entries[n][2];
                entry[3] = other.entries[n][3];
                g_shown.rides[at + n] = twin.plane;
            }
            last += other.count * 4;
        }
    }

    // What the picture a frame makes goes by: the list as it stands, the last object's entries to its end
    void Show(WidePlanes &planes, const Machine &machine) {
        if (planes.filling >= 0) {
            // (the twin was the last thing put in the list, which is the one the board takes next)
            WidePlanes mine = g_planes;
            g_planes = planes;
            Filled(machine.ObjectsToCome(), 0x400);
            planes = g_planes;
            g_planes = mine;
        }
        g_shown = planes;
        std::fill(g_shown.rides + std::min(planes.last + 1, 0x400), g_shown.rides + 0x400, planes.rides[planes.last]);
    }
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
    int MoveObject(uint32_t tile, uint16_t attributes, int entry) {
        const uint8_t rides = g_shown.rides[entry & 0x3FF];
        if (rides == kRidesNone || ThePanel(tile, attributes)) {
            return 0;
        }
        return g_shifts[rides == kRidesNotKnown ? 1 : rides - kRidesFirst];
    }

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
        const int looksAt = static_cast<int16_t>(Ram16(kBackLayer + kLayerX)) - g_side - g_shifts[0] + at;
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
    g_side = g_shift = g_shifts[0] = g_shifts[1] = g_shifts[2] = 0;
    g_play = FeatureOn("widescreen");
    if (g_play && OnStage(camera) && OnStage(shown) && !CurtainDrawn(machine)) {
        g_side = kSide;
        SetShifts(shown);
    }
    g_machine = &machine;
    if (!OnStage(camera)) {
        g_planes = WidePlanes(); // (no stage: its twins are gone with it)
    }
    Show(g_planes, machine);
    machine.video.objectsTaken = g_side ? ListTaken : nullptr;
    Video::Wide &wide = machine.video.wide;
    wide.side = g_side;
    std::copy(g_shifts, g_shifts + 3, wide.scroll);
    wide.object = g_side ? MoveObject : nullptr;
    // (a stage whose 8-pixel scroll screen is a whole picture in the map needs no help: it goes round)
    g_machine = &machine;
    wide.tile = g_side && Ram8(kBackLayer + kLayerDrawnAsItGoes) ? SideTile : nullptr;
    CheckPlan(machine);
}

// A frame of another machine's (its memory is the one loaded now): as wide as the port's own this frame, and
// drawn by the same rules from what that machine is showing. (Its camera a frame ago is not kept, so its
// planes can be a frame late against its camera: a pixel, while the camera moves.)
void WideForSpare(Machine &machine, WidePlanes &planes) {
    g_portPlanes = g_planes;
    g_portShown = g_shown;
    std::copy(g_shifts, g_shifts + 3, g_portShifts);
    const int camera = static_cast<int16_t>(Ram16(kCameraX));
    if (!OnStage(camera)) {
        planes = WidePlanes();
    }
    Show(planes, machine);
    g_planes = planes;
    g_shift = g_shifts[0] = g_shifts[1] = g_shifts[2] = 0;
    if (g_side && OnStage(camera)) {
        SetShifts(camera);
    }
    Video::Wide &wide = machine.video.wide;
    wide.side = g_side;
    std::copy(g_shifts, g_shifts + 3, wide.scroll);
    wide.object = g_side ? MoveObject : nullptr;
    wide.tile = g_side && Ram8(kBackLayer + kLayerDrawnAsItGoes) ? SideTile : nullptr;
}

void WideForPort(Machine &, WidePlanes &planes) {
    planes = g_planes;
    g_planes = g_portPlanes;
    g_shown = g_portShown;
    std::copy(g_portShifts, g_portShifts + 3, g_shifts);
    g_shift = g_shifts[1];
}

void WidePromoted(const WidePlanes &planes) {
    g_planes = planes;
}

void RegisterWidescreen() {
    // (028192 itself is hooked in battle.cpp, which calls WideKeepIn)
    AddHook(kKeepsInDone, "KeepsInDone", Widen);
    AddHook(kKeepsBothIn, "KeepsBothIn", KeepBothIn);
    AddHook(kKeepsBothInDone, "KeepsBothInDone", Widen);
    AddHook(kPlacedOnPlane, "PlacedOnPlane", PlacedOnPlane);
    AddHook(kPlacedOnPlane2, "PlacedOnPlane2", PlacedOnPlane);
    AddHook(kPlacedInPicture, "PlacedInPicture", PlacedInPicture);
    AddHook(0x07D438, "TwinScript", TwinScript<0x0A>);
    AddHook(0x07DE78, "TwinScript2", TwinScript<0x0A>);
    AddHook(0x0809A6, "TwinScript3", TwinScript<0x0B>);
    AddHook(0x080A20, "TwinScript4", TwinScript<0x0A>);
    AddHook(0x07D43E, "KeepTheDancers", KeepTheDancers<0x07D43E>);
    AddHook(0x07D48E, "KeepTheDancers2", KeepTheDancers<0x07D48E>);
    AddHook(0x07D4DA, "KeepThePiece0", KeepThePiece<0x07D4DA, 6, false>);
    AddHook(0x07DF4C, "KeepThePiece", KeepThePiece<0x07DF4C, 6, true>);
    AddHook(0x07E05E, "KeepThePiece2", KeepThePiece<0x07E05E, 6, false>);
    AddHook(0x07E0BA, "KeepThePiece3", KeepThePiece<0x07E0BA, 6, false>);
    AddHook(0x07F896, "KeepThePiece4", KeepThePiece<0x07F896, 6, false>);
    AddHook(0x07FB22, "KeepThePiece5", KeepThePiece<0x07FB22, 6, true>);
    AddHook(0x07FD4A, "KeepThePiece6", KeepThePiece<0x07FD4A, 6, false>);
    AddHook(0x07FE26, "KeepThePiece7", KeepThePiece<0x07FE26, 6, false>);
    AddHook(0x0808A6, "KeepThePiece8", KeepThePiece<0x0808A6, 6, false>);
    AddHook(0x0803B4, "KeepThePiece9", KeepThePiece<0x0803B4, 4, true>);
    AddHook(0x080424, "KeepThePiece10", KeepThePiece<0x080424, 4, true>);
    AddHook(0x07F26E, "KeepThePiece11", KeepThePiece<0x07F26E, 4, true>);
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
