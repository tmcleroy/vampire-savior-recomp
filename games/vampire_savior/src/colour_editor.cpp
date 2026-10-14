// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy
//
// Vampire Savior: the colour editor, a mode of the menu's "Alternative modes" page. It is the model viewer
// (model.cpp) with a panel at the right of the picture and player 1's controls given to the panel: a row for
// the character, the background and the size (the viewer's own choices), for the set of colours (by the
// buttons that choose it) and its line (the character's own colours, or its effects'), for the colour, and
// for its red, green and blue, 16 levels each as the board has them.
//
// A change is written where the game reads the character's colours (palettes.cpp, which keeps them in the
// ROM image), and into the palettes the board is showing, so that it is seen at once; the colour chosen
// blinks on the character for a moment. The character's colours that are not the game's are written to its
// file in the palettes folder a little after the last change, and when the editor ends.
#include "machine.h"
#include "palettes.h"
#include "vsav.h"

#include "../recomp-common/text.h"

#include <algorithm>
#include <cstdio>
#include <iterator>
#include <string>

namespace cps2recomp {

void StartModelViewer(Machine &machine); // model.cpp
bool ModelViewer();
int ModelCharacter();
int ModelCharacterNumber(int choice);

namespace {
    using namespace vsav;
    using recomp::text::Canvas;
    using recomp::text::Color;

    constexpr const char *kPage = "Colour editor";

    enum Row { kRowCharacter, kRowBackground, kRowSize, kRowSet, kRowLine, kRowColour, kRowRed, kRowGreen, kRowBlue, kRows };
    constexpr const char *kRowNames[kRows] = {"Character", "Background", "Size", "Set", "Line", "Colour", "Red", "Green", "Blue"};
    constexpr const char *kSetNames[kColourSetsEach] = {"LP", "MP", "HP", "LK", "MK", "HK", "PP", "KK", "AUTO1", "AUTO2"};
    constexpr const char *kLineNames[kPalettesInSet] = {"Body", "Effect 1", "Effect 2", "Effect 3"};
    // The viewer's "Background" and "Size" choices, as short as the panel needs them (model.cpp has them in full)
    constexpr const char *kBackgrounds[] = {"Stage 1", "Stage 2", "Stage 3", "Stage 4", "Stage 5", "Stage 6", "Stage 7",
                                            "Stage 8", "Stage 9", "Stage 10", "Stage 11", "Stage 12", "White", "Black"};
    constexpr const char *kSizes[] = {"1x", "1.5x", "2x", "3x", "4x", "6x", "8x", "12x", "16x", "25x", "0.25x", "0.5x", "0.75x"};
    constexpr int kSeen = kColoursInPalette - 1; // (the 16th colour of a palette is never seen)

    constexpr int kHeldFirst = 20, kHeldEvery = 8, kHeldEveryLevel = 4; // frames a direction is held before it repeats
    constexpr int kBlinkFor = 48, kBlinkEvery = 6; // frames the colour chosen blinks for, and how fast
    constexpr int kSaveAfter = 45;                 // frames after the last change that the file is written
    constexpr int kSaidFor = 120;                  // frames a message is shown for
    constexpr int kPanelWidth = 140, kShift = 72;  // the panel's width, and how far left the character goes

    struct State {
        bool on = false;
        int row = kRowColour;
        int set = 0, line = 0, colour = 1;
        int character = -1;   // the number of the character being edited (-1: not yet known)
        uint16_t before = 0;  // player 1's controls the frame before
        uint16_t held = 0;    // the direction held, and for how many frames
        int heldFor = 0;
        int blink = 0;        // frames the colour chosen still blinks for
        int unsaved = -1;     // frames since the last change not yet written (-1: none)
        int copied = -1;      // a colour copied with LP (-1: none)
        std::string said;     // a message, and frames it is still shown for
        int saidFor = 0;
    } g;

    void Say(std::string text) {
        g.said = std::move(text);
        g.saidFor = kSaidFor;
    }
    void Save() {
        if (g.unsaved < 0 || g.character < 0) {
            return;
        }
        g.unsaved = -1;
        Say(SaveColours(g.character) ? std::string("Saved ") + ColourName(g.character) : std::string("Not saved: no data folder"));
    }
    void End() {
        if (g.on) {
            Save();
            g = State();
        }
    }

