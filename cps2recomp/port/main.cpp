// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of cps2recomp.
//
// The port's entry point: reads the ROM set, runs the game, shows and plays what it makes.
#include "game_info.h"

#include "../runtime/game.h"
#include "../runtime/lift.h"
#include "../runtime/native.h"
#include "../runtime/native_z80.h"
#include "../runtime/state.h"
#include "hud.h"
#include "../runtime/board.h"
#include "png_write.h"
#include "present.h"
#include "../runtime/spare.h"
#include "port.h"
#include "../recomp-common/settings.h"
#include "../recomp-common/text.h"

#include <SDL3/SDL_main.h> // (on Windows the game is a windowed program: its main is called from WinMain)

#include <zlib.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <set>
#ifdef _WIN32
#include <windows.h>
#endif

using namespace cps2recomp;

namespace port {

bool Movie::Load(const std::string &path) {
    FILE *f = fopen(path.c_str(), "r");
    if (!f) {
        return false;
    }
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        unsigned long long frame;
        unsigned p1, p2;
        if (line[0] != '#' && sscanf(line, "%llu %x %x", &frame, &p1, &p2) == 3) {
            changes[frame] = {static_cast<uint16_t>(p1), static_cast<uint16_t>(p2)};
        }
    }
    fclose(f);
    return true;
}

bool Movie::Save(const std::string &path) const {
    FILE *f = fopen(path.c_str(), "w");
    if (!f) {
        return false;
    }
    fprintf(f, "# cps2recomp input movie: frame player1 player2 (pressed bits, hex)\n");
    for (const auto &[frame, pads] : changes) {
        fprintf(f, "%llu %04x %04x\n", static_cast<unsigned long long>(frame), pads.first, pads.second);
    }
    fclose(f);
    return true;
}

namespace {
    constexpr uint32_t kStateVersion = 5; // (5: the 68000's wait kept)

    bool ReadFile(const std::string &path, std::vector<uint8_t> &data) {
        FILE *f = fopen(path.c_str(), "rb");
        if (!f) {
            return false;
        }
        fseek(f, 0, SEEK_END);
        data.resize(static_cast<size_t>(ftell(f)));
        fseek(f, 0, SEEK_SET);
        const bool ok = fread(data.data(), 1, data.size(), f) == data.size();
        fclose(f);
        return ok;
    }
    bool WriteFile(const std::string &path, const void *data, size_t size) {
        FILE *f = fopen(path.c_str(), "wb");
        if (!f) {
            return false;
        }
        const bool ok = fwrite(data, 1, size, f) == size;
        fclose(f);
        return ok;
    }
    // "FRAME:path"
    bool AtFrame(const std::string &spec, uint64_t frame, std::string &path) {
        const size_t colon = spec.find(':');
        if (spec.empty() || colon == std::string::npos || strtoull(spec.c_str(), nullptr, 10) != frame) {
            return false;
        }
        path = spec.substr(colon + 1);
        return true;
    }
    uint64_t Random(uint64_t &state) {
        state ^= state << 13;
        state ^= state >> 7;
        state ^= state << 17;
        return state;
    }
} // namespace

// Every game's features that come from the port itself (after the game's own: RegisterGame), for the game and
// for the list the launcher asks for (--list-features)
void RegisterPortFeatures() {
    // (run_ahead: Port::RunAhead)
    TheGame().features.push_back({"run_ahead", "Game", "Run-ahead",
                                  "Shows the game a frame or two further on than it is, so that what you press is seen sooner than "
                                  "on the board (input lag taken away). The game runs as it does without it; it takes two or three "
                                  "times the processor. Off by itself in the game's own modes beyond fights and training.",
                                  0, {"Off", "1 frame", "2 frames"}});
    // (display_*: how the picture is fitted to the window, present.h; the first of each is how it always was)
    TheGame().features.push_back({"display_scaling", "Display", "Scaling",
                                  "Sharp: as large as fits, each pixel a crisp block. Whole pixels: every pixel the same whole "
                                  "number of the screen's pixels tall (a border round it). Smooth: softened all over.",
                                  0, {"Sharp", "Whole pixels", "Smooth"}});
    TheGame().features.push_back({"display_shape", "Display", "Picture shape",
                                  "4:3: as the arcade's monitor showed it. Square pixels: each pixel as wide as it is tall, "
                                  "a little wider than 4:3. Fill: the whole window or screen, stretched to it.",
                                  0, {"4:3", "Square pixels", "Fill"}});
    TheGame().features.push_back({"window_size", "Display", "Window size",
                                  "How large the window opens, in times the board's 224 lines (it can be made any size by "
                                  "dragging its edges; full screen is on the launcher's first page and F11).",
                                  0, {"3x", "2x", "4x", "5x", "6x"}});
}

Scaling DisplayScaling() {
    return static_cast<Scaling>(std::clamp(FeatureValue("display_scaling"), 0, 2));
}
Shape DisplayShape() {
    return static_cast<Shape>(std::clamp(FeatureValue("display_shape"), 0, 2));
}
int WindowSize() {
    static const int kTimes[] = {3, 2, 4, 5, 6};
    return kTimes[std::clamp(FeatureValue("window_size"), 0, 4)];
}

