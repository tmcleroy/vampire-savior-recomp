// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of cps2recomp.

#include "machine.h"

#include "../runtime/state.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace cps2recomp {

namespace {
    constexpr uint64_t kSoundInterruptPeriod = 32000; // Z80 cycles: 250 a second
    constexpr uint64_t kSoundClock = 8000000;

    inline uint16_t Word(const uint8_t *p) {
        return static_cast<uint16_t>((p[0] << 8) | p[1]);
    }
    inline void SetWord(uint8_t *p, uint16_t value) {
        p[0] = static_cast<uint8_t>(value >> 8);
        p[1] = static_cast<uint8_t>(value);
    }
    void Unmapped(const char *what, uint32_t address, uint32_t pc) {
        static int reports = getenv("CPS2RECOMP_UNMAPPED") ? 200 : 0;
        if (reports > 0) {
            reports--;
            fprintf(stderr, "%s of %06X (pc %06X): nothing there\n", what, address, pc);
        }
    }
} // namespace

Machine::Machine(const RomSet &roms)
    : cpu(*this)
    , soundCpu(*this)
    , qsound(roms.samples.data(), roms.samples.size())
    , video(Video::Sources{})
    , ram(0x10000)
    , videoRam(0x30000)
    , objectRam(0x4000)
    , extraRam(0x4000)
    , m_roms(roms)
    , m_picture(static_cast<size_t>(Video::kWidth) * Video::kHeight) {
    Video::Sources sources;
    sources.graphics = roms.graphics.data();
    sources.graphicsSize = roms.graphics.size();
    sources.videoRam = videoRam.data();
    sources.cpsA = cpsA;
    sources.cpsB = cpsB;
    sources.objectRegs = objectRegs;
    video = Video(sources);
    if (const char *layers = getenv("CPS2RECOMP_LAYERS")) {
        video.layerMask = static_cast<unsigned>(strtoul(layers, nullptr, 0)); // (bit 0 objects, 1-3 scroll screens)
    }
    cpu.ram = ram.data();
    cpu.rom = roms.program.data();
    cpu.opcodes = roms.opcodes.data();
    cpu.romSize = static_cast<uint32_t>(roms.program.size());
    Reset();
}

void Machine::Reset() {
    std::fill(ram.begin(), ram.end(), 0);
    std::fill(videoRam.begin(), videoRam.end(), 0);
    std::fill(objectRam.begin(), objectRam.end(), 0);
    std::fill(extraRam.begin(), extraRam.end(), 0);
    memset(soundRam, 0, sizeof(soundRam));
    memset(soundRam2, 0, sizeof(soundRam2));
    memset(cpsA, 0, sizeof(cpsA));
    memset(cpsB, 0, sizeof(cpsB));
    memset(objectRegs, 0, sizeof(objectRegs));
    // (where the video chip's memory areas are until the game says)
    cpsA[0] = 0x9200;
    cpsA[1] = 0x9000;
    cpsA[2] = 0x9040;
    cpsA[3] = 0x9080;
    cpsA[4] = 0x9100;
    m_line = 0;
    m_lineStart = 0;
    cpu.cycles = 0;
    m_blankInterrupt = m_rasterInterrupt = false;
    m_rasterCounter[0] = m_rasterCounter[1] = m_rasterCounter[2] = 0;
    m_rasterReload[0] = m_rasterReload[1] = 0x1FF;
    m_rasterReload[2] = 0;
    m_objectBank = 0;
    m_soundRunning = false;
    m_soundBank = 0;
    m_soundTime = 0;
    m_soundInterruptAt = kSoundInterruptPeriod;
    m_sampleCount = 0;
    m_sampleAt = kSoundClock / QSound::kSampleRate;
    soundCpu.cycles = 0;
    frame = 0;
    audio.clear();
    video.Reset();
    qsound.Reset();
    soundCpu.Reset();
    cpu.Reset(true);
}

// --- time ---

void Machine::UpdateInterruptLevel() {
    // The two interrupt sources drive two of the CPU's three level lines: together they read as level 6
    cpu.SetInterruptLevel(static_cast<uint8_t>((m_blankInterrupt ? 2 : 0) | (m_rasterInterrupt ? 4 : 0)));
}

void Machine::InterruptAck(unsigned) {
    m_blankInterrupt = m_rasterInterrupt = false;
    UpdateInterruptLevel();
}

void Machine::RunFrame() {
    do {
        RunLine();
    } while (m_line != kBlankingLine);
    frame++;
}

