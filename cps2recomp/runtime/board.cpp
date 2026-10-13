// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of cps2recomp.

#include "board.h"

#include "../machine/eeprom.h"
#include "game.h"

namespace cps2recomp {

namespace {
    Eeprom *g_eeprom = nullptr;
    SettingsMemory g_settings;

    uint32_t Var(M68K &c, int32_t offset) {
        return (c.a[5] + static_cast<uint32_t>(offset)) & 0xFFFFFF;
    }
    // Where the game's copy of the words to read or write is, the first word's place in the memory, and how many
    struct Span {
        uint32_t at;
        unsigned first, words;
    };
    Span Asked(M68K &c) {
        const uint16_t start = Ram16(Var(c, g_settings.start));
        return {Ram32(Var(c, g_settings.buffer)) + static_cast<uint32_t>(static_cast<int16_t>(start)), (start >> 1) & 0x3Fu,
                Ram16(Var(c, g_settings.count)) + 1u};
    }

    void ReadWords(M68K &c) {
        const Span span = Asked(c);
        for (unsigned n = 0; n < span.words; n++) {
            SetRam16(span.at + 2 * n, g_eeprom->words[(span.first + n) & 0x3F]);
        }
        // (the registers as the game's loops leave them: some games go on to use a counter that has run out)
        c.d[0] = g_eeprom->words[(span.first + span.words - 1) & 0x3F];
        c.d[3] = 0x0000FFFF;
        c.d[4] = (c.d[4] & 0xFFFF0000) | ((0x180 | span.first) + span.words);
        c.d[5] = (c.d[5] & 0xFFFF0000) | 0xFFFF;
        c.a[0] = span.at + 2 * span.words;
        Wait(c, g_settings.readTime + span.words * g_settings.readWordTime, Var(c, g_settings.busy));
    }

    void WriteWords(M68K &c) {
        const Span span = Asked(c);
        for (unsigned n = 0; n < span.words; n++) {
            g_eeprom->words[(span.first + n) & 0x3F] = Ram16(span.at + 2 * n);
        }
        g_eeprom->changed = true;
        c.d[0] = 0;
        c.d[3] = 0x0000FFFF;
        c.d[4] = (c.d[4] & 0xFFFF0000) | ((0x140 | ((span.first + span.words - 1) & 0x3F)) + 1);
        c.d[5] = (c.d[5] & 0xFFFF0000) | 0xFFFF;
        c.a[0] = span.at + 2 * span.words;
        Wait(c, g_settings.writeTime + span.words * g_settings.writeWordTime, Var(c, g_settings.busy));
    }
} // namespace

void SetEeprom(Eeprom *eeprom) {
    g_eeprom = eeprom;
}

void AddSettingsMemory(const SettingsMemory &memory) {
    g_settings = memory;
    constexpr uint32_t kScratch = kFreeD1 | kFreeD2 | kFreeFlags; // (what the lines' own routines use)
    AddRewrite(memory.readWords, "ReadSettingsWords", ReadWords, kScratch, 1);
    AddRewrite(memory.writeWords, "WriteSettingsWords", WriteWords, kScratch, 1);
}

} // namespace cps2recomp