bool Port::Init(std::string &error) {
    const RomList list = ParseRomList(GAME_PROGRAM, GAME_GRAPHICS, GAME_SOUND_PROGRAM, GAME_SAMPLES, GAME_KEY, GAME_ROM_SET, GAME_ROM_PARENT);
    if (!LoadRomSet(opt.romPath, list, roms, error)) {
        return false;
    }
    machine = std::make_unique<Machine>(roms);
    machine->cpuSpeed = opt.cpuSpeed > 0 ? opt.cpuSpeed : 1;
    SetRam(machine->ram.data());
    SetProgram(roms.program.data(), static_cast<uint32_t>(roms.program.size()));
    SetEeprom(&machine->eeprom);
    RegisterGame();
    RegisterPortFeatures();
    LoadFeatures(opt.settingsPath, opt.features);
    // The 68000 and the Z80 are run the same way; each falls back to its interpreter if the build has no
    // translated code for it
    if (opt.cpu.empty()) {
        opt.cpu = HasNativeCode() || HasZ80Code() ? "native" : "interp";
    }
    if (opt.cpu == "native") {
        machine->cpuMode = M68K::Mode::Native;
        machine->soundMode = Z80::Mode::Native;
    } else if (opt.cpu == "blocks") {
        machine->cpuMode = M68K::Mode::Blocks;
        machine->soundMode = Z80::Mode::Blocks;
    } else if (opt.cpu == "interp") {
        machine->cpuMode = M68K::Mode::Interpreter;
        machine->soundMode = Z80::Mode::Interpreter;
    } else if (opt.cpu == "check") {
        machine->cpuMode = M68K::Mode::Check;
        machine->soundMode = Z80::Mode::Check;
    } else {
        error = "--cpu: native, blocks, interp or check";
        return false;
    }
    // The sound program as ordinary code where there is one for it, unless the Z80's code is what is being
    // looked at
    if (opt.sound.empty()) {
        opt.sound = machine->soundDriver && opt.cpu == "native" && opt.recordEntries.empty() ? "driver" : "z80";
    }
    if (opt.sound == "z80") {
        machine->soundSide = Machine::SoundSide::Z80;
    } else if ((opt.sound == "driver" || opt.sound == "check") && machine->soundDriver) {
        machine->soundSide = opt.sound == "driver" ? Machine::SoundSide::Driver : Machine::SoundSide::Check;
    } else {
        error = machine->soundDriver ? "--sound: driver, z80 or check" : "--sound: this game's sound program is not one there is a driver for";
        return false;
    }
    machine->soundCpu.recording = !opt.recordEntries.empty();
    SetRewrites(opt.checkRewrites, opt.noRewrites);
    machine->cpu.watchLow = opt.watchLow;
    machine->cpu.watchHigh = opt.watchHigh;
    machine->cpu.recording = !opt.recordEntries.empty();
    if (!opt.eepromPath.empty()) {
        std::vector<uint8_t> data;
        if (ReadFile(opt.eepromPath, data) && data.size() == sizeof(machine->eeprom.words)) {
            memcpy(machine->eeprom.words, data.data(), data.size());
        }
    }
    if (!opt.moviePlay.empty() && !playMovie.Load(opt.moviePlay)) {
        error = "cannot read the movie " + opt.moviePlay;
        return false;
    }
    if (!opt.hashLog.empty()) {
        hashFile = fopen(opt.hashLog.c_str(), "w");
    }
    autoState = opt.autoSeed * 0x9E3779B97F4A7C15ull + 0x1234567ull;
    return true;
}

// What the game's state comes to, for comparing runs: the CPU, its memory, the video and object memory, the
// registers and the picture
uint32_t Port::StateHash() const {
    const Machine &m = *machine;
    uLong h = crc32(0, nullptr, 0);
    auto add = [&](const void *p, size_t size) { h = crc32(h, static_cast<const Bytef *>(p), static_cast<uInt>(size)); };
    if (opt.hashPicture) {
        add(m.Picture(), sizeof(uint32_t) * m.PictureWidth() * Video::kHeight);
        return static_cast<uint32_t>(h);
    }
    add(m.cpu.d, sizeof(m.cpu.d));
    add(m.cpu.a, sizeof(m.cpu.a));
    add(&m.cpu.pc, sizeof(m.cpu.pc));
    const uint16_t sr = m.cpu.SR();
    add(&sr, sizeof(sr));
    add(&m.cpu.cycles, sizeof(m.cpu.cycles));
    add(m.ram.data(), m.ram.size());
    add(m.videoRam.data(), m.videoRam.size());
    add(m.objectRam.data(), m.objectRam.size());
    if (!opt.hashGame) {
        add(m.soundRam, sizeof(m.soundRam));
    } else {
        // (byte 5 is the sound program's note of the last place it gave a sound, which it writes when it
        // comes to it; the game does not read it)
        add(m.soundRam, 5);
        add(m.soundRam + 6, sizeof(m.soundRam) - 6);
    }
    if (!opt.hashGame) {
        add(m.soundRam2, sizeof(m.soundRam2));
        add(m.soundCpu.r, sizeof(m.soundCpu.r));
        add(m.soundCpu.r2, sizeof(m.soundCpu.r2));
        const uint16_t sound[] = {m.soundCpu.pc, m.soundCpu.sp, m.soundCpu.ix, m.soundCpu.iy, m.soundCpu.f, m.soundCpu.f2,
                                  m.soundCpu.refresh};
        add(sound, sizeof(sound));
        add(&m.soundCpu.cycles, sizeof(m.soundCpu.cycles));
    }
    add(m.cpsA, sizeof(m.cpsA));
    add(m.cpsB, sizeof(m.cpsB));
    add(m.objectRegs, sizeof(m.objectRegs));
    add(m.Picture(), sizeof(uint32_t) * m.PictureWidth() * Video::kHeight);
    return static_cast<uint32_t>(h);
}

void Port::MenuAct(Menu::Action action) {
    std::string error;
    const std::string path = StatePath(menu.Slot());
    switch (action) {
    case Menu::Action::None: return;
    case Menu::Action::Coin:
        coinFrames = 6;
        menu.Close();
        return;
    case Menu::Action::Start2:
        start2Frames = 6;
        menu.Close();
        return;
    case Menu::Action::Test:
        testFrames = 6;
        menu.Close();
        return;
    case Menu::Action::Reset:
        EndSessionFeatures();
        machine->Reset();
        menu.Close();
        return;
    case Menu::Action::Quit:
        quit = true;
        menu.Close();
        return;
    case Menu::Action::Command:
        RunCommand(menu.Command());
        menu.Close();
        return;
    case Menu::Action::Save:
        if (SaveState(path, error)) {
            menu.Close();
            return;
        }
        break;
    case Menu::Action::Load:
        if (LoadState(path, error)) {
            menu.Close();
            return;
        }
        error = "Nothing is kept in slot " + std::to_string(menu.Slot()) + ".";
        break;
    }
    menu.Note(error);
}

void Port::RunCommand(size_t index) {
    const auto &commands = TheGame().commands;
    if (index < commands.size() && commands[index].run) {
        commands[index].run(*machine);
    }
}

void Port::Key(char key, unsigned n) {
    switch (key) {
    case 'p':
        paused = !paused;
        steps = 0;
        break;
    case 'o': // (stops the game if it is running)
        if (paused) {
            steps++;
        }
        paused = true;
        break;
    case '-': menu.SetSlowBy(menu.SlowBy() * 2); break;
    case '=': menu.SetSlowBy(menu.SlowBy() / 2); break;
    case 'k':
        for (size_t i = 0; i < TheGame().commands.size(); i++) {
            if (TheGame().commands[i].key == n && n != kKeyNone) {
                RunCommand(i);
            }
        }
        break;
    default: break;
    }
}

