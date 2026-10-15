// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of cps2recomp.
//
// Shared between the port's entry point (main.cpp) and its window (frontend_sdl.cpp).
#pragma once

#include "../machine/machine.h"
#include "menu.h"
#include "present.h"

#include <cstdint>
#include <cstdio>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace port {

struct Options {
    std::string romPath = "rom";
    std::string eepromPath;    // the board's settings memory (created when missing)
    std::string moviePlay;     // input movie to replay
    std::string movieRecord;   // input movie to write
    std::string shotPrefix;    // PNG prefix for --shot-every
    std::string hashLog;       // per-frame state hash log
    std::string recordEntries; // entries file to add to (tools/recomp.py reads it)
    std::string dumpProgram;   // write the deciphered program here and stop
    std::string audioDump;     // WAV file of the sound (headless)
    std::string settingsPath;  // settings.ini with the player's feature choices
    std::string features;      // --features A,B,-C
    std::string menuTest;      // --menu FRAME:KEYS[:shot.png]
    std::string keys;          // --keys TICK:KEY,TICK:KEY...
    std::string dumpRam, poke; // --dump-ram FRAME:file, --poke ADDR=BYTE,...
    uint32_t watchLow = 1, watchHigh = 0; // --watch LOW[-HIGH]
    std::string trace;         // --trace LOW-HIGH:file
    bool checkRewrites = false, noRewrites = false;
    std::string cpu;           // native | blocks | interp | check (default: native when there is translated code)
    std::string sound;         // driver | z80 | check (default: driver when the game's sound program is a known one)
    std::string stateSave, stateLoad; // FRAME:path
    uint64_t frames = 0;       // stop after this many (0: run on)
    uint64_t shotEvery = 0;
    uint64_t autoSeed = 0;     // --auto: made-up input from this seed
    uint64_t soundTest = 0;    // --sound-test SEED[:EVERY]: every song and command sent to the sound side
    unsigned soundTestEvery = 30;
    uint64_t autoFrom = 0;     // ... from this frame on
    int autoPlayers = 2;       // ... for this many players
    uint64_t testFrom = 0, testTo = 0; // --test-switch FROM:TO: the operator's test switch held over these frames
    bool autoPlay = false;
    bool headless = false, mute = false, unthrottled = false, fullscreen = false, stats = false;
    bool hashPicture = false; // --hash-log hashes the picture only
    bool hashGame = false;     // --hash-game: the state hash without the Z80 and its own memory (to compare --sound driver and z80)
    int cpuSpeed = 1;         // --cpu-speed
    std::string renderer;     // --renderer sdl|gpu (gpu unless said)
    std::string presentTest;  // --present-test FILE.png: the last picture drawn by the GPU path with no window
    int presentWidth = 0, presentHeight = 0; // --present-size WxH: the window it is drawn in
    int scale = 0;            // --scale: the window this many times the board's lines (0: the Display page's)
};

// Recorded input: what the players hold, at the frames where it changes
struct Movie {
    std::map<uint64_t, std::pair<uint16_t, uint16_t>> changes;
    bool Load(const std::string &path);
    bool Save(const std::string &path) const;
};

struct Port {
    Options opt;
    cps2recomp::RomSet roms;
    std::unique_ptr<cps2recomp::Machine> machine;
    uint64_t frame = 0;       // frames run
    uint16_t pad[2] = {};     // what the players hold (cps2recomp::Button bits), set by the window
    bool service = false, test = false;
    Movie playMovie, recMovie;
    uint16_t lastRecorded[2] = {0xFFFF, 0xFFFF};
    FILE *hashFile = nullptr;
    uint64_t autoState = 0;
    uint16_t autoPad[2] = {};
    Menu menu;
    // Stopping and slowing the game: a tick is one picture of the screen; the game runs a frame in every
    // tick, or in one of so many (menu.SlowBy()), or while stopped only when asked to step
    uint64_t tick = 0;
    bool paused = false;
    int steps = 0;        // frames to go on by while stopped
    // One tick; true if the game ran (so that there is a new picture and sound)
    bool Tick();
    // The keys that work while playing: 'p' stops and starts, 'o' goes on by a frame, '-' and '=' change the
    // speed, 'k' does the game's command with the key `n` (runtime/game.h, kKeyRestart...)
    void Key(char key, unsigned n = 0);
    void RunCommand(size_t index);
    int coinFrames = 0;   // frames left of the menu's coin
    int start2Frames = 0; // ... and of its press of player 2's Start
    int testFrames = 0;   // ... and of its press of the test switch

    // What the menu asked for (it is closed if that went well)
    void MenuAct(Menu::Action action);
    bool quit = false; // the player chose the menu's "Quit game": the frontend stops
    // The game's picture at the size the menu is drawn at (4:3, three times the lines)
    void MenuPicture(std::vector<uint32_t> &pixels, uint32_t &width, uint32_t &height) const;
    // The picture with the readout over it (runtime/game.h, SetHud), as the window and screenshots show it
    void Shown(std::vector<uint32_t> &pixels) const;
    // How many pixels a line of it has (the machine's picture: wider when the game's sources ask)
    int ShownWidth() const {
        return machine->PictureWidth();
    }
    std::string StatePath(int slot) const;

    bool Init(std::string &error);
    // One frame: input (window, movie or made up), the machine, the per-frame outputs
    void RunFrame();
    void RunAhead();
    void SoundTest();
    uint64_t soundTestState = 0;
    uint16_t soundTestSong = 0;
    void Finish();
    uint32_t StateHash() const;
    bool SaveState(const std::string &path, std::string &error);
    bool LoadState(const std::string &path, std::string &error);
};

int RunWindowed(Port &port);

// The Display page's choices (the port's display_* and window_size features): how the picture is fitted to
// the window, and how many times the board's lines the window opens
Scaling DisplayScaling();
Shape DisplayShape();
int WindowSize();

} // namespace port