    uint16_t Level(uint16_t colour, int part) { // part 0 red, 1 green, 2 blue
        return static_cast<uint16_t>((colour >> (8 - 4 * part)) & 0x0F);
    }
    uint16_t WithLevel(uint16_t colour, int part, int level) {
        const int shift = 8 - 4 * part;
        return static_cast<uint16_t>((colour & ~(0x0F << shift)) | (std::clamp(level, 0, 15) << shift));
    }
    // A colour of the board's as the picture has it (full brightness): red in the low byte
    Color Shown(uint16_t colour) {
        return {static_cast<uint8_t>(Level(colour, 0) * 0x11), static_cast<uint8_t>(Level(colour, 1) * 0x11),
                static_cast<uint8_t>(Level(colour, 2) * 0x11), 255};
    }

    uint16_t Current() {
        return Colour(g.character, g.set, g.line, g.colour);
    }
    void Change(uint16_t colour) {
        if (colour == Current()) {
            return;
        }
        SetColour(g.character, g.set, g.line, g.colour, colour);
        g.unsaved = 0;
    }

    // A choice of the viewer's stepped by one, round from the last to the first
    void StepChoice(const char *name, int count, int by) {
        SetFeatureValue(name, (std::clamp(FeatureValue(name), 0, count - 1) + by + count) % count);
    }
    int Characters() {
        int count = 0;
        while (ModelCharacterNumber(count) >= 0) {
            count++;
        }
        return count;
    }

    // Left or right on a row
    void Step(int by) {
        switch (g.row) {
        case kRowCharacter:
            Save(); // (the viewer starts again with the next one: model.cpp)
            StepChoice("model_character", Characters(), by);
            break;
        case kRowBackground: StepChoice("model_background", static_cast<int>(std::size(kBackgrounds)), by); break;
        case kRowSize: StepChoice("model_size", static_cast<int>(std::size(kSizes)), by); break;
        case kRowSet: g.set = (g.set + by + kColourSetsEach) % kColourSetsEach; break;
        case kRowLine:
            g.line = (g.line + by + kPalettesInSet) % kPalettesInSet;
            g.blink = kBlinkFor;
            break;
        case kRowColour:
            g.colour = (g.colour + by + kSeen) % kSeen;
            g.blink = kBlinkFor;
            break;
        default: {
            const int part = g.row - kRowRed;
            Change(WithLevel(Current(), part, Level(Current(), part) + by));
            break;
        }
        }
    }

    // The player's set made the one being edited, and its palettes put where the board shows them from (the
    // colour chosen, while it blinks, turned over)
    void ShowColours(Machine &machine) {
        SetRam8(Player(0) + kColours, static_cast<uint8_t>(g.set));
        SetRam32(Player(0) + kColourSet, ColourSetAddress(g.character, g.set));
        const uint32_t buffer = Ram32(kPaletteBuffer);
        const uint32_t first = buffer + static_cast<uint32_t>(Ram8(Player(0) + kPaletteSlot)) * kColoursInPalette * 2;
        const bool firstOnly = (kFirstLineOnly >> (g.character & 31)) & 1;
        for (int line = 0; line < (firstOnly ? 1 : kPalettesInSet); line++) {
            for (int n = 0; n < kColoursInPalette; n++) {
                uint16_t colour = Colour(g.character, g.set, line, n);
                if (line == g.line && n == g.colour && g.blink > 0 && (g.blink / kBlinkEvery) % 2 == 1) {
                    colour ^= 0x0FFF;
                }
                const uint32_t at = first + static_cast<uint32_t>(line * kColoursInPalette + n) * 2;
                machine.Write16(at, static_cast<uint16_t>((machine.Read16(at) & 0xF000) | colour)); // (the brightness kept)
            }
        }
    }