bool Port::Tick() {
    // (tests: --keys TICK:KEY,...: keys pressed at those ticks; a digit after 'k' is the command key's number)
    for (size_t at = 0; at < opt.keys.size();) {
        const size_t colon = opt.keys.find(':', at);
        if (colon == std::string::npos) {
            break;
        }
        if (strtoull(opt.keys.c_str() + at, nullptr, 10) == tick && colon + 1 < opt.keys.size()) {
            const char key = opt.keys[colon + 1];
            const size_t end = std::min(opt.keys.find(',', colon), opt.keys.size());
            if (key == 'c') { // (c and a command's name)
                const std::string name = opt.keys.substr(colon + 2, end - colon - 2);
                for (size_t i = 0; i < TheGame().commands.size(); i++) {
                    if (name == TheGame().commands[i].name) {
                        RunCommand(i);
                    }
                }
            } else if (key == 'f') { // (f and NAME=N: a feature set)
                const std::string what = opt.keys.substr(colon + 2, end - colon - 2);
                const size_t eq = what.find('=');
                if (eq != std::string::npos) {
                    SetFeatureValue(what.substr(0, eq).c_str(), atoi(what.c_str() + eq + 1));
                }
            } else {
                Key(key, key == 'k' ? static_cast<unsigned>(atoi(opt.keys.c_str() + colon + 2)) : 0);
            }
        }
        const size_t comma = opt.keys.find(',', colon);
        at = comma == std::string::npos ? opt.keys.size() : comma + 1;
    }
    tick++;
    if (FastForward()) {
        // The game's sources are taking the game somewhere: as fast as it goes, a slice at a time
        for (int n = 0; n < 120 && FastForward(); n++) {
            RunFrame();
            machine->audio.clear();
        }
        return true;
    }
    if (paused) {
        if (steps == 0) {
            return false;
        }
        steps--;
    } else if (tick % static_cast<uint64_t>(menu.SlowBy()) != 0) {
        return false;
    }
    RunFrame();
    return true;
}

void Port::Shown(std::vector<uint32_t> &pixels) const {
    const int across = ShownWidth();
    pixels.assign(machine->Picture(), machine->Picture() + across * Video::kHeight);
    DrawHud(pixels.data(), across, Video::kHeight);
    // Stopped or slowed: said at the bottom, with the frame's number (for counting frames by stepping)
    if (paused || menu.SlowBy() > 1) {
        char status[64];
        if (paused) {
            snprintf(status, sizeof(status), "STOPPED  frame %llu", static_cast<unsigned long long>(frame));
        } else {
            snprintf(status, sizeof(status), "SPEED 1/%d", menu.SlowBy());
        }
        const recomp::text::Canvas canvas{pixels.data(), across, Video::kHeight};
        const float width = recomp::text::TextWidth(status, 1);
        const float x = (across - width) / 2, y = Video::kHeight - 11;
        canvas.Fill(x - 3, y - 2, width + 6, 12, {20, 22, 30, 220});
        canvas.Text(x, y, 1, 0, {255, 255, 255, 255}, status);
    }
}

void Port::MenuPicture(std::vector<uint32_t> &pixels, uint32_t &width, uint32_t &height) const {
    std::vector<uint32_t> shown;
    Shown(shown);
    const uint32_t across = static_cast<uint32_t>(ShownWidth());
    height = Video::kHeight * 3;
    width = height * 4 / 3 * across / Video::kWidth;
    pixels.resize(static_cast<size_t>(width) * height);
    for (uint32_t y = 0; y < height; y++) {
        const uint32_t *from = &shown[static_cast<size_t>(y / 3) * across];
        uint32_t *to = &pixels[static_cast<size_t>(y) * width];
        for (uint32_t x = 0; x < width; x++) {
            to[x] = from[x * across / width];
        }
    }
}

std::string Port::StatePath(int slot) const {
    const size_t slash = opt.eepromPath.find_last_of('/');
    const std::string folder = slash == std::string::npos ? "" : opt.eepromPath.substr(0, slash + 1);
    return folder + "state" + std::to_string(slot) + ".bin";
}

bool Port::SaveState(const std::string &path, std::string &error) {
    std::vector<uint8_t> data;
    Archive ar(data, true);
    ar.Mark(0x43505332);
    uint32_t version = kStateVersion;
    ar.Pod(version);
    machine->Serialize(ar);
    if (TheGame().state) {
        TheGame().state(ar);
    }
    if (!WriteFile(path, data.data(), data.size())) {
        error = "cannot write " + path;
        return false;
    }
    return true;
}

bool Port::LoadState(const std::string &path, std::string &error) {
    std::vector<uint8_t> data;
    if (!ReadFile(path, data)) {
        error = "cannot read " + path;
        return false;
    }
    Archive ar(data, false);
    ar.Mark(0x43505332);
    uint32_t version = 0;
    ar.Pod(version);
    if (ar.Ok() && version == kStateVersion) {
        machine->Serialize(ar);
        if (TheGame().state) {
            TheGame().state(ar);
        }
    }
    if (!ar.Ok() || version != kStateVersion) {
        error = path + " is not a state this build wrote";
        machine->Reset();
        return false;
    }
    return true;
}

