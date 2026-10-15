// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of cps2recomp.
//
// The players' own colours for a fighting game's characters, and a colour editor to make them. A game's sources
// describe where the game keeps its characters' colours (Description) and register it; the rest is here:
//
// - Custom colours (the feature `custom_colours`, on the Display page): in the folder "palettes" beside
//   settings.ini, a copy of the ROM file that holds the colours, changed with PalMod (the players' palette
//   editor), gives its colours for the characters, and a text file for a character (its name, .txt) new colours
//   for any of its sets over the PalMod file's. They are written over the game's in the ROM image, where the
//   game reads them each time it colours a player, so the game's code is the same with them as without.
//   Commands write the game's own colours out as files to start from, and read the folder again.
// - The colour editor (Alternative modes → Colour editor): the game's model viewer with a panel at the right of
//   the picture, worked with player 1's controls. Its changes are saved to the character's file a little after
//   the last one, and a set can be saved on its own to the character's library (palettes/library/<name>/) and
//   loaded into any set.
//
// A character file: a section a set ([LP], by the description's names), each with up to a line a palette of up
// to 16 colours (#RRGGBB or #RGB, rounded to the board's 16 levels; - keeps the game's); a "#" with no hex digit
// after it starts a remark. A colour here is the board's 12 bits (4 each of red, green, blue: 0x0RGB).
#pragma once

#include "game.h"

#include <cstdint>
#include <string>
#include <vector>

namespace cps2recomp {
class Machine;
}

namespace cps2recomp::colours {

constexpr int kMostPalettes = 8;      // in a set (a description has palettesInSet up to this)
constexpr int kColoursInPalette = 16; // the board's; the 16th of an object's palette is the see-through pen

struct Character {
    int number;         // the game's own number for it
    const char *name;   // as files are called, and as the editor shows it
    const char *abroad; // the name the game has elsewhere (files may be called by it too), or nullptr
};

struct Range {
    uint32_t from, to; // ROM addresses, `to` not in it
};

struct Description {
    // The characters that have colours
    std::vector<Character> characters;
    // The sets each has, by the names the files and the editor give them (the game's own order)
    std::vector<const char *> setNames;
    int palettesInSet = 4;
    // The editor's names for a set's palettes ("Body", "Effect 1"... when not given)
    std::vector<const char *> lineNames;
    // Where a character's set is in the ROM (the data view, Program)
    uint32_t (*setAddress)(int number, int set) = nullptr;
    // Every place of the characters' colours (their sets and their effects' palettes): what a PalMod file may
    // change, and what is kept to put back
    std::vector<Range> (*ranges)() = nullptr;

    // The ROM file PalMod changes: its name, where it is in the program and how long, and a stretch of it that
    // must be the game's for the file to be taken (the tables of where the colours are, or anything PalMod
    // never changes). Program ROM files have the bytes of each word swapped against the program as the 68000
    // reads it (RomSet::program).
    const char *romFile = nullptr;
    uint32_t romFileAt = 0, romFileSize = 0;
    Range same = {0, 0};
    bool romFileSwapped = true;
    const char *romFileWhose = ""; // which set's it must be, as the message refusing a file says it

    // Words of the menu: the feature's description, and the remark at the head of each file "Write the game's
    // colours" writes (after the line naming the character)
    const char *featureDescription = "";
    const char *originalHelp = "";

    // The editor's preview: makes player 1 use the set (the player's own fields for it), and says where the
    // player's first palette is in the buffer the board shows from (0: nowhere now) and how many of the set's
    // palettes the game puts there for the character
    uint32_t (*showSet)(Machine &machine, int number, int set, int &lines) = nullptr;

    // The model viewer the editor works in (its page has the choices model_character, model_background and
    // model_size): start it; is it on; the character shown (by number); the number for a choice of
    // "Character" (-1: past the last); the animation after `choice` (by 1) or before it (-1), standing (0)
    // before the first and after the last; the "Animation" choice's names
    void (*startViewer)(Machine &machine) = nullptr;
    bool (*viewerOn)() = nullptr;
    int (*viewerCharacter)() = nullptr;
    int (*characterNumber)(int choice) = nullptr;
    int (*animationStep)(Machine &machine, int choice, int by) = nullptr;
    const std::vector<const char *> &(*animationNames)() = nullptr;
    // The animation (a choice of the editor's "Animation") that best shows a line of the character's set, for
    // the Line row to go to: 0 standing, -1 none found (the animation is left as it is), -2 not known yet (asked
    // again next frame). Not given: the Line row leaves the animation alone.
    int (*animationForLine)(Machine &machine, int line) = nullptr;
    // The "Background" choice as short as the panel needs it (the menu's own names when not given)
    std::vector<const char *> backgrounds;
};

// Adds the feature, its commands and the editor's page (a game's RegisterGame). The description is kept.
void Register(const Description &description);
// Before every frame (the game's beforeFrame, before the viewer's): the files' colours put in or taken out as
// the feature is switched, and the editor ended when its viewer is
void BeforeFrame(Machine &machine);

// The viewer's side of the editor (the viewer calls these while it runs):
// before a frame, after the viewer's own round is on: player 1's controls are the panel's (it zeroes them)
void EditorBeforeFrame(Machine &machine);
// after the frame, with the viewer's picture made: the panel over its right
void EditorFrame(Machine &machine);
// how far left of where the viewer would put the character it is drawn, so that the panel does not cover it
int EditorShift();
// the editor's own "Animation" choice, used in place of the viewer's while it is on (nullptr: the editor is off)
const char *EditorAnimation();
void EndEditor();

// The colours: the character's name, where its set is, a colour as it is now (with the files' and the editor's
// changes) and as the game has it, a colour changed (in the ROM image: used when the player is next coloured)
const char *Name(int number);
uint32_t SetAddress(int number, int set);
uint16_t Colour(int number, int set, int line, int n);
uint16_t GameColour(int number, int set, int line, int n);
void SetColour(int number, int set, int line, int n, uint16_t colour);
// Writes the character's colours that are not the game's to its file in the palettes folder (removes the file
// when they all are); false if there is no folder to write to
bool SaveColours(int number);

// The character's library: sets of colours saved on their own (palettes/library/<name>/<saved>.txt), to be
// put into any set. Their names, in order ("Lilith 2" before "Lilith 10"); the set saved as a new one (its
// name in `saved`); a saved one read (`colours` filled where it says, the rest left; false if it cannot be
// read); a saved one removed.
std::vector<std::string> SavedSets(int number);
bool SaveSet(int number, int set, std::string &saved);
bool ReadSavedSet(int number, const std::string &saved, uint16_t colours[kMostPalettes][kColoursInPalette]);
bool RemoveSavedSet(int number, const std::string &saved);

// (for the editor)
const Description &TheDescription();

} // namespace cps2recomp::colours