    void Draw(Machine &machine) {
        const int width = machine.PictureWidth(), height = Video::kHeight;
        const Canvas canvas{machine.Picture(), width, height};
        const Color white{255, 255, 255, 255}, dim{150, 150, 170, 255}, black{0, 0, 0, 255};
        const int x = width - kPanelWidth - 4, right = width - 10;
        int y = 6;
        canvas.Fill(static_cast<float>(x), 4, kPanelWidth, static_cast<float>(height - 8), {18, 20, 30, 235});
        auto text = [&](int at, int row, const char *s, Color c, int align) { // align: -1 left, 1 right
            const float w = recomp::text::TextWidth(s, 1);
            canvas.Text(align < 0 ? static_cast<float>(at) : static_cast<float>(at) - w, static_cast<float>(row), 1, 0, c, s);
        };
        text(x + 6, y + 2, "COLOUR EDITOR", white, -1);
        y += 13;
        const uint16_t colour = Current();
        for (int row = 0; row < kRows; row++) {
            const bool chosen = row == g.row;
            if (chosen) {
                canvas.Fill(static_cast<float>(x + 2), static_cast<float>(y - 1), kPanelWidth - 4, 10, {64, 70, 110, 255});
            }
            text(x + 6, y, kRowNames[row], chosen ? white : dim, -1);
            char value[32] = "";
            switch (row) {
            case kRowCharacter: snprintf(value, sizeof(value), "%s", ColourName(g.character)); break;
            case kRowBackground: snprintf(value, sizeof(value), "%s", kBackgrounds[std::clamp(FeatureValue("model_background"), 0, 13)]); break;
            case kRowSize: snprintf(value, sizeof(value), "%s", kSizes[std::clamp(FeatureValue("model_size"), 0, 12)]); break;
            case kRowSet: snprintf(value, sizeof(value), "%s", kSetNames[g.set]); break;
            case kRowLine: snprintf(value, sizeof(value), "%s", kLineNames[g.line]); break;
            case kRowColour: snprintf(value, sizeof(value), "%d", g.colour + 1); break;
            default: {
                // a bar of 16 levels, and the level
                const int part = row - kRowRed, level = Level(colour, part);
                const Color fill = part == 0 ? Color{230, 60, 60, 255} : part == 1 ? Color{60, 200, 80, 255} : Color{70, 110, 240, 255};
                const int barX = x + 52;
                canvas.Fill(static_cast<float>(barX), static_cast<float>(y + 1), 64, 6, {50, 52, 64, 255});
                canvas.Fill(static_cast<float>(barX), static_cast<float>(y + 1), static_cast<float>(4 * (level + 1)), 6, fill);
                snprintf(value, sizeof(value), "%d", level);
                break;
            }
            }
            text(right, y, value, chosen ? white : dim, 1);
            y += 10;
        }
        // The line's colours, the one chosen marked
        y += 3;
        for (int n = 0; n < kSeen; n++) {
            const int sx = x + 6 + n * 8;
            if (n == g.colour) {
                canvas.Fill(static_cast<float>(sx - 1), static_cast<float>(y - 1), 9, 10, white);
                canvas.Fill(static_cast<float>(sx), static_cast<float>(y), 7, 8, black);
            }
            canvas.Fill(static_cast<float>(sx + (n == g.colour)), static_cast<float>(y + (n == g.colour)), static_cast<float>(7 - 2 * (n == g.colour)),
                        static_cast<float>(8 - 2 * (n == g.colour)), Shown(Colour(g.character, g.set, g.line, n)));
        }
        y += 12;
        // The colour chosen, large, with its value and the game's
        canvas.Fill(static_cast<float>(x + 6), static_cast<float>(y), 26, 20, Shown(colour));
        char hex[16];
        snprintf(hex, sizeof(hex), "#%02X%02X%02X", Level(colour, 0) * 0x11, Level(colour, 1) * 0x11, Level(colour, 2) * 0x11);
        text(x + 38, y + 1, hex, white, -1);
        // (and below it the game's, and the one copied with LP)
        const uint16_t game = GameColour(g.character, g.set, g.line, g.colour);
        text(x + 38, y + 11, "game", dim, -1);
        canvas.Fill(static_cast<float>(x + 64), static_cast<float>(y + 11), 10, 8, Shown(game));
        if (g.copied >= 0) {
            text(x + 80, y + 11, "copy", dim, -1);
            canvas.Fill(static_cast<float>(x + 106), static_cast<float>(y + 11), 10, 8, Shown(static_cast<uint16_t>(g.copied)));
        }
        y += 25;
        // What the controls do
        const char *const help[] = {"Up/Down: a row", "Left/Right: change", "LP copy  MP paste", "HP the game's"};
        for (const char *line : help) {
            text(x + 6, y, line, dim, -1);
            y += 10;
        }
        if (g.saidFor > 0) {
            text(x + 6, y + 4, g.said.c_str(), white, -1);
        } else if (g.unsaved >= 0) {
            text(x + 6, y + 4, "Saving...", dim, -1);
        }
    }