// --sound-test: once the game has the sound side going, a command every so many frames in place of whatever
// the game sends: each song in turn with made-up placing, and now and then one of the other commands with
// made-up numbers. For --sound check: far more of the driver than a game's play reaches.
void Port::SoundTest() {
    uint8_t *shared = machine->soundRam;
    machine->monoForTest = opt.soundTest % 4 == 0; // (every fourth seed)
    // (not at once: the sound program takes the game's stereo setting in its first interrupts)
    if (shared[0xFFD] != 0x88 || shared[0xFFF] != 0xFF || machine->frame < 400 || machine->frame % opt.soundTestEvery ||
        shared[0x00F] != 0xFF) {
        return;
    }
    auto random = [&]() {
        soundTestState = soundTestState * 6364136223846793005ull + 1442695040888963407ull;
        return static_cast<uint32_t>(soundTestState >> 33);
    };
    if (!soundTestState) {
        soundTestState = opt.soundTest;
        // Songs of the kinds this game's sound ROM may have none of, made up and put in the ROM's empty end
        // under song numbers the game does not use: single samples, ADPCM samples, and sequences with the
        // commands sequences seldom have (the driver's layout: sound/driver.cpp)
        std::vector<uint8_t> &rom = roms.soundProgram;
        const uint32_t table = static_cast<uint32_t>(rom[0x3FE0] | (rom[0x3FE1] << 8));
        const unsigned songs = static_cast<unsigned>((rom[table] << 8) | rom[table + 1]);
        uint32_t at = 0x30000;
        unsigned slot = 0;
        auto add = [&](const std::vector<uint8_t> &song) {
            for (; slot < songs; slot++) {
                uint8_t *entry = &rom[table + 5 + slot * 4];
                if (!(entry[0] | entry[1] | entry[2]) && at + song.size() <= rom.size()) {
                    entry[0] = static_cast<uint8_t>(at >> 16), entry[1] = static_cast<uint8_t>(at >> 8), entry[2] = static_cast<uint8_t>(at);
                    memcpy(&rom[at], song.data(), song.size());
                    at += static_cast<uint32_t>(song.size());
                    slot++;
                    return;
                }
            }
        };
        auto byte = [&](uint32_t range) {
            return static_cast<uint8_t>(random() % range);
        };
        auto track = [&](bool music) {
            std::vector<uint8_t> t = {0xC1, 0x01, byte(256), 0xC2, 0x00, 0xC4, byte(60), 0xC6, static_cast<uint8_t>(0x40 + byte(0x40)),
                                      0xC8, static_cast<uint8_t>(0x40 + byte(0x40)), 0xDE, byte(256), 0xDF, byte(256), 0xDF, byte(256),
                                      0xDC, byte(256), 0xDD, byte(256), 0xDD, byte(256), 0xC3, byte(256), 0xE1, byte(128), 0xC5, byte(128),
                                      0xE2, byte(128), 0xE0, byte(2), 0xE3, byte(0x30), 0xC9, byte(128), 0xE6, byte(128)};
            if (music) {
                t.insert(t.end(), {0xC7, byte(128)});
            }
            auto note = [&](bool tied) {
                t.insert(t.end(), {static_cast<uint8_t>(0x80 + byte(0x40)), static_cast<uint8_t>(0x24 + byte(0x30) + (tied ? 0x80 : 0))});
                if (byte(4) == 0) {
                    t.push_back(static_cast<uint8_t>(0x81 + byte(3)));
                }
                t.push_back(byte(128));
                if (byte(5) == 0) {
                    t.push_back(1);
                }
                t.push_back(static_cast<uint8_t>(1 + byte(60)));
            };
            auto jumpTo = [&](size_t operand, size_t target) {
                const int offset = static_cast<int>(target) - static_cast<int>(operand + 2);
                t[operand] = static_cast<uint8_t>(offset >> 8), t[operand + 1] = static_cast<uint8_t>(offset);
            };
            t.insert(t.end(), {0xC0, 0xD3});
            note(false);
            const size_t out = t.size() + 1;
            t.insert(t.end(), {0xDB, 0, 0});
            note(true);
            note(false);
            t.insert(t.end(), {0xD7, static_cast<uint8_t>(byte(4))});
            if (t.back() == 0) {
                t.back() = 2;
            }
            jumpTo(out, t.size());
            const size_t second = t.size() + 1;
            t.insert(t.end(), {0xCD, 0, 0, 0xCB});
            note(false);
            t.push_back(0xCA);
            jumpTo(second, t.size());
            note(false);
            const size_t end = t.size() + 1;
            t.insert(t.end(), {0xCC, 0, 0});
            note(false);
            jumpTo(end, t.size());
            t.insert(t.end(), {0xCB, 0xFF});
            return t;
        };
        // (and of the first table's instruments, some whose notes start again after a sound effect and take
        // the game's pitch shift, and some that are silent once they have decayed)
        const uint32_t instruments = static_cast<uint32_t>(rom[0x3FE4] | (rom[0x3FE5] << 8));
        for (unsigned n = 40; n < 60; n++) {
            if (n < 50) {
                rom[instruments + n * 8 + 5] = 0;
            } else {
                rom[instruments + n * 8 + 1] |= 0x80;
            }
        }
        for (int n = 0; n < 150; n++) {
            const uint32_t kind = random() % 5;
            if (kind < 2) {
                const bool instrument = random() % 2;
                add({static_cast<uint8_t>(0x80 + byte(16)), static_cast<uint8_t>(byte(0x20) | (byte(6) ? 0 : 0x80)), 0,
                     static_cast<uint8_t>(1 + byte(120)), 0, byte(100), byte(128), static_cast<uint8_t>(instrument ? 0x80 + byte(2) : 0),
                     byte(60), static_cast<uint8_t>(0x30 + byte(0x30)), byte(256)});
            } else if (kind == 2) {
                add({static_cast<uint8_t>(0x90 + byte(3)), static_cast<uint8_t>(byte(0x20) | (byte(4) ? 0 : 0x80)), byte(128), 0, byte(60)});
            } else {
                const bool music = kind == 4 && random() % 3 == 0;
                std::vector<uint8_t> song(33, 0);
                song[0] = music ? 0 : static_cast<uint8_t>(1 + byte(0x30));
                for (unsigned voice = 0; voice < 16; voice++) {
                    if (random() % 4 == 0) {
                        song[1 + voice * 2] = static_cast<uint8_t>(song.size() >> 8), song[2 + voice * 2] = static_cast<uint8_t>(song.size());
                        const std::vector<uint8_t> t = track(music);
                        song.insert(song.end(), t.begin(), t.end());
                    }
                }
                add(song);
            }
        }
    }
    uint8_t command[16] = {};
    const uint32_t pick = random() % 16;
    if (pick < 11) {
        command[0] = static_cast<uint8_t>(soundTestSong >> 8);
        command[1] = static_cast<uint8_t>(soundTestSong);
        soundTestSong++;
        command[6] = static_cast<uint8_t>(random() % 3 ? random() % 8 : 0);
        command[7] = static_cast<uint8_t>(random() % 0x24);
        command[8] = static_cast<uint8_t>(random() % 0x24);
        command[9] = static_cast<uint8_t>(random() % 3);
        command[10] = static_cast<uint8_t>(random());
        command[11] = static_cast<uint8_t>(random() % 3);
        command[12] = static_cast<uint8_t>(random());
    } else {
        static const uint8_t others[] = {0, 1, 2, 2, 3, 4, 5, 6, 7, 6, 7, 8, 9, 12, 13, 12, 13, 8, 9};
        command[0] = 0xFF;
        command[1] = others[random() % sizeof(others)];
        command[2] = static_cast<uint8_t>(random());
        command[3] = static_cast<uint8_t>(random() % 2 ? 0xFF : random() % 4);
        command[4] = static_cast<uint8_t>(command[1] == 2 ? random() % 16 : command[1] == 4 ? random() % 3 : random());
    }
    memcpy(shared, command, 15);
    shared[0x00F] = 0;
}

