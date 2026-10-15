// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of cps2recomp.
//
// The colour editor (colours.h), a mode of the menu's "Alternative modes" page. It is the game's model viewer
// with a panel at the right of the picture and player 1's controls given to the panel: a row for the character,
// the background and the size (the viewer's own choices), for the set of colours (by the buttons that choose it)
// and its line (the character's own colours, or its effects'), for the colour, and for its red, green and blue,
// 16 levels each as the board has them.
//
// A change is written where the game reads the character's colours (colours.cpp, which keeps them in the ROM
// image), and into the palettes the board is showing, so that it is seen at once; the colour chosen blinks on
// the character for a moment. The character's colours that are not the game's are written to its file in the
// palettes folder a little after the last change, and when the editor ends.
#include "colours.h"
#include "machine.h"

#include "../recomp-common/text.h"

#include <algorithm>
#include <cstdio>
#include <iterator>
#include <random>
#include <string>
#include <vector>

namespace cps2recomp::colours {

namespace {
    using recomp::text::Canvas;
    using recomp::text::Color;

    constexpr const char *kPage = "Colour editor";

    enum Row { kRowCharacter, kRowBackground, kRowSize, kRowAnimation, kRowSet, kRowLine, kRowColour, kRowRed, kRowGreen, kRowBlue,
               kRowRandom, kRowSave, kRowLoad, kRows };
    constexpr const char *kRowNames[kRows] = {"Character", "Background", "Size", "Animation", "Set", "Line", "Colour",
                                              "Red", "Green", "Blue", "Random colours", "Save set", "Load"};
    // The viewer's choices the editor has rows for
    constexpr const char *kCharacterChoice = "model_character", *kBackgroundChoice = "model_background", *kSizeChoice = "model_size";
    // The editor's own "Animation" choice (the viewer's is left as it was): standing, the game's idle, unless chosen
    constexpr const char *kAnimation = "colours_animation";
    const std::vector<const char *> &AnimationNames() { // (the viewer's, with standing named shortly)
        static std::vector<const char *> names;
        if (names.empty()) {
            names = TheDescription().animationNames();
            names[0] = "Standing";
        }
        return names;
    }
    constexpr int kSeen = kColoursInPalette - 1; // (the 16th colour of a palette is never seen)

    constexpr int kHeldFirst = 20, kHeldEvery = 8, kHeldEveryLevel = 4; // frames a direction is held before it repeats
    constexpr int kBlinkFor = 48, kBlinkEvery = 6; // frames the colour chosen blinks for, and how fast
    constexpr int kSaveAfter = 45;                 // frames after the last change that the file is written
    constexpr int kSaidFor = 120;                  // frames a message is shown for
    constexpr int kPanelWidth = 140, kShift = 72;  // the panel's width, and how far left the character goes

    int Sets() {
        return static_cast<int>(TheDescription().setNames.size());
    }
    int Lines() {
        return TheDescription().palettesInSet;
    }
    std::string LineName(int line) {
        const auto &names = TheDescription().lineNames;
        return line < static_cast<int>(names.size()) ? names[static_cast<size_t>(line)] : line ? "Effect " + std::to_string(line) : "Body";
    }
    // A choice of the menu's: how many it has and the name of one
    const Feature *FindFeature(const char *name) {
        for (const Feature &feature : TheGame().features) {
            if (std::string(feature.name) == name) {
                return &feature;
            }
        }
        return nullptr;
    }
    int ChoiceCount(const char *name) {
        const Feature *feature = FindFeature(name);
        return feature ? std::max(static_cast<int>(feature->choices.size()), 1) : 1;
    }
    const char *ChoiceName(const char *name, const std::vector<const char *> &shortNames) {
        const int value = std::clamp(FeatureValue(name), 0, ChoiceCount(name) - 1);
        if (value < static_cast<int>(shortNames.size())) {
            return shortNames[static_cast<size_t>(value)];
        }
        const Feature *feature = FindFeature(name);
        return feature && value < static_cast<int>(feature->choices.size()) ? feature->choices[static_cast<size_t>(value)] : "";
    }

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
        // The character's saved sets (colours.h), and the one being tried on the Load row (0: none; n: the
        // n-th), with the set's colours as they were before it, to put back if it is not loaded
        std::vector<std::string> saved;
        int loading = 0;
        bool trying = false;
        uint16_t kept[kMostPalettes][kColoursInPalette] = {};
        int deleteFor = 0;    // frames MK can still be pressed again to delete the saved set
        int randomFor = 0;    // frames the Random row can still be pressed again (LK, left or right) for random colours
        // The set as it was before the last random colours, for HK to put back (until another change)
        bool canUndo = false;
        int undoSet = 0;
        uint16_t undo[kMostPalettes][kColoursInPalette] = {};
        int lineWanted = -1;  // a line chosen whose animation is still to be asked for (Description::animationForLine)
        std::string said;     // a message, and frames it is still shown for
        int saidFor = 0;
    } g;

