// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy
//
// Vampire Savior: what the modes of the menu's "Alternative modes" page share (modes.cpp: attract mode and
// the stage viewer; battle.cpp: survival, the endless battle and the ring out). Most of it is for running more
// than one machine: the port's own, which is what is seen, and others kept as save states ("spares"), which
// are loaded into the port's machine, run and put away again within a frame.
#pragma once

#include "machine.h"
#include "vsav.h"

#include <cstdint>
#include <functional>
#include <vector>

namespace cps2recomp {

// What the machine that is running is to fight when the game next makes a match, for the game's own choice
// of stage (training.cpp's hook): the stage (1-12; 0: the game's choice), the two characters and their
// colours (-1: the ones chosen at the character choice). Whoever runs a machine sets this first.
struct MatchWanted {
    int stage = 0;
    int characters[2] = {-1, -1};
    int colours[2] = {-1, -1}; // which of a character's sets of colours (-1: as chosen)
};
MatchWanted &Wanted();
// The character a mode's fighter on side n (0, 1) is given for `character`: its hidden form when the "Hidden
// characters" feature asks for that side (choosing.cpp), or itself
int HiddenForm(int n, int character);

// Where the machine that is running is to have its camera (the picture's left edge and bottom, in the
// stage), whatever the players do: -1 leaves that to the game.
struct CameraWanted {
    int x = -1, y = -1;
};
CameraWanted &CameraAt();

// A round of a match is being fought
bool RoundFought();
// The controls of a machine on its way from switching on to a round of a two-player game: coins in, both
// Starts until both players are in, then the characters and their choices taken as they come (player 1's
// left to `one`, the controls as the port has them, when `choose` is set). True once the round is on.
bool ToRound(Machine &machine, int32_t &frames, bool choose = false);
// A sound asked for as the game asks: a tune's number, or kTuneOff before another tune
void PlaySound(uint16_t code);
constexpr uint16_t kStageTune = 0x09; // a stage's own tune: this and its number less one
constexpr uint16_t kTuneOff = 0xFF05;

// Objects by what they are (for Video::hideObject). The players, what they throw and sparks use colour sets
// 5-21, the panel (bars, names, the clock, the gauges) 0-4 and, for the faces beside the bars, the players'
// own with tiles 13800-143FF; a stage's own pieces 22-31; the players' shadows are in set 22 with some stages'
// pieces, and are told by their tiles.
bool IsShadow(uint32_t tile, uint16_t attributes);
bool NotTheStage(uint32_t tile, uint16_t attributes); // everything but the stage's pieces
bool ThePanel(uint32_t tile, uint16_t attributes);    // the panel only
bool NotTheFighters(uint32_t tile, uint16_t attributes); // the panel and the stage's pieces

// A frame's picture and sound, taken out of the machine to be put together with another's
struct Made {
    std::vector<uint32_t> picture;
    std::vector<int16_t> sound;
};
// The frame the machine has just run: its picture, and the sound made since `soundBefore` samples (which is
// taken out of the machine's)
void Take(Machine &machine, size_t soundBefore, Made &made);
// ... and what is to be seen and heard for the frame put in
void Give(Machine &machine, const Made &made);

// A machine kept as a save state
struct Spare {
    std::vector<uint8_t> state; // (empty: not yet switched on)
    bool ready = false;         // its round is on
    int32_t frames = 0;         // frames run on the way there
    void Clear() {
        state.clear();
        ready = false;
        frames = 0;
    }
};
// Runs the spare for a frame in the port's machine, which is left as it was. `before` is called with the
// spare loaded, before its frame: it sets the controls, Wanted(), CameraAt(), the video's switches, and may
// change the game's memory. `made`, if given, gets the frame's picture and sound. `after`, if given, is
// called after the frame with the spare still loaded (to read its memory, or change it).
void RunSpare(Machine &machine, Spare &spare, const std::function<void(Machine &)> &before, Made *made = nullptr,
              const std::function<void(Machine &)> &after = nullptr);
// The spare's machine becomes the port's (and the spare is cleared)
void Promote(Machine &machine, Spare &spare);

// Modes by the files that have them: whether one is on, and ending it
bool AttractMode();  // modes.cpp
bool StageViewer();
void EndModes();
bool BattleMode();   // battle.cpp
void EndBattle();
// The colour sets of a player's own objects (the first player's 10-15, the second's 16-21), as a mask of bits
constexpr uint32_t kPlayerSets[2] = {0x0000FC00, 0x003F0000};
bool BattleNoClock();     // the round's clock stands still
bool BattleNoKnockOut();  // nobody's health goes below nothing

} // namespace cps2recomp