void Machine::RunLine() {
    // The video chip's two line counters count down each line (and start again at the top of the frame);
    // one reaching zero asks for a raster interrupt, which comes as far into the line as the third says
    bool raster = false;
    for (int n = 0; n < 2; n++) {
        if (m_line == 0) {
            m_rasterCounter[n] = m_rasterReload[n];
        } else {
            m_rasterCounter[n] = (m_rasterCounter[n] - 1) & 0x1FF;
        }
        raster = raster || m_rasterCounter[n] == 0;
    }
    m_rasterCounter[2] = m_rasterReload[2];

    if (m_line == kBlankingLine) {
        m_blankInterrupt = true;
        UpdateInterruptLevel();
        video.LatchObjects(&objectRam[m_objectBank ? 0x2000 : 0]);
    }

    // (the CPU stops a little after the time it is given, and the next line is that much shorter for it)
    const uint64_t start = m_lineStart;
    m_lineStart += (kCpuCyclesPerLine * cpuSpeed);
    if (raster) {
        cpu.Run(start + static_cast<uint64_t>(m_rasterCounter[2]) * 4, cpuMode);
        m_rasterInterrupt = true;
        UpdateInterruptLevel();
    }
    cpu.Run(m_lineStart, cpuMode);

    RunSound(kSoundCyclesPerLine);

    if (m_line >= Video::kFirstLine && m_line < Video::kFirstLine + Video::kHeight) {
        const int row = m_line - Video::kFirstLine;
        video.RenderLine(row, &m_picture[static_cast<size_t>(row) * Video::kWidth]);
    }
    m_line = (m_line + 1) % kLines;
}

// The Z80 and the QSound chip for a stretch of time. The Z80's cycle count is the sound side's clock: it
// runs to the next thing due (its interrupt, 250 times a second; the chip's next sample; the end of the
// stretch), a little past it, and then what has come due happens.
void Machine::RunSound(int cycles) {
    m_soundTime += static_cast<uint64_t>(cycles);
    while (soundCpu.cycles < m_soundTime) {
        const uint64_t next = std::min({m_soundTime, m_soundInterruptAt, m_sampleAt});
        if (m_soundRunning) {
            soundCpu.Run(next, soundMode);
        }
        soundCpu.cycles = std::max(soundCpu.cycles, next); // (held in reset or halted: time passes all the same)
        while (m_soundInterruptAt <= soundCpu.cycles) {
            m_soundInterruptAt += kSoundInterruptPeriod;
            if (m_soundRunning) {
                soundCpu.RaiseInterrupt();
            }
        }
        while (m_sampleAt <= soundCpu.cycles) {
            int16_t sample[2];
            qsound.Step(sample);
            audio.push_back(sample[0]);
            audio.push_back(sample[1]);
            m_sampleCount++;
            m_sampleAt = (m_sampleCount + 1) * kSoundClock / QSound::kSampleRate;
        }
    }
}

// --- the 68000's bus ---

uint16_t Machine::Fetch16(uint32_t address) {
    if (address < m_roms.opcodes.size()) {
        return Word(&m_roms.opcodes[address]);
    }
    return Read16(address); // (code in RAM is not enciphered)
}

// 68000 cycles into the line
int Machine::LineCycles() const {
    return static_cast<int>(static_cast<int64_t>(cpu.cycles) - static_cast<int64_t>(m_lineStart - (kCpuCyclesPerLine * cpuSpeed)));
}

uint16_t Machine::Read16(uint32_t address) {
    if (address >= 0xFF0000) {
        return Word(&ram[address & 0xFFFE]);
    }
    if (address < 0x400000) {
        return Word(&m_roms.program[address]);
    }
    if (address >= 0x900000 && address < 0x930000) {
        return Word(&videoRam[address - 0x900000]);
    }
    return ReadRegister(address);
}

uint8_t Machine::Read8(uint32_t address) {
    if (address >= 0xFF0000) {
        return ram[address & 0xFFFF];
    }
    if (address < 0x400000) {
        return m_roms.program[address];
    }
    if (address >= 0x900000 && address < 0x930000) {
        return videoRam[address - 0x900000];
    }
    const uint16_t word = ReadRegister(address & ~1u);
    return static_cast<uint8_t>((address & 1) ? word : word >> 8);
}

void Machine::Write16(uint32_t address, uint16_t value) {
    if (address >= 0xFF0000) {
        SetWord(&ram[address & 0xFFFE], value);
        return;
    }
    if (address >= 0x900000 && address < 0x930000) {
        SetWord(&videoRam[address - 0x900000], value);
        return;
    }
    WriteRegister(address, value, 0xFFFF);
}