    constexpr size_t kValueMost = 15, kLineMost = 21; // characters: beside a row's name, and a whole line
    // Text cut to fit the panel (".." at its end when cut)
    std::string Fit(std::string text, size_t most) {
        return text.size() <= most ? text : text.substr(0, most - 2) + "..";
    }
    void Say(std::string text) {
        g.said = Fit(std::move(text), kLineMost);
        g.saidFor = kSaidFor;
    }
    // A saved set's name as the panel shows it: without the character's own name at its start ("Ryu Classic" is
    // "Classic" on Ryu's panel), to fit
    std::string ShortName(const std::string &saved, size_t most) {
        const std::string prefix = std::string(Name(g.character)) + " ";
        const bool own = saved.size() > prefix.size() && saved.compare(0, prefix.size(), prefix) == 0;
        return Fit(own ? saved.substr(prefix.size()) : saved, most);
    }
    void Save() {
        if (g.unsaved < 0 || g.character < 0) {
            return;
        }
        g.unsaved = -1;
        Say(SaveColours(g.character) ? std::string("Saved ") + Name(g.character) : std::string("Not saved: no folder"));
    }
    void PutBack();
    void End() {
        if (g.on) {
            PutBack();
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
        g.canUndo = false;
    }

    // A choice of the viewer's stepped by one, round from the last to the first
    void StepChoice(const char *name, int count, int by) {
        SetFeatureValue(name, (std::clamp(FeatureValue(name), 0, count - 1) + by + count) % count);
    }
    int Characters() {
        int count = 0;
        while (TheDescription().characterNumber(count) >= 0) {
            count++;
        }
        return count;
    }

    void Randomise(Machine &machine);
    // Random colours by the panel (the Random row: LK, left or right): only at a second press within a second and a half,
    // so that one press by mistake does not lose the set
    void AskRandom(Machine &machine) {
        if (g.randomFor > 0) {
            g.randomFor = 0;
            Randomise(machine);
        } else {
            g.randomFor = 90;
            Say("Again: random colours");
        }
    }
    // HK: the set back as it was before the random colours
    void Undo() {
        if (!g.canUndo) {
            return;
        }
        PutBack();
        for (int line = 0; line < Lines(); line++) {
            for (int n = 0; n < kColoursInPalette; n++) {
                SetColour(g.character, g.undoSet, line, n, g.undo[line][n]);
            }
        }
        g.canUndo = false;
        g.unsaved = 0;
        Say("Random colours undone");
    }

    // Trying a saved set: the set's colours kept to put back, then the saved ones over them (a "-" in the
    // file keeps the set's own); stopping puts them back unless the saved one was loaded
    void PutBack() {
        if (g.trying) {
            for (int line = 0; line < Lines(); line++) {
                for (int n = 0; n < kColoursInPalette; n++) {
                    SetColour(g.character, g.set, line, n, g.kept[line][n]);
                }
            }
            g.trying = false;
        }
        g.loading = 0;
    }
    void Try(int choice) {
        PutBack();
        g.loading = choice;
        if (!choice) {
            return;
        }
        for (int line = 0; line < Lines(); line++) {
            for (int n = 0; n < kColoursInPalette; n++) {
                g.kept[line][n] = Colour(g.character, g.set, line, n);
            }
        }
        uint16_t colours[kMostPalettes][kColoursInPalette];
        std::copy(&g.kept[0][0], &g.kept[0][0] + kMostPalettes * kColoursInPalette, &colours[0][0]);
        if (!ReadSavedSet(g.character, g.saved[static_cast<size_t>(choice - 1)], colours)) {
            Say("Cannot read it");
            return;
        }
        g.trying = true;
        for (int line = 0; line < Lines(); line++) {
            for (int n = 0; n < kSeen; n++) {
                SetColour(g.character, g.set, line, n, colours[line][n]);
            }
        }
    }
    void Load() {
        if (!g.trying) {
            return;
        }
        g.trying = false; // (kept)
        g.unsaved = 0;
        Say("Loaded " + ShortName(g.saved[static_cast<size_t>(g.loading - 1)], kLineMost - 7));
    }
    void Delete() {
        if (!g.loading) {
            return;
        }
        if (g.deleteFor <= 0) {
            g.deleteFor = 90;
            Say("MK again: delete");
            return;
        }
        const std::string name = g.saved[static_cast<size_t>(g.loading - 1)];
        PutBack();
        g.deleteFor = 0;
        Say(RemoveSavedSet(g.character, name) ? "Deleted " + ShortName(name, kLineMost - 8) : std::string("Not deleted"));
        g.saved = SavedSets(g.character);
    }

    // Left or right on a row
    void Step(Machine &machine, int by) {
        switch (g.row) {
        case kRowCharacter:
            Save(); // (the viewer starts again with the next one)
            StepChoice(kCharacterChoice, Characters(), by);
            break;
        case kRowBackground: StepChoice(kBackgroundChoice, ChoiceCount(kBackgroundChoice), by); break;
        case kRowSize: StepChoice(kSizeChoice, ChoiceCount(kSizeChoice), by); break;
        case kRowAnimation: SetFeatureValue(kAnimation, TheDescription().animationStep(machine, FeatureValue(kAnimation), by)); break;
        case kRowSet: g.set = (g.set + by + Sets()) % Sets(); break;
        case kRowLine:
            g.line = (g.line + by + Lines()) % Lines();
            g.blink = kBlinkFor;
            g.lineWanted = g.line; // (and the animation that shows it, when the game can say)
            break;
        case kRowColour:
            g.colour = (g.colour + by + kSeen) % kSeen;
            g.blink = kBlinkFor;
            break;
        case kRowRandom: AskRandom(machine); break;
        case kRowSave: {
            std::string name;
            Say(SaveSet(g.character, g.set, name) ? "Saved as " + ShortName(name, kLineMost - 9) : std::string("Not saved: no folder"));
            break;
        }
        case kRowLoad:
            if (g.saved.empty()) {
                Say("None saved yet");
            } else {
                Try((g.loading + by + static_cast<int>(g.saved.size()) + 1) % (static_cast<int>(g.saved.size()) + 1));
            }
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
        int lines = 0;
        const uint32_t first = TheDescription().showSet(machine, g.character, g.set, lines);
        if (!first) {
            return;
        }
        for (int line = 0; line < std::min(lines, Lines()); line++) {
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
            case kRowCharacter: snprintf(value, sizeof(value), "%s", Name(g.character)); break;
            case kRowBackground: snprintf(value, sizeof(value), "%s", ChoiceName(kBackgroundChoice, TheDescription().backgrounds)); break;
            case kRowSize: snprintf(value, sizeof(value), "%s", ChoiceName(kSizeChoice, {})); break;
            case kRowAnimation: {
                const int choice = FeatureValue(kAnimation);
                const auto &names = AnimationNames();
                snprintf(value, sizeof(value), "%s", choice <= 0 || choice >= static_cast<int>(names.size()) ? "Standing" : names[static_cast<size_t>(choice)]);
                break;
            }
            case kRowSet: snprintf(value, sizeof(value), "%s", TheDescription().setNames[static_cast<size_t>(g.set)]); break;
            case kRowLine: snprintf(value, sizeof(value), "%s", LineName(g.line).c_str()); break;
            case kRowColour: snprintf(value, sizeof(value), "%d", g.colour + 1); break;
            case kRowRandom:
            case kRowSave: snprintf(value, sizeof(value), "%s", chosen ? "< >" : ""); break;
            case kRowLoad:
                snprintf(value, sizeof(value), "%s",
                         g.loading ? ShortName(g.saved[static_cast<size_t>(g.loading - 1)], kValueMost).c_str()
                                   : !chosen ? "" : g.saved.empty() ? "none saved" : "< >");
                break;
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
        canvas.Fill(static_cast<float>(x + 6), static_cast<float>(y), 26, 18, Shown(colour));
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
        y += 22;
        // What the controls do (on the Load row, its own), the last line giving way to a message
        const char *const help[] = {"Arrows: choose/change", "LP copy  MP paste",
                                    g.canUndo ? "HK undo random" : g.row == kRowRandom ? "LK twice: random" : "HP game's colour"};
        const char *const loadHelp[] = {"Left/Right: try one", "LP load it  MK delete", "Up/Down: put it back"};
        const char *const *lines = g.row == kRowLoad ? loadHelp : help;
        for (int n = 0; n < 2; n++) {
            text(x + 6, y + 10 * n, lines[n], dim, -1);
        }
        if (g.saidFor > 0) {
            text(x + 6, y + 20, g.said.c_str(), white, -1);
        } else if (g.unsaved >= 0) {
            text(x + 6, y + 20, "Saving...", dim, -1);
        } else {
            text(x + 6, y + 20, lines[2], dim, -1);
        }
    }

    void Start(Machine &machine) {
        End();
        // (the colours made are the files': they are used in play only with the feature on)
        if (!FeatureOn("custom_colours")) {
            SetFeature("custom_colours", true);
        }
        g.on = true;
        TheDescription().startViewer(machine);
    }
    void ResetSet(Machine &) {
        if (!g.on || g.character < 0) {
            return;
        }
        PutBack();
        for (int line = 0; line < Lines(); line++) {
            for (int n = 0; n < kColoursInPalette; n++) {
                SetColour(g.character, g.set, line, n, GameColour(g.character, g.set, line, n));
            }
        }
        g.unsaved = 0;
    }
    // Every colour of the set that is seen (each line's first 15) at random: any of the board's 16 levels of
    // each of red, green and blue
    void Randomise(Machine &) {
        if (!g.on || g.character < 0) {
            return;
        }
        PutBack();
        static std::mt19937 random(std::random_device{}());
        std::uniform_int_distribution<int> level(0, 0x0FFF);
        for (int line = 0; line < Lines(); line++) {
            for (int n = 0; n < kColoursInPalette; n++) {
                g.undo[line][n] = Colour(g.character, g.set, line, n);
            }
            for (int n = 0; n < kSeen; n++) {
                SetColour(g.character, g.set, line, n, static_cast<uint16_t>(level(random)));
            }
        }
        g.unsaved = 0;
        g.canUndo = true;
        g.undoSet = g.set;
        Say("Random: HK undoes it");
    }
    void ResetCharacter(Machine &) {
        if (!g.on || g.character < 0) {
            return;
        }
        PutBack();
        for (int set = 0; set < Sets(); set++) {
            for (int line = 0; line < Lines(); line++) {
                for (int n = 0; n < kColoursInPalette; n++) {
                    SetColour(g.character, set, line, n, GameColour(g.character, set, line, n));
                }
            }
        }
        g.unsaved = 0;
    }
} // namespace

const char *EditorAnimation() {
    return g.on ? kAnimation : nullptr;
}
int EditorShift() {
    return g.on ? kShift : 0;
}
void EndEditor() {
    End();
}

// Before every frame (BeforeFrame): the editor ends with the viewer (the menu's Reset, another mode, a state
// loaded), and keeps Custom colours on while it runs (switched off, the files' colours would be put back under it)
void EditorCheck() {
    if (g.on && !TheDescription().viewerOn()) {
        End();
    } else if (g.on && !FeatureOn("custom_colours")) {
        SetFeature("custom_colours", true);
    }
}

void EditorBeforeFrame(Machine &machine) {
    if (!g.on) {
        return;
    }
    const int character = TheDescription().viewerCharacter();
    if (character != g.character) {
        Save();
        g.character = character;
        g.canUndo = false;
        g.lineWanted = g.line ? g.line : -1; // (the new character's animation for the line chosen)
    }
    const uint16_t now = machine.players[0];
    machine.players[0] = 0;
    const uint16_t pressed = static_cast<uint16_t>(now & ~g.before);
    g.before = now;
    // A direction acts when pressed, and again and again while it is held (faster for the levels)
    const uint16_t direction = now & (kUp | kDown | kLeft | kRight);
    g.heldFor = direction && direction == g.held ? g.heldFor + 1 : 0;
    g.held = direction;
    const int every = g.row >= kRowRed && g.row <= kRowBlue ? kHeldEveryLevel : kHeldEvery;
    const bool acts = g.heldFor == 0 || (g.heldFor >= kHeldFirst && (g.heldFor - kHeldFirst) % every == 0);
    if (direction && acts) {
        if (direction & (kUp | kDown)) {
            // (leaving the Load row puts back a set tried and not loaded; coming to it reads the saved ones)
            if (g.row == kRowLoad) {
                if (g.trying) {
                    Say("Put back");
                }
                PutBack();
            }
            g.row = (g.row + (direction & kUp ? kRows - 1 : 1)) % kRows;
            if (g.row == kRowLoad) {
                g.saved = SavedSets(g.character);
                g.loading = 0;
            }
        } else {
            Step(machine, direction & kRight ? 1 : -1);
        }
    }
    g.deleteFor = std::max(g.deleteFor - 1, 0);
    // (a first press counts only while the Random row stays chosen)
    g.randomFor = g.row == kRowRandom ? std::max(g.randomFor - 1, 0) : 0;
    // The animation that shows the line chosen (the game may take a moment to find out: asked each frame)
    if (g.lineWanted >= 0) {
        const int animation = TheDescription().animationForLine ? TheDescription().animationForLine(machine, g.lineWanted) : -1;
        if (animation >= 0) {
            SetFeatureValue(kAnimation, animation);
        }
        if (animation != -2) {
            g.lineWanted = -1;
        }
    }
    if (g.row == kRowLoad) {
        if (pressed & kPunch1) {
            Load();
        } else if (pressed & kKick2) {
            Delete();
        }
    } else if (pressed & kPunch1) {
        g.copied = Current();
        Say("Copied");
    } else if ((pressed & kPunch2) && g.copied >= 0) {
        Change(static_cast<uint16_t>(g.copied));
    } else if (pressed & kPunch3) {
        Change(GameColour(g.character, g.set, g.line, g.colour));
    } else if ((pressed & kKick1) && g.row == kRowRandom) {
        AskRandom(machine);
    } else if (pressed & kKick3) {
        Undo();
    }
    if (g.unsaved >= 0 && ++g.unsaved >= kSaveAfter) {
        Save();
    }
    g.blink = std::max(g.blink - 1, 0);
    g.saidFor = std::max(g.saidFor - 1, 0);
    ShowColours(machine);
}

void EditorFrame(Machine &machine) {
    if (g.on && g.character >= 0) {
        Draw(machine);
    }
}

void RegisterEditor() {
    Game &game = TheGame();
    game.pages.push_back({kPage, "Alternative modes",
                          "The characters' colours changed while you watch: any set, the character's own colours or its "
                          "effects', 16 levels of red, green and blue."});
    game.commands.push_back({"colours_edit", kPage, "Start",
                             "The model viewer's character with a panel worked with player 1's controls. Changes are "
                             "saved to the palettes folder and used in play (Custom colours). Reset ends it.",
                             Start});
    game.features.push_back({kAnimation, kPage, "Animation",
                             "What the character does in the editor: stands in the game's idle, or goes through one of "
                             "its animations (as in the model viewer). The panel has this row too.",
                             0, AnimationNames()});
    game.commands.push_back({"colours_reset_set", kPage, "Game's colours for this set",
                             "While editing: every colour of the set being edited back to the game's.", ResetSet});
    game.commands.push_back({"colours_random", kPage, "Random colours for this set",
                             "While editing: every colour of the set being edited at random (the panel's Random row does it "
                             "too, pressed twice: LK, left or right). HK puts back the colours from before, until the next change; "
                             "\"Game's colours for this set\" puts back the game's.",
                             Randomise});
    game.commands.push_back({"colours_reset_character", kPage, "Game's colours for this character",
                             "While editing: every set of the character being edited back to the game's colours.", ResetCharacter});
}

} // namespace cps2recomp::colours
