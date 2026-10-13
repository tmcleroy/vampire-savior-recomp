// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy
//
// Vampire Savior: the boxes the game tests for hits and for pushing (boxes.h), read out of the objects'
// records and the ROM's tables the way the game's own tests read them (collision.cpp).
#include "boxes.h"

namespace vsav {

using namespace cps2recomp;

Box ReadBox(Machine &machine, uint32_t address) {
    return {static_cast<int16_t>(machine.Read16(address)), static_cast<int16_t>(machine.Read16(address + 2)),
            static_cast<int16_t>(machine.Read16(address + 4)), static_cast<int16_t>(machine.Read16(address + 6))};
}

namespace {
    // (a table's address has to be somewhere in the ROM's data)
    bool IsTable(uint32_t address) {
        return address >= 0x1000 && address < 0x400000 && !(address & 1);
    }

    void Place(Machine &machine, uint32_t object, BoxKind kind, uint32_t boxAddress, bool thrown, std::vector<PlacedBox> &boxes) {
        const Box box = ReadBox(machine, boxAddress);
        if (box.halfWidth <= 0 || box.halfHeight <= 0) {
            return;
        }
        const int x = static_cast<int16_t>(Ram16(object + kX)) + (Ram8(object + kFacesRight) ? -box.x : box.x);
        const int y = static_cast<int16_t>(Ram16(object + kY)) + box.y;
        boxes.push_back({kind, x, y, box.halfWidth, box.halfHeight, thrown});
    }
} // namespace

void ObjectBoxes(Machine &machine, uint32_t object, bool thrown, std::vector<PlacedBox> &boxes) {
    if (!Ram8(object + kActive)) {
        return;
    }
    static constexpr struct {
        BoxKind kind;
        uint32_t table, number;
    } kParts[] = {{BoxKind::Head, kHeadBoxes, kHeadBox},
                  {BoxKind::Body, kBodyBoxes, kBodyBox},
                  {BoxKind::Legs, kLegBoxes, kLegBox},
                  {BoxKind::Push, kPushBoxes, kPushBox}};
    for (const auto &part : kParts) {
        const uint32_t table = Ram32(object + part.table);
        if (const uint8_t number = Ram8(object + part.number); number && IsTable(table)) {
            Place(machine, object, part.kind, table + number * 8u, thrown, boxes);
        }
    }
    // The attack box is named by the animation frame being shown
    const uint32_t frame = Ram32(object + kFrame), table = Ram32(object + kAttackBoxes);
    if (IsTable(frame) && IsTable(table)) {
        if (const uint8_t number = static_cast<uint8_t>(machine.Read8(frame + kFrameAttack))) {
            Place(machine, object, BoxKind::Attack, table + number * kAttackBoxSize, thrown, boxes);
        }
    }
}

std::vector<PlacedBox> AllBoxes(Machine &machine) {
    std::vector<PlacedBox> boxes;
    for (int n = 0; n < kPlayerRecords; n++) {
        ObjectBoxes(machine, Player(n), false, boxes);
    }
    for (int n = 0; n < kThrownRecords; n++) {
        ObjectBoxes(machine, kThrown + static_cast<uint32_t>(n) * kThrownSize, true, boxes);
    }
    return boxes;
}

} // namespace vsav