void Machine::Write8(uint32_t address, uint8_t value) {
    if (address >= 0xFF0000) {
        ram[address & 0xFFFF] = value;
        return;
    }
    if (address >= 0x900000 && address < 0x930000) {
        videoRam[address - 0x900000] = value;
        return;
    }
    if (address & 1) {
        WriteRegister(address & ~1u, value, 0x00FF);
    } else {
        WriteRegister(address, static_cast<uint16_t>(value << 8), 0xFF00);
    }
}

// The inputs are wired so that a line reads 0 while pressed
uint16_t Machine::Input(int port) const {
    const uint16_t p1 = players[0], p2 = players[1];
    uint16_t pressed = 0;
    switch (port) {
    case 0: // sticks and punches
        pressed = static_cast<uint16_t>((p1 & 0x7F) | ((p2 & 0x7F) << 8));
        break;
    case 1: // kicks (player 2's hard kick is on the next port)
        pressed = static_cast<uint16_t>(((p1 >> 7) & 7) | (((p2 >> 7) & 3) << 4));
        break;
    default:
        pressed = static_cast<uint16_t>((testSwitch ? 0x0002 : 0) | (serviceButton ? 0x0004 : 0) | ((p1 & kStart) ? 0x0100 : 0) |
                                        ((p2 & kStart) ? 0x0200 : 0) | ((p1 & kCoin) ? 0x1000 : 0) |
                                        ((p2 & kCoin) ? 0x2000 : 0) | ((p2 & kKick3) ? 0x4000 : 0));
        return static_cast<uint16_t>((~pressed & 0xFFFE) | (eeprom.DataOut() ? 1 : 0));
    }
    return static_cast<uint16_t>(~pressed);
}

uint16_t Machine::ReadRegister(uint32_t address) {
    if (address >= 0x400000 && address < 0x40000C) {
        return objectRegs[(address - 0x400000) / 2];
    }
    if (address >= 0x618000 && address < 0x61A000) {
        return static_cast<uint16_t>(0xFF00 | soundRam[(address - 0x618000) / 2]);
    }
    if (address >= 0x660000 && address < 0x664000) {
        return Word(&extraRam[address - 0x660000]);
    }
    if (address >= 0x708000 && address < 0x710000) {
        return Word(&objectRam[(m_objectBank ^ 1) * 0x2000 + (address & 0x1FFE)]);
    }
    if ((address & ~0x4000u) >= 0x800140 && (address & ~0x4000u) < 0x800180) {
        // The CPS-B chip: a 16 x 16 multiplier, and the raster counters
        const unsigned index = (address & 0x3E) / 2;
        switch (index) {
        case 0x04 / 2: return static_cast<uint16_t>(cpsB[0] * cpsB[1]);
        case 0x06 / 2: return static_cast<uint16_t>((static_cast<uint32_t>(cpsB[0]) * cpsB[1]) >> 16);
        case 0x0E / 2:
            // (how far along the line, counted from the third counter in steps of two pixels)
            return static_cast<uint16_t>((((m_rasterCounter[2] - LineCycles() / 4) << 1) & 0x1FE) | (cpsB[0x0E / 2] & 1));
        case 0x10 / 2: return m_rasterCounter[0] & 0x1FF;
        case 0x12 / 2: return m_rasterCounter[1] & 0x1FF;
        default: return 0xFFFF;
        }
    }
    switch (address) {
    case 0x804000: return Input(0);
    case 0x804010: return Input(1);
    case 0x804020: return Input(2);
    case 0x804030: return 0xE021; // the volume setting (full); no extra memory or network board
    case 0x8040B0:
    case 0x8040B2: return 0xFFFF; // (switches only development boards have)
    default: break;
    }
    Unmapped("read", address, cpu.pc);
    return 0xFFFF;
}