// Run-ahead: after the frame, the frames after it are run with the controls as they are and the picture (and
// the readout) of the last is the one shown; then the machine and the game's sources are put back as they were
// after the frame, and its sound is the frame's own. So the game goes on exactly as without it, and the picture
// shows what a press does a frame or two sooner. Not done for a frame a spare machine ran in (runtime/spare.h).
void Port::RunAhead() {
    const int ahead = FeatureValue("run_ahead");
    // (nor while stopped, stepped a frame at a time or slowed: there the frame shown is the frame)
    if (ahead <= 0 || FastForward() || SpareRan() || paused || menu.SlowBy() > 1 || SlowMotion() > 1 ||
        (TheGame().noRunAhead && TheGame().noRunAhead())) {
        return;
    }
    static std::vector<uint8_t> kept;
    kept.clear();
    {
        Archive ar(kept, true);
        machine->Serialize(ar);
        if (TheGame().state) {
            TheGame().state(ar);
        }
    }
    const size_t sound = machine->audio.size();
    const uint16_t held[2] = {machine->players[0], machine->players[1]};
    SetRunningAhead(true);
    for (int n = 0; n < ahead; n++) {
        machine->players[0] = held[0];
        machine->players[1] = held[1];
        if (TheGame().beforeFrame) {
            TheGame().beforeFrame(*machine);
        }
        machine->RunFrame();
        if (TheGame().frame) {
            TheGame().frame(*machine);
        }
    }
    {
        Archive ar(kept, false);
        machine->Serialize(ar);
        if (TheGame().state) {
            TheGame().state(ar);
        }
    }
    SetRunningAhead(false);
    machine->audio.resize(sound);
    machine->players[0] = held[0];
    machine->players[1] = held[1];
}

void Port::RunFrame() {
    ClearSpareRan();
    if (!opt.moviePlay.empty()) {
        auto it = playMovie.changes.find(frame);
        if (it != playMovie.changes.end()) {
            pad[0] = it->second.first;
            pad[1] = it->second.second;
        }
    } else if (opt.autoPlay) {
        // Made-up play, to reach as much of the game as it can: coins and start buttons now and then, sticks
        // and buttons at random, held for random lengths of time. The same seed gives the same run.
        if (frame >= opt.autoFrom) {
            for (int p = 0; p < opt.autoPlayers; p++) {
                if (Random(autoState) % 8 == 0) {
                    uint16_t held = 0;
                    const uint64_t r = Random(autoState);
                    static const uint16_t kSticks[] = {0, kLeft, kRight, kUp, kDown, kLeft | kDown, kRight | kDown,
                                                       kLeft | kUp, kRight | kUp, 0, kRight, kLeft};
                    held |= kSticks[r % 12];
                    held |= static_cast<uint16_t>((r >> 8) & (r >> 16) & 0x3F0); // punches and kicks
                    if ((r >> 32) % 24 == 0) {
                        held |= kStart;
                    }
                    if ((r >> 40) % 96 == 0) {
                        held |= kCoin;
                    }
                    autoPad[p] = held;
                }
            }
            pad[0] = autoPad[0];
            pad[1] = autoPad[1];
        }
    }
    if (!opt.movieRecord.empty() && (pad[0] != lastRecorded[0] || pad[1] != lastRecorded[1])) {
        recMovie.changes[frame] = {pad[0], pad[1]};
        lastRecorded[0] = pad[0];
        lastRecorded[1] = pad[1];
    }
    // (tests: --menu FRAME:KEYS[:shot.png]: the menu opened at that frame and those buttons pressed in it:
    // u d l r, a for a button)
    if (!opt.menuTest.empty() && strtoull(opt.menuTest.c_str(), nullptr, 10) == frame) {
        const size_t first = opt.menuTest.find(':'), second = opt.menuTest.find(':', first + 1);
        const std::string keys =
            opt.menuTest.substr(first + 1, second == std::string::npos ? std::string::npos : second - first - 1);
        menu.Open(true);
        for (const char key : keys) {
            MenuAct(menu.Buttons(key == 'u' ? kUp : key == 'd' ? kDown : key == 'l' ? kLeft : key == 'r' ? kRight : key == 'a' ? kPunch1 : 0));
        }
        if (second != std::string::npos && menu.IsOpen()) {
            std::vector<uint32_t> pixels;
            uint32_t width = 0, height = 0;
            MenuPicture(pixels, width, height);
            menu.Draw(pixels.data(), static_cast<int>(width), static_cast<int>(height));
            WritePng(opt.menuTest.substr(second + 1).c_str(), pixels.data(), width, height);
        }
        menu.Close();
    }
    machine->players[0] = pad[0];
    machine->players[1] = pad[1];
    if (coinFrames > 0) {
        coinFrames--;
        machine->players[0] |= kCoin;
    }
    if (start2Frames > 0) {
        start2Frames--;
        machine->players[1] |= kStart;
    }
    machine->serviceButton = service;
    machine->testSwitch = test || testFrames > 0 || (frame >= opt.testFrom && frame < opt.testTo);
    if (testFrames > 0) {
        testFrames--;
    }
    const uint16_t pressed[2] = {machine->players[0], machine->players[1]}; // (for run-ahead's frames)

    std::string path, error;
    if (AtFrame(opt.stateLoad, frame, path) && !LoadState(path, error)) {
        fprintf(stderr, "%s\n", error.c_str());
    }
    if (TheGame().beforeFrame) {
        TheGame().beforeFrame(*machine);
    }
    machine->RunFrame();
    if (opt.soundTest) {
        SoundTest();
    }
    if (TheGame().frame) {
        TheGame().frame(*machine);
    }
    frame++;
    // (for finding things out: --poke ADDR=BYTE,... every frame; --dump-ram FRAME:file)
    if (!opt.poke.empty()) {
        const char *at = opt.poke.c_str();
        while (*at) {
            char *end;
            const uint32_t address = static_cast<uint32_t>(strtoul(at, &end, 16));
            if (*end != '=') {
                break;
            }
            SetRam8(address, static_cast<uint8_t>(strtoul(end + 1, &end, 16)));
            at = *end == ',' ? end + 1 : end;
        }
    }
    if (!opt.trace.empty()) {
        // LOW-HIGH:FILE (hex addresses): those bytes of work RAM after every frame, one frame after another
        static FILE *file = nullptr;
        static uint32_t low = 0, high = 0;
        if (!file) {
            const size_t dash = opt.trace.find('-'), colon = opt.trace.find(':');
            low = static_cast<uint32_t>(strtoul(opt.trace.c_str(), nullptr, 16)) & 0xFFFF;
            high = static_cast<uint32_t>(strtoul(opt.trace.c_str() + dash + 1, nullptr, 16)) & 0xFFFF;
            file = fopen(opt.trace.c_str() + colon + 1, "wb");
        }
        if (file && high >= low) {
            fwrite(&machine->ram[low], 1, high - low + 1, file);
            fflush(file);
        }
    }
    if (AtFrame(opt.dumpRam, frame, path)) {
        WriteFile(path, machine->ram.data(), machine->ram.size());
        // (and beside it the video memory, and the video chips' and object registers as 16-bit words)
        WriteFile(path + ".video", machine->videoRam.data(), machine->videoRam.size());
        WriteFile(path + ".objects", machine->objectRam.data(), machine->objectRam.size());
        std::vector<uint16_t> regs(machine->cpsA, machine->cpsA + 32);
        regs.insert(regs.end(), machine->cpsB, machine->cpsB + 32);
        regs.insert(regs.end(), machine->objectRegs, machine->objectRegs + 6);
        WriteFile(path + ".regs", regs.data(), regs.size() * 2);
    }
    if (AtFrame(opt.stateSave, frame, path) && !SaveState(path, error)) {
        fprintf(stderr, "%s\n", error.c_str());
    }
    if (hashFile) {
        fprintf(hashFile, "%llu %08x\n", static_cast<unsigned long long>(frame), StateHash());
    }
    machine->players[0] = pressed[0];
    machine->players[1] = pressed[1];
    RunAhead();
    if (opt.shotEvery && frame % opt.shotEvery == 0) {
        char shot[512];
        snprintf(shot, sizeof(shot), "%s%06llu.png", opt.shotPrefix.c_str(), static_cast<unsigned long long>(frame));
        std::vector<uint32_t> shown;
        Shown(shown);
        WritePng(shot, shown.data(), ShownWidth(), Video::kHeight);
    }
}

