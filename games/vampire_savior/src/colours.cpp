// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy
//
// Vampire Savior: where the game keeps its characters' colours, for the toolkit's custom colours and colour
// editor (fight/colours.h). The facts are in vsav.h and PROGRESS.md ("Custom colours").
#include "colours.h"
#include "machine.h"
#include "vsav.h"

namespace cps2recomp {

void StartModelViewer(Machine &machine); // model.cpp
bool ModelViewer();
int ModelCharacter();
int ModelCharacterNumber(int choice);
int ModelAnimationStep(Machine &machine, int choice, int by);
int ModelAnimationForLine(Machine &machine, int line);
const std::vector<const char *> &ModelAnimationNames();

namespace {
    using namespace vsav;

    uint32_t Long(uint32_t address) {
        const uint8_t *p = Program(address);
        return (static_cast<uint32_t>(p[0]) << 24) | (static_cast<uint32_t>(p[1]) << 16) | (static_cast<uint32_t>(p[2]) << 8) | p[3];
    }
    uint32_t SetAddress(int number, int set) {
        return Long(kColourSets + 4u * static_cast<uint32_t>(number)) + static_cast<uint32_t>(set) * kColourSetSize;
    }
    // Every character number's sets and its effects' palettes (the tables have an entry for each number)
    std::vector<colours::Range> Ranges() {
        std::vector<colours::Range> ranges;
        for (int n = 0; n < kColourNumbers; n++) {
            const uint32_t sets = Long(kColourSets + 4u * static_cast<uint32_t>(n));
            ranges.push_back({sets, sets + kColourSetSize * kColourSetsEach});
        }
        for (int n = 0; n < kColourNumbers; n++) {
            const uint32_t effects = Long(kEffectColours + 4u * static_cast<uint32_t>(n));
            ranges.push_back({effects, effects + static_cast<uint32_t>(kEffectPalettes * kColoursInPalette * 2)});
        }
        return ranges;
    }
    // Player 1 in the set, and where its palettes are in the buffer the board shows from (vsav.h)
    uint32_t ShowSet(Machine &, int number, int set, int &lines) {
        SetRam8(Player(0) + kColours, static_cast<uint8_t>(set));
        SetRam32(Player(0) + kColourSet, SetAddress(number, set));
        lines = (kFirstLineOnly >> (number & 31)) & 1 ? 1 : kPalettesInSet;
        return Ram32(kPaletteBuffer) + static_cast<uint32_t>(Ram8(Player(0) + kPaletteSlot)) * kColoursInPalette * 2;
    }
} // namespace

void RegisterColours() {
    colours::Description d;
    d.characters = {
        {0, "Bulleta", "BB Hood"},   {1, "Demitri", nullptr},    {2, "Gallon", "Talbain"},
        {3, "Victor", nullptr},      {4, "Zabel", "Lord Raptor"}, {5, "Morrigan", nullptr},
        {6, "Anakaris", nullptr},    {7, "Felicia", nullptr},    {8, "Bishamon", nullptr},
        {9, "Aulbath", "Rikuo"},     {10, "Sasquatch", nullptr}, {12, "Q-Bee", nullptr},
        {13, "Lei-Lei", "Hsien-Ko"}, {14, "Lilith", nullptr},    {15, "Jedah", nullptr},
        {kDarkGallon, "Dark Gallon", "Dark Talbain"}, {kOboroBishamon, "Oboro Bishamon", nullptr},
    };
    // (by what chooses them: vsav.h, kColours)
    d.setNames = {"LP", "MP", "HP", "LK", "MK", "HK", "PP", "KK", "AUTO1", "AUTO2"};
    d.palettesInSet = kPalettesInSet;
    d.setAddress = SetAddress;
    d.ranges = Ranges;
    d.romFile = "vm3j.10b";
    d.romFileAt = 0x380000;
    d.romFileSize = 0x80000;
    d.same = {kColourSets, kEffectColours + kColourNumbers * 4};
    d.romFileWhose = "the Japanese set's, 970519";
    d.featureDescription = "Characters in the colours of the files in the \"palettes\" folder in the game's data folder: a "
                           "vm3j.10b changed with PalMod, and text files, one a character (Morrigan.txt), which go before "
                           "it. With no files the game's own colours. They are taken when a player is coloured: at the next "
                           "round.";
    d.originalHelp = "# To change them, copy this file to the palettes folder and change the colours there. Each set\n"
                     "# is chosen by the buttons in its name ([PP]: two punches at once, [KK]: two kicks; [AUTO1] and\n"
                     "# [AUTO2]: chosen for auto guard) and has four lines of 16 colours; the character is drawn in\n"
                     "# the first line's, the others are its effects. The 16th colour of a line is never seen. A\n"
                     "# colour is #RRGGBB or #RGB, rounded to the board's 16 levels of each; - keeps the game's.\n"
                     "# Sets and lines that are left out keep the game's colours.\n";
    d.showSet = ShowSet;
    d.startViewer = StartModelViewer;
    d.viewerOn = ModelViewer;
    d.viewerCharacter = ModelCharacter;
    d.characterNumber = ModelCharacterNumber;
    d.animationStep = ModelAnimationStep;
    d.animationNames = ModelAnimationNames;
    d.animationForLine = ModelAnimationForLine;
    d.backgrounds = {"Stage 1", "Stage 2", "Stage 3", "Stage 4", "Stage 5", "Stage 6", "Stage 7",
                     "Stage 8", "Stage 9", "Stage 10", "Stage 11", "Stage 12", "White", "Black"};
    colours::Register(d);
}

} // namespace cps2recomp
