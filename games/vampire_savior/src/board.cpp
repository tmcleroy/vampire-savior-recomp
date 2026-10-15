// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy
//
// Vampire Savior: the board's settings (the operator's, in its test menu: 設定, System and Game) as features on
// the launcher's Board page. Each is "As the board is set" unless chosen, and a choice is what the test menu
// would have set: the game reads its settings from the settings memory once, as the board starts (001980,
// called at 001168), into the variables below; the hook at 00116E, where that call returns, puts the choices in
// those variables. The board's settings memory itself is not changed, nor what the test menu shows and saves.
//
// The settings record (FF0C66, as the settings memory holds it) and the variable each goes to (from A5):
//   +01 continue      98  0 no, 1 yes             +10 difficulty     A0  0-7: levels 1 to 8 (2 on a new board)
//   +03 demo sound    99  0 off, 1 on             +11 attack power   A1  0-3: levels 1 to 4 (2)
//   +06 censorship    8C  0 off, 1 on (自主規制)   +12 timer speed    A2  0-3: levels 1 to 4 (2)
//                                                 +13 game speed     A3  0 normal, 1-3 turbo 1-3, 4-6 free
//                                                                         select 1-3 (free select 2)
//                                                 +14 damage gauges  A4  0-3: one to four (two)
//                                                 +15 event          A5  0 normal, 1 one match (no continue)
//                                                 +16 automatic      BB  0 none, 1 free select (free select)
// (the values and their names from the test menu's pages, stepped through; 001A80 on copies them)
#include "vsav.h"

#include "machine.h"

#include <iterator>

namespace cps2recomp {

namespace {
    constexpr uint32_t kVars = 0xFF8000; // (A5)
    constexpr const char *kPage = "Board";

    struct Setting {
        const char *name;
        uint32_t variable;
    };
    // (the value of a choice is its number less one: the first choice is "As the board is set")
    constexpr Setting kSettings[] = {
        {"board_difficulty", kVars + 0xA0}, {"board_attack", kVars + 0xA1},  {"board_timer", kVars + 0xA2},
        {"board_speed", kVars + 0xA3},      {"board_gauges", kVars + 0xA4},  {"board_event", kVars + 0xA5},
        {"board_automatic", kVars + 0xBB},  {"board_continue", kVars + 0x98}, {"board_demo_sound", kVars + 0x99},
        {"board_censorship", kVars + 0x8C},
    };
    constexpr uint32_t kContinue = kVars + 0x98, kEvent = kVars + 0xA5;

    // The board's own values of what was chosen, to put back for its test menu
    uint8_t g_board[std::size(kSettings)];
    uint8_t g_continue = 0;
    bool g_chosen = false; // the choices are in the variables
    bool g_again = false;  // they were taken out for the test menu, and go back in when a game begins

    // The choices put in the variables (the board's own kept first)
    void PutChoices() {
        g_chosen = false;
        for (size_t n = 0; n < std::size(kSettings); n++) {
            g_board[n] = Ram8(kSettings[n].variable);
        }
        g_continue = Ram8(kContinue); // (a one-match event clears it below)
        for (const Setting &setting : kSettings) {
            const int choice = FeatureValue(setting.name);
            if (choice > 0) {
                SetRam8(setting.variable, static_cast<uint8_t>(choice - 1));
                g_chosen = true;
            }
        }
        if (g_chosen && Ram8(kEvent)) {
            SetRam8(kContinue, 0); // (no continue in the one-match event, as the game has it: 001AA4)
        }
    }

    // 00116E: the board's settings have been read
    bool SettingsRead(M68K &) {
        g_again = false;
        PutChoices();
        return false; // (and on with the game's own code)
    }
    // 008F56, 00A4BA: a game begins (each copies difficulty to FF80BE, and continue)
    bool GameBegins(M68K &) {
        if (g_again) {
            g_again = false;
            PutChoices();
        }
        return false;
    }
} // namespace

// Before every frame. The board's test switch pressed (its test menu shows, changes and saves the variables)
// puts the board's own values back, so that what the test menu saves is the board's; leaving the test menu does
// not read the settings again, so the choices go back in when a game next begins (demo sound and censorship,
// which the show uses as well, at the next reset)
void BoardBeforeFrame(Machine &machine) {
    if (g_chosen && machine.testSwitch) {
        for (size_t n = 0; n < std::size(kSettings); n++) {
            SetRam8(kSettings[n].variable, g_board[n]);
        }
        SetRam8(kContinue, g_continue);
        g_chosen = false;
        g_again = true;
    }
}

void RegisterBoard() {
    Game &game = TheGame();
    game.pages.push_back({kPage, "", "The board's own settings, as its test menu has them (設定): each as the board is set unless "
                                     "chosen here. Taken when the game starts or is reset."});
    constexpr const char *kAsSet = "As the board is set";
    game.features.push_back({"board_difficulty", kPage, "Difficulty", "How well the computer fights, 1 (easiest) to 8 (hardest).", 0,
                             {kAsSet, "1 (easiest)", "2", "3", "4", "5", "6", "7", "8 (hardest)"}});
    game.features.push_back({"board_attack", kPage, "Attack power", "How much damage a hit does, 1 (least) to 4.", 0,
                             {kAsSet, "1 (least)", "2", "3", "4 (most)"}});
    game.features.push_back({"board_timer", kPage, "Timer speed", "How fast the round's clock runs, 1 (slowest) to 4.", 0,
                             {kAsSet, "1 (slowest)", "2", "3", "4 (fastest)"}});
    game.features.push_back({"board_speed", kPage, "Game speed",
                             "The game's speed: normal, one of three turbo speeds, or a free choice of speed for the players "
                             "(free select 1 to 3).",
                             0, {kAsSet, "Normal", "Turbo 1", "Turbo 2", "Turbo 3", "Free select 1", "Free select 2", "Free select 3"}});
    game.features.push_back({"board_gauges", kPage, "Damage gauges", "How many health bars each fighter has.", 0,
                             {kAsSet, "1", "2", "3", "4"}});
    game.features.push_back({"board_event", kPage, "Event", "Normal, or one match only (and no continue).", 0, {kAsSet, "Normal", "One match"}});
    game.features.push_back({"board_automatic", kPage, "Automatic", "Whether players may choose the automatic guard.", 0,
                             {kAsSet, "None", "Free select"}});
    game.features.push_back({"board_continue", kPage, "Continue", "Whether a game over can be continued.", 0, {kAsSet, "No", "Yes"}});
    game.features.push_back({"board_demo_sound", kPage, "Demo sound", "Whether the attract mode plays sound.", 0, {kAsSet, "Off", "On"}});
    game.features.push_back({"board_censorship", kPage, "Self-censorship", "The board's own restraint setting (自主規制).", 0,
                             {kAsSet, "Off", "On"}});
    AddHook(HookAt(0x00116E), "SettingsRead", SettingsRead);
    AddHook(HookAt(0x008F56), "GameBegins", GameBegins);
    AddHook(HookAt(0x00A4BA), "GameBeginsTwo", GameBegins);
}

} // namespace cps2recomp