void Port::Finish() {
    if (hashFile) {
        fclose(hashFile);
    }
    if (!opt.movieRecord.empty()) {
        recMovie.Save(opt.movieRecord);
    }
    if (!opt.eepromPath.empty() && machine->eeprom.changed) {
        WriteFile(opt.eepromPath, machine->eeprom.words, sizeof(machine->eeprom.words));
    }
    if (!opt.recordEntries.empty()) {
        // The entries files: one address a line, kept sorted; ours added to what is there. The 68000's, and
        // beside it (.z80) the sound driver's.
        auto merge = [](const std::string &path, const std::vector<uint32_t> &found, const char *format) {
            std::set<uint32_t> entries;
            if (FILE *f = fopen(path.c_str(), "r")) {
                char line[64];
                while (fgets(line, sizeof(line), f)) {
                    if (line[0] != '#' && line[0] != '\n') {
                        entries.insert(static_cast<uint32_t>(strtoul(line, nullptr, 16)));
                    }
                }
                fclose(f);
            }
            const size_t before = entries.size();
            entries.insert(found.begin(), found.end());
            if (entries.size() == before && before != 0) {
                return; // (nothing new: the file is left alone)
            }
            if (FILE *f = fopen(path.c_str(), "w")) {
                fprintf(f, "# Places the game's code was seen to reach in ways the translator cannot follow\n");
                for (uint32_t e : entries) {
                    fprintf(f, format, e);
                }
                fclose(f);
            }
            fprintf(stderr, "entries: %zu new (%zu in %s)\n", entries.size() - before, entries.size(), path.c_str());
        };
        merge(opt.recordEntries, machine->cpu.newEntries, "%06X\n");
        merge(opt.recordEntries + ".z80",
              std::vector<uint32_t>(machine->soundCpu.newEntries.begin(), machine->soundCpu.newEntries.end()), "%04X\n");
    }
}

} // namespace port