    void Start(Machine &machine) {
        End();
        // (the colours made are the files': they are used in play only with the feature on)
        if (!FeatureOn("custom_colours")) {
            SetFeature("custom_colours", true);
        }
        g.on = true;
        StartModelViewer(machine);
    }
    void ResetSet(Machine &) {
        if (!g.on || g.character < 0) {
            return;
        }
        for (int line = 0; line < kPalettesInSet; line++) {
            for (int n = 0; n < kColoursInPalette; n++) {
                SetColour(g.character, g.set, line, n, GameColour(g.character, g.set, line, n));
            }
        }
        g.unsaved = 0;
    }
    void ResetCharacter(Machine &) {
        if (!g.on || g.character < 0) {
            return;
        }
        for (int set = 0; set < kColourSetsEach; set++) {
            for (int line = 0; line < kPalettesInSet; line++) {
                for (int n = 0; n < kColoursInPalette; n++) {
                    SetColour(g.character, set, line, n, GameColour(g.character, set, line, n));
                }
            }
        }
        g.unsaved = 0;
    }
} // namespace

int ColourEditorShift() {
    return g.on ? kShift : 0;
}
void EndColourEditor() {
    End();
}

// Before every frame: the editor ends with the viewer (the menu's Reset, another mode, a state loaded), and
// keeps Custom colours on while it runs (switched off, the files' colours would be put back under it)
void ColourEditorCheck() {
    if (g.on && !ModelViewer()) {
        End();
    } else if (g.on && !FeatureOn("custom_colours")) {
        SetFeature("custom_colours", true);
    }
}

// With the viewer's round on: player 1's controls are the panel's (and the character stands still)
void ColourEditorBeforeFrame(Machine &machine) {
    if (!g.on) {
        return;
    }
    if (ModelCharacter() != g.character) {
        Save();
        g.character = ModelCharacter();
    }
    const uint16_t now = machine.players[0];
    machine.players[0] = 0;
    const uint16_t pressed = static_cast<uint16_t>(now & ~g.before);
    g.before = now;
    // A direction acts when pressed, and again and again while it is held (faster for the levels)
    const uint16_t direction = now & (kUp | kDown | kLeft | kRight);
    g.heldFor = direction && direction == g.held ? g.heldFor + 1 : 0;
    g.held = direction;
    const int every = g.row >= kRowRed ? kHeldEveryLevel : kHeldEvery;
    const bool acts = g.heldFor == 0 || (g.heldFor >= kHeldFirst && (g.heldFor - kHeldFirst) % every == 0);
    if (direction && acts) {
        if (direction & kUp) {
            g.row = (g.row + kRows - 1) % kRows;
        } else if (direction & kDown) {
            g.row = (g.row + 1) % kRows;
        } else {
            Step(direction & kRight ? 1 : -1);
        }
    }
    if (pressed & kPunch1) {
        g.copied = Current();
        Say("Copied");
    } else if ((pressed & kPunch2) && g.copied >= 0) {
        Change(static_cast<uint16_t>(g.copied));
    } else if (pressed & kPunch3) {
        Change(GameColour(g.character, g.set, g.line, g.colour));
    }
    if (g.unsaved >= 0 && ++g.unsaved >= kSaveAfter) {
        Save();
    }
    g.blink = std::max(g.blink - 1, 0);
    g.saidFor = std::max(g.saidFor - 1, 0);
    ShowColours(machine);
}

// After the frame, with the viewer's picture made: the panel over its right
void ColourEditorFrame(Machine &machine) {
    if (g.on && g.character >= 0) {
        Draw(machine);
    }
}

void RegisterColourEditor() {
    Game &game = TheGame();
    game.pages.push_back({kPage, "Alternative modes",
                          "The characters' colours changed while you watch: any set, the character's own colours or its "
                          "effects', 16 levels of red, green and blue."});
    game.commands.push_back({"colours_edit", kPage, "Start",
                             "Shows the model viewer's character with a panel at the right, worked with player 1's controls: "
                             "up and down choose a row, left and right change it. LP copies the colour, MP pastes it, HP puts "
                             "back the game's. What is changed is saved in the palettes folder (switching on Custom colours, "
                             "which uses it in play). Reset ends it.",
                             Start});
    game.commands.push_back({"colours_reset_set", kPage, "Game's colours for this set",
                             "While editing: every colour of the set being edited back to the game's.", ResetSet});
    game.commands.push_back({"colours_reset_character", kPage, "Game's colours for this character",
                             "While editing: every set of the character being edited back to the game's colours.", ResetCharacter});
}

} // namespace cps2recomp
