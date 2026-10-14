// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of cps2recomp.

#include "video.h"

#include "../runtime/state.h"

#include <cstring>

namespace cps2recomp {

namespace {
    // CPS-A registers (word index)
    enum { kScroll1Base = 1, kScroll2Base = 2, kScroll3Base = 3, kOtherBase = 4, kPaletteBase = 5 };
    enum { kScroll1X = 6, kRowScrollOffset = 16, kVideoControl = 17 };
    // CPS-B registers (word index), as the CPS-2's chip has them
    enum { kLayerControl = 0x26 / 2, kPaletteControl = 0x30 / 2 };
    // Object registers at 0x400000 (word index)
    enum { kObjPriority = 2, kObjXOffset = 4, kObjYOffset = 5 };

    inline uint16_t Word(const uint8_t *p) {
        return static_cast<uint16_t>((p[0] << 8) | p[1]);
    }

    // Eight pixels of a tile row from its four bytes: one bit plane per byte, leftmost pixel in bit 7
    inline void Unpack(const uint8_t *row, uint8_t *pixels) {
        const uint8_t b0 = row[0], b1 = row[1], b2 = row[2], b3 = row[3];
        for (int k = 0; k < 8; k++) {
            const int s = 7 - k;
            pixels[k] = static_cast<uint8_t>(((b0 >> s) & 1) | (((b1 >> s) & 1) << 1) | (((b2 >> s) & 1) << 2) |
                                             (((b3 >> s) & 1) << 3));
        }
    }
} // namespace

void Video::Reset() {
    memset(m_palette, 0, sizeof(m_palette));
    memset(m_objects, 0, sizeof(m_objects));
    m_priorityControl = 0;
    m_lastObject = -4;
}

// The palette in video RAM is up to six pages of 512 colours (objects, scroll 1-3, two for stars);
// the control register says which pages are there. A colour is 4 bits each of brightness, red, green, blue.
void Video::CopyPalette() {
    const uint32_t base = (static_cast<uint32_t>(m_src.cpsA[kPaletteBase]) * 256) & ~0x3FFu & 0x3FFFF;
    const uint16_t control = m_src.cpsB[kPaletteControl];
    uint32_t at = base;
    for (int page = 0; page < 6; page++) {
        if (control & (1u << page)) {
            for (int n = 0; n < 0x200; n++) {
                const uint16_t colour = Word(&m_src.videoRam[at & 0x3FFFF]);
                at += 2;
                const unsigned bright = 0x0F + ((colour >> 12) << 1);
                const unsigned r = ((colour >> 8) & 0x0F) * 0x11 * bright / 0x2D;
                const unsigned g = ((colour >> 4) & 0x0F) * 0x11 * bright / 0x2D;
                const unsigned b = (colour & 0x0F) * 0x11 * bright / 0x2D;
                m_palette[0x200 * page + n] = r | (g << 8) | (b << 16);
            }
        } else if (at != base) {
            at += 0x400; // (a page left out after the first still takes its room)
        }
    }
}

void Video::LatchObjects(const uint8_t *objectRam) {
    m_priorityControl = m_src.objectRegs[kObjPriority];
    for (int n = 0; n < 0x1000; n++) {
        m_objects[n] = Word(&objectRam[n * 2]);
    }
    // The list ends at the first entry marked so
    m_lastObject = 0x1000 - 4;
    for (int n = 0; n < 0x1000; n += 4) {
        if (m_objects[n + 1] >= 0x8000 || m_objects[n + 3] >= 0xFF00) {
            m_lastObject = n - 4;
            break;
        }
    }
    if (objectsTaken) {
        objectsTaken(m_objects, m_lastObject);
    }
}

// One scroll screen's pixels on a raster line. which: 0 (8-pixel tiles), 1 (16), 2 (32).
template <bool kWide>
void Video::DrawScroll(int which, int rasterLine, uint16_t *pens, uint8_t *priority, uint8_t mark) {
    const uint16_t *a = m_src.cpsA;
    const uint32_t base = (static_cast<uint32_t>(a[kScroll1Base + which]) * 256) & ~0x3FFFu & 0x3FFFF;
    const uint8_t *map = &m_src.videoRam[base];
    const int shift = 3 + which;              // tile size as a power of two
    const int size = 1 << shift;
    const uint32_t mapMask = (64u << shift) - 1; // the map is 64 x 64 tiles
    uint32_t scrollX = a[kScroll1X + which * 2];
    const uint32_t scrollY = a[kScroll1X + which * 2 + 1];
    if (which == 1 && (a[kVideoControl] & 1)) {
        // Scroll 2 can have its own horizontal position on every line
        const uint32_t other = (static_cast<uint32_t>(a[kOtherBase]) * 256) & ~0x7FFu & 0x3FFFF;
        scrollX += Word(&m_src.videoRam[other + (((rasterLine + a[kRowScrollOffset]) & 0x3FF) * 2)]);
    }
    const uint32_t y = (rasterLine + scrollY) & mapMask;
    const uint32_t row = y >> shift;
    const int width = kWide ? Width() : +kWidth;
    const bool rowScroll = which == 1 && (a[kVideoControl] & 1);
    const int ownLeft = kWide ? wide.side + wide.scroll[which] + (rowScroll && wide.rows ? wide.rows[(rasterLine + a[kRowScrollOffset]) & 0x3FF] : 0) : 0;
    const int ownRight = ownLeft + kWidth; // the board's own columns
    const uint32_t firstX = static_cast<uint32_t>(kFirstColumn - ownLeft + static_cast<int>(scrollX)) & mapMask;
    const uint16_t colourBase = static_cast<uint16_t>(0x20 * (which + 1));

    int out = -static_cast<int>(firstX & (size - 1)); // where the first (partly hidden) tile starts
    uint32_t column = firstX >> shift;
    for (; out < width; out += size, column = (column + 1) & 63) {
        uint32_t index;
        switch (which) {
        case 0: index = (row & 0x1F) + (column << 5) + ((row & 0x20) << 6); break;
        case 1: index = (row & 0x0F) + (column << 4) + ((row & 0x30) << 6); break;
        default: index = (row & 0x07) + (column << 3) + ((row & 0x38) << 6); break;
        }
        uint32_t code = Word(&map[index * 4]);
        uint16_t attr = Word(&map[index * 4 + 2]);
        if (kWide && wide.tile && (out < ownLeft || out + size > ownRight)) {
            // (beyond the board's own columns the game's sources may know better than the map: video.h)
            uint16_t itsCode = 0, itsAttr = 0;
            if (wide.tile(which, static_cast<int>(column), static_cast<int>(row), out, itsCode, itsAttr)) {
                code = itsCode;
                attr = itsAttr;
            }
        }
        // Where the tile is in the graphics ROMs: the scroll screens' tiles are in the second 8 MB
        uint32_t at;
        int rowBytes;
        switch (which) {
        case 0:
            at = 0x800000 + code * 64 + ((column & 1) ? 4 : 0); // (odd columns use the right half of a 16-pixel strip)
            rowBytes = 8;
            break;
        case 1:
            at = 0x800000 + code * 128;
            rowBytes = 8;
            break;
        default:
            code &= 0x3FFF;
            at = 0x800000 + code * 512;
            rowBytes = 16;
            break;
        }
        const int line = static_cast<int>(y & (size - 1));
        const bool flipX = attr & 0x20, flipY = attr & 0x40;
        at += static_cast<uint32_t>(flipY ? size - 1 - line : line) * rowBytes;
        if (at + rowBytes > m_src.graphicsSize) {
            continue;
        }
        const uint16_t colour = static_cast<uint16_t>(((attr & 0x1F) + colourBase) * 16);
        uint8_t pixels[32];
        for (int part = 0; part < size / 8; part++) {
            Unpack(&m_src.graphics[at + part * 4], &pixels[part * 8]);
        }
        for (int k = 0; k < size; k++) {
            const int x = out + k;
            if (x < 0 || x >= width) {
                continue;
            }
            const uint8_t pen = pixels[flipX ? size - 1 - k : k];
            if (pen != 15) {
                pens[x] = static_cast<uint16_t>(colour + pen);
                priority[x] |= mark;
            }
        }
    }
}

// The objects on a raster line. Each entry: x, y, tile, attributes; an entry can be a block of up to
// 16 x 16 tiles. An object's priority (0-7) against the scroll screens is in its x word; among objects,
// later entries of the list are in front. Drawn back to front here would need the scroll screens' marks
// kept apart, so (as MAME does) they are drawn front to back and a pixel once claimed stays claimed.
template <bool kWide>
void Video::DrawObjects(int rasterLine, uint16_t *pens, uint8_t *priority, const uint32_t *masks) {
    const int xOffset = 64 - m_src.objectRegs[kObjXOffset];
    const int yOffset = 16 - m_src.objectRegs[kObjYOffset];
    const int width = kWide ? Width() : +kWidth;
    for (int i = m_lastObject; i >= 0; i -= 4) {
        int x = m_objects[i];
        int y = m_objects[i + 1];
        const unsigned level = (x >> 13) & 7;
        const uint32_t code = m_objects[i + 2] + ((y & 0x6000u) << 3);
        const uint16_t attr = m_objects[i + 3];
        if (hideObject && hideObject(code, attr)) {
            continue;
        }
        if (attr & 0x80) {
            x += m_src.objectRegs[kObjXOffset];
            y += m_src.objectRegs[kObjYOffset];
        }
        const int across = ((attr >> 8) & 0x0F) + 1, down = ((attr >> 12) & 0x0F) + 1;
        const bool flipX = attr & 0x20, flipY = attr & 0x40;
        const uint16_t colour = static_cast<uint16_t>((attr & 0x1F) * 16);
        const uint32_t mask = masks[level] | 0x80000000u;
        // (where the first column drawn is in the object's own count across, which wraps at 1024)
        const int first = kWide ? kFirstColumn - wide.side - (wide.object ? wide.object(code, attr, i / 4) : 0) - 16 : 0;
        for (int ty = 0; ty < down; ty++) {
            const int sy = (y + ty * 16 + yOffset) & 0x3FF;
            if (rasterLine < sy || rasterLine >= sy + 16) {
                continue;
            }
            const int line = rasterLine - sy;
            for (int tx = 0; tx < across; tx++) {
                const int sx = kWide ? ((x + tx * 16 + xOffset - first) & 0x3FF) - 16 : ((x + tx * 16 + xOffset) & 0x3FF) - kFirstColumn;
                if (sx <= -16 || sx >= width) {
                    continue;
                }
                uint32_t tile;
                if (across == 1 && down == 1) {
                    tile = code;
                } else {
                    const int cx = flipX ? across - 1 - tx : tx;
                    const int cy = flipY ? down - 1 - ty : ty;
                    // (a plain block wraps within its row of sixteen tiles; flipped ones do not)
                    tile = (!flipX && !flipY) ? (code & ~0xFu) + ((code + cx) & 0xF) + 0x10 * cy : code + cx + 0x10 * cy;
                }
                const uint32_t at = (tile * 128 + static_cast<uint32_t>(flipY ? 15 - line : line) * 8);
                if (at + 8 > m_src.graphicsSize) {
                    continue;
                }
                uint8_t pixels[16];
                Unpack(&m_src.graphics[at], pixels);
                Unpack(&m_src.graphics[at + 4], pixels + 8);
                for (int k = 0; k < 16; k++) {
                    const int px = sx + k;
                    if (px < 0 || px >= width) {
                        continue;
                    }
                    const uint8_t pen = pixels[flipX ? 15 - k : k];
                    if (pen != 15) {
                        if (!((mask >> (priority[px] & 0x1F)) & 1)) {
                            pens[px] = static_cast<uint16_t>(colour + pen);
                        }
                        priority[px] = 31;
                    }
                }
            }
        }
    }
}

void Video::RenderLine(int line, uint32_t *out) {
    if (wide.side || wide.scroll[0] || wide.scroll[1] || wide.scroll[2] || wide.rows || wide.object || wide.tile) {
        RenderLineAs<true>(line, out);
    } else {
        RenderLineAs<false>(line, out);
    }
}

// Pixels that nothing was drawn at (the backdrop's pen), in runs no wider than the added side, take the line's
// own picture mirrored at the nearest drawn pixel (or that pixel where the mirror finds nothing). Wider runs are
// left: a stage that shows the backdrop on purpose shows more of it than that.
void Video::FillHoles(uint16_t *pens, int width) const {
    constexpr uint16_t kBackdrop = 0xBFF;
    uint16_t drawn[kMaxWidth]; // (the line as drawn: what is mirrored)
    memcpy(drawn, pens, static_cast<size_t>(width) * sizeof(uint16_t));
    for (int start = 0; start < width;) {
        if (drawn[start] != kBackdrop) {
            start++;
            continue;
        }
        int end = start;
        while (end < width && drawn[end] == kBackdrop) {
            end++;
        }
        const bool left = start > 0, right = end < width;
        if (end - start <= wide.side && (left || right)) {
            for (int x = start; x < end; x++) {
                // (mirrored at the nearer drawn pixel; at the line's ends, the one there is)
                const bool fromLeft = left && (!right || x - (start - 1) <= end - x);
                const int edge = fromLeft ? start - 1 : end;
                const int mirror = 2 * edge - x;
                pens[x] = mirror >= 0 && mirror < width && drawn[mirror] != kBackdrop ? drawn[mirror] : drawn[edge];
            }
        }
        start = end;
    }
}

template <bool kWide>
void Video::RenderLineAs(int line, uint32_t *out) {
    const int rasterLine = line + kFirstLine;
    const int width = kWide ? Width() : +kWidth;
    uint16_t pens[kMaxWidth];
    uint8_t priority[kMaxWidth];
    for (int x = 0; x < width; x++) {
        pens[x] = 0xBFF; // the backdrop colour
    }
    memset(priority, 0, sizeof(priority));

    // The layer control register orders four layers back to front (0: the CPS-1's objects, which the
    // CPS-2 does not have; 1-3: the scroll screens) and switches the scroll screens on
    const uint16_t control = m_src.cpsB[kLayerControl];
    const uint16_t videoControl = m_src.cpsA[kVideoControl];
    unsigned l0 = (control >> 6) & 3, l1 = (control >> 8) & 3, l2 = (control >> 10) & 3, l3 = (control >> 12) & 3;
    unsigned p0 = (m_priorityControl >> (4 * l0)) & 0xF, p1 = (m_priorityControl >> (4 * l1)) & 0xF;
    unsigned p2 = (m_priorityControl >> (4 * l2)) & 0xF, p3 = (m_priorityControl >> (4 * l3)) & 0xF;
    if (l0 == 0) {
        l0 = l1;
        l1 = 0;
        p0 = p1;
    }
    if (l1 == 0) {
        l1 = l2;
        l2 = 0;
        p1 = p2;
    }
    if (l2 == 0) {
        l2 = l3;
        l3 = 0;
        p2 = p3;
    }
    // Which objects each scroll screen hides: an object of level n is in front of the screens whose
    // priority is below n. masks[n] has a bit for every combination of screens (bit 0: the back one ...)
    // behind which such an object is not drawn.
    uint32_t masks[8] = {};
    {
        uint32_t mask0 = 0xAA, mask1 = 0xCC;
        if (p0 > p1) {
            mask0 &= ~0x88u;
        }
        if (p0 > p2) {
            mask0 &= ~0xA0u;
        }
        if (p1 > p2) {
            mask1 &= ~0xC0u;
        }
        masks[0] = 0xFF;
        for (unsigned n = 1; n < 8; n++) {
            if (n <= p0 && n <= p1 && n <= p2) {
                masks[n] = 0xFE;
                continue;
            }
            masks[n] = 0;
            if (n <= p0) {
                masks[n] |= mask0;
            }
            if (n <= p1) {
                masks[n] |= mask1;
            }
            if (n <= p2) {
                masks[n] |= 0xF0;
            }
        }
    }
    auto enabled = [&](unsigned layer) {
        if (!(layerMask & (1u << layer))) {
            return false;
        }
        switch (layer) {
        case 1: return (control & 0x02) != 0;
        case 2: return (control & 0x04) != 0 && (videoControl & 0x04) != 0;
        case 3: return (control & 0x08) != 0 && (videoControl & 0x08) != 0;
        default: return false;
        }
    };
    const unsigned order[3] = {l0, l1, l2};
    for (int n = 0; n < 3; n++) {
        if (order[n] && enabled(order[n])) {
            DrawScroll<kWide>(static_cast<int>(order[n]) - 1, rasterLine, pens, priority, static_cast<uint8_t>(1u << n));
        }
    }
    if (layerMask & 1) {
        DrawObjects<kWide>(rasterLine, pens, priority, masks);
    }
    if (kWide && wide.fillHoles && wide.side > 0) {
        FillHoles(pens, width);
    }
    for (int x = 0; x < width; x++) {
        out[x] = m_palette[pens[x]];
    }
    if (markObjects) {
        for (int x = 0; x < width; x++) {
            out[x] |= priority[x] == 31 ? 0x80000000u | (static_cast<uint32_t>(pens[x] >> 4) & 0x1F) << 24 : 0;
        }
    }
}

void Video::Serialize(Archive &ar) {
    ar.Mark(0x56494445);
    ar.Pod(m_palette);
    ar.Pod(m_objects);
    ar.Pod(m_priorityControl);
    ar.Pod(m_lastObject);
}

} // namespace cps2recomp