namespace {

void Usage() {
    fprintf(stderr,
            "%s\n"
            "  --rom PATH             the ROM set (%s): a folder with its files, or its zip\n"
            "  --eeprom FILE          where the board's settings memory is kept\n"
            "  --cpu MODE             native (translated code), blocks (the interpreter run the same way),\n"
            "                         interp (the interpreter, an instruction at a time), check\n"
            "  --sound SIDE           driver (the sound program as ordinary code), z80 (the program itself),\n"
            "                         check (both, every write to the chip compared)\n"
            "  --sound-test SEED[:N]  every N frames (30) a command to the sound side: each song in turn, placed\n"
            "                         some made-up way, and the other commands (with --sound check)\n"
            "  --scale N              window size, times 224 lines (else the Display page's)\n"
            "  --fullscreen  --mute  --unthrottled\n"
            "  --headless             no window, no sound, as fast as it goes\n"
            "  --frames N             stop after N frames\n"
            "  --shot-every N:PFX     write PFX<frame>.png every N frames\n"
            "  --hash-log FILE        a line per frame with a hash of the game's state\n"
            "  --hash-picture         ... of the picture only (to compare runs whose timing is not the same)\n"
            "  --hash-game            ... without the Z80 and its own memory (to compare --sound driver and z80)\n"
"  --renderer sdl|gpu     how the picture gets to the window: SDL's GPU API unless said (or not there)\n"
            "  --present-test F.png   (headless) the run's last picture drawn by the GPU path, no window, as the\n"
            "                         Display page has it (896x672 at 4:3)\n"
            "  --present-size WxH     ... in a window this big\n"
                        "  --cpu-speed N          the 68000 N times as fast as the board's: the game is never short of time\n"
            "  --movie FILE           replay an input movie\n"
            "  --record FILE          record an input movie\n"
            "  --auto SEED[:FROM[:N]] made-up input (from frame FROM; for N players, 2 unless said)\n"
            "  --test-switch FROM:TO  the operator's test switch held from frame FROM to TO\n"
            "  --record-entries FILE  add the places reached by computed jumps to FILE (for tools/recomp.py)\n"
            "  --state-save F:FILE  --state-load F:FILE   at frame F\n"
            "  --features A,B         switch optional features on (--list-features lists them; -NAME: off)\n"
            "  --settings FILE        settings.ini with the player's feature choices\n"
            "  --poke A=B,...         write byte B at RAM address A after every frame (hex)\n"
            "  --dump-ram F:FILE      write work RAM after frame F (and FILE.video, .objects, .regs)\n"
            "  --check-rewrites       run every rewritten routine and its original, and report differences\n"
            "  --no-rewrites          run the game's own routines where the sources have rewrites\n"
            "  --trace LOW-HIGH:FILE  those bytes of work RAM after every frame, appended to FILE\n"
            "  --watch LOW[-HIGH]     report the 68000's writes to these addresses (hex), with who made them\n"
            "  --keys TICK:KEY,...    keys pressed at those ticks of the screen: p (stop/go), o (a frame on),\n"
            "                         - and = (speed), k1.. (the game's command keys), cNAME (a command),\n"
            "                         fNAME=N (a feature set)\n"
            "  --stats                speed, at the end\n"
            "  --audio-dump FILE      write the sound to a WAV file\n"
            "  --dump-program FILE    write the deciphered 68000 program and stop\n",
            GAME_NAME, GAME_ROM_SET);
}

bool ParseArgs(int argc, char **argv, port::Options &o) {
    for (int i = 1; i < argc; i++) {
        const std::string a = argv[i];
        auto next = [&]() -> const char * { return i + 1 < argc ? argv[++i] : ""; };
        if (a == "--rom") {
            o.romPath = next();
        } else if (a == "--eeprom") {
            o.eepromPath = next();
        } else if (a == "--cpu") {
            o.cpu = next();
        } else if (a == "--sound-test") {
            const std::string v = next();
            o.soundTest = strtoull(v.c_str(), nullptr, 10);
            if (const size_t colon = v.find(':'); colon != std::string::npos) {
                o.soundTestEvery = std::max(1u, static_cast<unsigned>(strtoul(v.c_str() + colon + 1, nullptr, 10)));
            }
        } else if (a == "--sound") {
            o.sound = next();
        } else if (a == "--scale") {
            o.scale = atoi(next());
        } else if (a == "--fullscreen") {
            o.fullscreen = true;
        } else if (a == "--mute") {
            o.mute = true;
        } else if (a == "--unthrottled") {
            o.unthrottled = true;
        } else if (a == "--headless") {
            o.headless = true;
        } else if (a == "--frames") {
            o.frames = strtoull(next(), nullptr, 10);
        } else if (a == "--shot-every") {
            const std::string v = next();
            const size_t colon = v.find(':');
            o.shotEvery = strtoull(v.c_str(), nullptr, 10);
            o.shotPrefix = colon == std::string::npos ? "shot_" : v.substr(colon + 1);
        } else if (a == "--hash-log") {
            o.hashLog = next();
        } else if (a == "--hash-game") {
            o.hashGame = true;
        } else if (a == "--hash-picture") {
            o.hashPicture = true;
        } else if (a == "--renderer") {
            o.renderer = next();
        } else if (a == "--present-test") {
            o.presentTest = next();
        } else if (a == "--present-size") {
            const std::string v = next();
            o.presentWidth = atoi(v.c_str());
            o.presentHeight = v.find('x') == std::string::npos ? 0 : atoi(v.c_str() + v.find('x') + 1);
        } else if (a == "--cpu-speed") {
            o.cpuSpeed = atoi(next());
        } else if (a == "--movie") {
            o.moviePlay = next();
        } else if (a == "--record") {
            o.movieRecord = next();
        } else if (a == "--auto") {
            const std::string v = next();
            const size_t colon = v.find(':');
            o.autoPlay = true;
            o.autoSeed = strtoull(v.c_str(), nullptr, 10);
            o.autoFrom = colon == std::string::npos ? 0 : strtoull(v.c_str() + colon + 1, nullptr, 10);
            const size_t second = colon == std::string::npos ? colon : v.find(':', colon + 1);
            o.autoPlayers = second == std::string::npos ? 2 : atoi(v.c_str() + second + 1);
        } else if (a == "--test-switch") {
            const std::string v = next();
            const size_t colon = v.find(':');
            o.testFrom = strtoull(v.c_str(), nullptr, 10);
            o.testTo = colon == std::string::npos ? ~0ull : strtoull(v.c_str() + colon + 1, nullptr, 10);
        } else if (a == "--record-entries") {
            o.recordEntries = next();
        } else if (a == "--state-save") {
            o.stateSave = next();
        } else if (a == "--state-load") {
            o.stateLoad = next();
        } else if (a == "--stats") {
            o.stats = true;
        } else if (a == "--list-features") {
            // For the launcher: one optional feature per line (recomp-common/settings.h; the ones that last a
            // session, switched in the game's menu only, are not in it)
            RegisterGame();
            port::RegisterPortFeatures();
            std::vector<recomp::FeatureInfo> list = FeatureList();
            // (nor are the ones whose switch is on the launcher's first page: game.h, kFeatureFirstPage)
            list.erase(std::remove_if(list.begin(), list.end(), [](const recomp::FeatureInfo &f) { return f.flags & kFeatureFirstPage; }), list.end());
            fputs(recomp::FeatureListText(list).c_str(), stdout);
            exit(0);
        } else if (a == "--settings") {
            o.settingsPath = next();
        } else if (a == "--features") {
            o.features = next();
        } else if (a == "--watch") {
            // LOW[-HIGH] (hex): who writes there; needs the interpreter, so the run is done block by block
            const std::string v = next();
            const size_t dash = v.find('-');
            o.watchLow = static_cast<uint32_t>(strtoul(v.c_str(), nullptr, 16));
            o.watchHigh = dash == std::string::npos ? o.watchLow : static_cast<uint32_t>(strtoul(v.c_str() + dash + 1, nullptr, 16));
            o.cpu = "blocks";
        } else if (a == "--check-rewrites") {
            o.checkRewrites = true;
        } else if (a == "--no-rewrites") {
            o.noRewrites = true;
        } else if (a == "--trace") {
            o.trace = next();
        } else if (a == "--dump-ram") {
            o.dumpRam = next();
        } else if (a == "--poke") {
            o.poke = next();
        } else if (a == "--keys") {
            o.keys = next();
        } else if (a == "--menu") {
            o.menuTest = next();
        } else if (a == "--audio-dump") {
            o.audioDump = next();
        } else if (a == "--dump-program") {
            o.dumpProgram = next();
        } else {
            Usage();
            return false;
        }
    }
    return true;
}

// The game's program opened by itself, with nothing to say what to play (a double click on it rather than on
// the launcher): the launcher beside it is started instead, straight into the game if the player has chosen
// their ROM set in it before (its --play), or at its first page to choose one. True: it was started.
bool StartLauncher() {
    const char *base = SDL_GetBasePath();
    if (!base) {
        return false;
    }
#ifdef _WIN32
    const std::string launcher = std::string(base) + GAME_ID "-launcher.exe";
#else
    const std::string launcher = std::string(base) + GAME_ID "-launcher";
#endif
    std::error_code missing;
    if (!std::filesystem::exists(launcher, missing)) {
        return false;
    }
    bool setUp = false;
    if (char *folder = SDL_GetPrefPath("", GAME_NAME)) { // (the launcher's own folder and file)
        recomp::Ini prefs(std::filesystem::path(folder) / "launcher.ini");
        SDL_free(folder);
        prefs.load();
        setUp = !prefs.get("game", "rom").empty();
    }
    const char *args[] = {launcher.c_str(), setUp ? "--play" : nullptr, nullptr};
    SDL_Process *process = SDL_CreateProcess(args, false);
    if (!process) {
        return false;
    }
    SDL_DestroyProcess(process); // (only SDL's record of it: the launcher goes on)
    return true;
}

// A start that went wrong: said on the standard error, and, with a window wanted, in a message box too (a
// windowed program started by a double click has nowhere else to say it)
void SayFailed(const port::Options &opt, const std::string &error) {
    fprintf(stderr, "%s: %s\n", GAME_NAME, error.c_str());
    if (!opt.headless) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, GAME_NAME, error.c_str(), nullptr);
    }
}

} // namespace