void Machine::WriteRegister(uint32_t address, uint16_t value, uint16_t mask) {
    auto combine = [&](uint16_t &reg) {
        reg = static_cast<uint16_t>((reg & ~mask) | (value & mask));
        return reg;
    };
    auto combineBytes = [&](uint8_t *p) {
        if (mask & 0xFF00) {
            p[0] = static_cast<uint8_t>(value >> 8);
        }
        if (mask & 0x00FF) {
            p[1] = static_cast<uint8_t>(value);
        }
    };
    if (address >= 0x400000 && address < 0x40000C) {
        combine(objectRegs[(address - 0x400000) / 2]);
        return;
    }
    if (address >= 0x618000 && address < 0x61A000) {
        if (mask & 0x00FF) {
            soundRam[(address - 0x618000) / 2] = static_cast<uint8_t>(value);
        }
        return;
    }
    if (address >= 0x660000 && address < 0x664000) {
        combineBytes(&extraRam[address - 0x660000]);
        return;
    }
    if (address >= 0x700000 && address < 0x702000) {
        combineBytes(&objectRam[m_objectBank * 0x2000 + (address & 0x1FFE)]);
        return;
    }
    if (address >= 0x708000 && address < 0x710000) {
        combineBytes(&objectRam[(m_objectBank ^ 1) * 0x2000 + (address & 0x1FFE)]);
        return;
    }
    const uint32_t chip = address & ~0x4000u; // (the video chips answer at 8001xx as well as 8041xx)
    if (chip >= 0x800100 && chip < 0x800140) {
        const unsigned index = (address & 0x3E) / 2;
        combine(cpsA[index]);
        if (index == 0x0A / 2) {
            video.CopyPalette(); // (writing the palette's address makes the chip take the palette)
        }
        return;
    }
    if (chip >= 0x800140 && chip < 0x800180) {
        const unsigned index = (address & 0x3E) / 2;
        const uint16_t now = combine(cpsB[index]);
        if (index == 0x0E / 2) {
            m_rasterReload[2] = (now >> 1) & 0xFF;
        } else if (index == 0x10 / 2 || index == 0x12 / 2) {
            const int n = index & 1;
            m_rasterReload[n] = now & 0x1FF;
            if (now & 0x8000) {
                m_rasterCounter[n] = m_rasterReload[n];
            }
        }
        return;
    }
    switch (address) {
    case 0x804040:
        if (mask & 0xFF00) {
            eeprom.SetLines(value & 0x4000, value & 0x2000, value & 0x1000);
        }
        if (mask & 0x00FF) {
            // Bit 3 lets the Z80 run; the rest are coin counters and lock-outs
            const bool run = value & 0x0008;
            if (run && !m_soundRunning) {
                soundCpu.Reset();
            }
            m_soundRunning = run;
        }
        return;
    case 0x8040A0: return; // (written once at start-up)
    case 0x8040E0:
        if (mask & 0x00FF) {
            m_objectBank = value & 1;
        }
        return;
    case 0x664000: return;
    default: break;
    }
    Unmapped("write", address, cpu.pc);
}

// --- the Z80's bus ---

uint8_t Machine::Read(uint16_t address) {
    if (address < 0x8000) {
        return m_roms.soundProgram[address];
    }
    if (address < 0xC000) {
        const size_t at = 0x8000 + static_cast<size_t>(m_soundBank) * 0x4000 + (address - 0x8000);
        return at < m_roms.soundProgram.size() ? m_roms.soundProgram[at] : 0xFF;
    }
    if (address < 0xD000) {
        return soundRam[address - 0xC000];
    }
    if (address == 0xD007) {
        return qsound.Read();
    }
    if (address >= 0xF000) {
        return soundRam2[address - 0xF000];
    }
    return 0xFF;
}

void Machine::Write(uint16_t address, uint8_t value) {
    if (address >= 0xC000 && address < 0xD000) {
        soundRam[address - 0xC000] = value;
    } else if (address >= 0xD000 && address <= 0xD002) {
        qsound.Write(address - 0xD000, value);
    } else if (address == 0xD003) {
        m_soundBank = value & 0x0F;
    } else if (address >= 0xF000) {
        soundRam2[address - 0xF000] = value;
    }
}

void Machine::Serialize(Archive &ar) {
    ar.Mark(0x4D414348);
    cpu.Serialize(ar);
    soundCpu.Serialize(ar);
    qsound.Serialize(ar);
    video.Serialize(ar);
    eeprom.Serialize(ar);
    ar.Vec(ram);
    ar.Vec(videoRam);
    ar.Vec(objectRam);
    ar.Vec(extraRam);
    ar.Pod(soundRam);
    ar.Pod(soundRam2);
    ar.Pod(cpsA);
    ar.Pod(cpsB);
    ar.Pod(objectRegs);
    ar.Pod(frame);
    ar.Pod(m_line);
    ar.Pod(m_lineStart);
    ar.Pod(m_blankInterrupt);
    ar.Pod(m_rasterInterrupt);
    ar.Pod(m_rasterCounter);
    ar.Pod(m_rasterReload);
    ar.Pod(m_objectBank);
    ar.Pod(m_soundRunning);
    ar.Pod(m_soundBank);
    ar.Pod(m_soundTime);
    ar.Pod(m_soundInterruptAt);
    ar.Pod(m_sampleCount);
    ar.Pod(m_sampleAt);
}

} // namespace cps2recomp