int main(int argc, char **argv) {
#ifdef _WIN32
    // (a windowed program has no console: started from one, its output goes there, unless it was sent elsewhere)
    if (!GetStdHandle(STD_ERROR_HANDLE) && AttachConsole(ATTACH_PARENT_PROCESS)) {
        freopen("CONOUT$", "w", stdout);
        freopen("CONOUT$", "w", stderr);
    }
#endif
    if (argc == 1 && StartLauncher()) {
        return 0;
    }
    port::Port p;
    if (!ParseArgs(argc, argv, p.opt)) {
        return 2;
    }
    std::string error;
    if (!p.Init(error)) {
        SayFailed(p.opt, argc == 1 ? error + "\n\nStart the game with its launcher (" GAME_ID "-launcher), which passes it your ROM set." : error);
        return 1;
    }
    if (!p.opt.dumpProgram.empty()) {
        FILE *f = fopen(p.opt.dumpProgram.c_str(), "wb");
        if (!f) {
            fprintf(stderr, "cannot write %s\n", p.opt.dumpProgram.c_str());
            return 1;
        }
        fwrite(p.roms.opcodes.data(), 1, p.roms.opcodes.size(), f);
        fclose(f);
        // (and beside it the sound driver's program: the fixed part of its ROM)
        if (FILE *sound = fopen((p.opt.dumpProgram + ".z80").c_str(), "wb")) {
            fwrite(p.roms.soundProgram.data(), 1, std::min<size_t>(p.roms.soundProgram.size(), 0x8000), sound);
            fclose(sound);
        }
        // (and the program as data: what the game's tables are read from)
        if (FILE *data = fopen((p.opt.dumpProgram + ".data").c_str(), "wb")) {
            fwrite(p.roms.program.data(), 1, p.roms.program.size(), data);
            fclose(data);
        }
        return 0;
    }
    int result = 0;
    const auto begin = std::chrono::steady_clock::now();
    if (p.opt.headless) {
        std::vector<int16_t> sound;
        while (!p.quit && (p.opt.frames == 0 || p.frame < p.opt.frames)) {
            if (!p.Tick()) {
                continue;
            }
            if (!p.opt.audioDump.empty()) {
                sound.insert(sound.end(), p.machine->audio.begin(), p.machine->audio.end());
            }
            p.machine->audio.clear();
        }
        if (!p.opt.presentTest.empty()) {
            // (the picture the run ended on, through the GPU path and read back: what a window would show)
            std::vector<uint32_t> shown, drawn;
            std::string error;
            p.Shown(shown);
            // (in a window --present-size big, or one the picture's shape at 3 times its lines)
            const int across = p.ShownWidth();
            const int outHeight = p.opt.presentHeight > 0 ? p.opt.presentHeight : 672;
            const int outWidth = p.opt.presentWidth > 0 ? p.opt.presentWidth
                                                        : static_cast<int>(std::lround(outHeight * port::PictureShape(across, Video::kHeight, port::DisplayShape(), 4.0f / 3.0f)));
            const SDL_FRect where = port::FitPicture(outWidth, outHeight, across, Video::kHeight, port::DisplayShape(), port::DisplayScaling());
            if (SDL_Init(SDL_INIT_VIDEO) &&
                port::Presenter::Render(shown.data(), across, Video::kHeight, outWidth, outHeight, where, port::DisplayScaling(), drawn, error)) {
                WritePng(p.opt.presentTest.c_str(), drawn.data(), outWidth, outHeight);
            } else {
                fprintf(stderr, "--present-test: %s\n", error.empty() ? SDL_GetError() : error.c_str());
                result = 1;
            }
        }
        if (!p.opt.audioDump.empty()) {
            if (FILE *f = fopen(p.opt.audioDump.c_str(), "wb")) {
                const uint32_t bytes = static_cast<uint32_t>(sound.size() * 2), rate = QSound::kSampleRate;
                const uint32_t header[11] = {0x46464952, 36 + bytes, 0x45564157, 0x20746D66, 16,      0x00020001,
                                             rate,       rate * 4,   0x00100004, 0x61746164, bytes};
                fwrite(header, 4, 11, f);
                fwrite(sound.data(), 2, sound.size(), f);
                fclose(f);
            }
        }
    } else {
        result = port::RunWindowed(p);
    }
    if (p.opt.stats) {
        const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - begin).count();
        fprintf(stderr, "%llu frames in %.2f s: %.0f a second (cpu: %s)\n", static_cast<unsigned long long>(p.frame), seconds,
                p.frame / seconds, p.opt.cpu.c_str());
        if (p.opt.cpu == "native") {
            fprintf(stderr, "sound driver: %llu instructions had no translation\n",
                    static_cast<unsigned long long>(p.machine->soundCpu.interpreted));
            // (about 12 cycles to an instruction)
            fprintf(stderr, "%llu instructions had no translation and ran in the interpreter (about %.3f%% of the run)\n",
                    static_cast<unsigned long long>(p.machine->cpu.interpreted),
                    100.0 * p.machine->cpu.interpreted * 12.0 / std::max<uint64_t>(1, p.machine->cpu.cycles));
        }
        if (p.machine->soundSide == Machine::SoundSide::Check) {
            fprintf(stderr, "%llu writes to the sound chip the same from the driver and the Z80\n",
                    static_cast<unsigned long long>(p.machine->soundWritesChecked));
            p.machine->ReportSoundTimes();
        }
#ifdef CPS2RECOMP_CHECK
        fprintf(stderr, "%llu translated blocks checked against the interpreter, and %llu of the sound driver's\n",
                static_cast<unsigned long long>(BlocksChecked()), static_cast<unsigned long long>(Z80BlocksChecked()));
#endif
    }
    p.Finish();
    if (p.opt.checkRewrites && !ReportRewrites()) {
        return 4;
    }
    return result;
}
