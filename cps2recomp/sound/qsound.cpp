// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of cps2recomp.

#include "qsound.h"

#include "../runtime/state.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace cps2recomp {

namespace {
    // Addresses in the DSP's program that the sound driver writes to the next-state register
    enum : uint16_t { kStateInit1 = 0x288, kStateInit2 = 0x61A };
    // The echo length register holds an address in the DSP's memory; the delay line starts here
    enum : uint16_t { kDelayBase1 = 0x554, kDelayBase2 = 0x53C };
    constexpr uint16_t kPanBase = 0x110; // the pan register is an address too: 33 places, 0x110 to 0x130

    // A voice's share of the left output by place (0: all left ... 32: all right), 0x4000 = all of it.
    // The chip has this as a table in its ROM; this is an equal-power curve.
    struct PanTable {
        int16_t left[33];
        PanTable() {
            for (int n = 0; n <= 32; n++) {
                left[n] = static_cast<int16_t>(std::lround(16384.0 * std::sqrt((32 - n) / 32.0)));
            }
        }
    };
    const PanTable kPan;
} // namespace

void QSound::Reset() {
    m_ready = 0;
    m_dataLatch = 0;
    m_nextState = 0;
    m_counter = 0;
    Init(false);
}

// What the DSP's start-up routine does: everything silent, voices in the middle
void QSound::Init(bool secondMode) {
    for (auto &voice : m_voice) {
        voice = Voice();
    }
    for (auto &pan : m_pan) {
        pan = kPanBase + 0x10;
    }
    m_echo = Echo();
    m_echo.endPos = static_cast<uint16_t>((secondMode ? kDelayBase2 : kDelayBase1) + 6);
    m_secondMode = secondMode;
    m_ready = 0;
    m_busy = 4;
    m_nextState = 0;
}

void QSound::Write(unsigned port, uint8_t value) {
    switch (port) {
    case 0: m_dataLatch = static_cast<uint16_t>((m_dataLatch & 0x00FF) | (value << 8)); break;
    case 1: m_dataLatch = static_cast<uint16_t>((m_dataLatch & 0xFF00) | value); break;
    case 2:
        WriteRegister(value, m_dataLatch);
        m_ready = 0; // (until the program has come round again)
        break;
    default: break;
    }
}

void QSound::WriteRegister(uint8_t address, uint16_t data) {
    static const bool log = getenv("CPS2RECOMP_QSOUND_LOG") != nullptr; // (every register write, to stderr)
    if (log) {
        fprintf(stderr, "qs %02X %04X\n", address, data);
    }
    if (address < 0x80) {
        const unsigned n = address >> 3;
        Voice &voice = m_voice[n];
        switch (address & 7) {
        case 0: m_voice[(n + 1) & 15].bank = data; break; // (the bank register is the next voice's)
        case 1: voice.addr = static_cast<int16_t>(data); break;
        case 2: voice.rate = data; break;
        case 3: voice.phase = data; break;
        case 4: voice.loopLen = static_cast<int16_t>(data); break;
        case 5: voice.endAddr = static_cast<int16_t>(data); break;
        case 6: voice.volume = static_cast<int16_t>(data); break;
        default: break;
        }
    } else if (address < 0x90) {
        m_pan[address - 0x80] = data;
    } else if (address == 0x93) {
        m_echo.feedback = static_cast<int16_t>(data);
    } else if (address >= 0xBA && address < 0xCA) {
        m_voice[address - 0xBA].echo = static_cast<int16_t>(data);
    } else if (address == 0xD9) {
        m_echo.endPos = data;
    } else if (address == 0xE3) {
        m_nextState = data;
    }
}

void QSound::Step(int16_t out[2]) {
    if (m_busy) {
        m_busy--;
        out[0] = out[1] = 0;
        return;
    }
    m_ready = 0x80;

    m_echo.length = static_cast<int16_t>(
        std::clamp<int>(static_cast<int>(m_echo.endPos) - (m_secondMode ? kDelayBase2 : kDelayBase1), 0, 1024));

    // The voices: the sample at the voice's place times its volume; then the place moves on by the rate
    // (a 4.12 fixed-point step) and goes back by the loop length when it reaches the end
    int32_t voiceOut[16];
    int32_t echoIn = 0;
    for (int n = 0; n < 16; n++) {
        Voice &voice = m_voice[n];
        const uint32_t at = (static_cast<uint32_t>(voice.bank & 0x7FFF) << 16) | static_cast<uint16_t>(voice.addr);
        const int16_t sample = static_cast<int16_t>((at < m_romSize ? m_rom[at] : 0) << 8);
        const int16_t output = static_cast<int16_t>((voice.volume * sample) >> 14);
        echoIn += (output * voice.echo) << 2;
        int32_t place = voice.rate + ((static_cast<int32_t>(voice.addr) << 12) | (voice.phase >> 4));
        if ((place >> 12) >= voice.endAddr) {
            place -= static_cast<int32_t>(voice.loopLen) << 12;
        }
        place = std::clamp<int32_t>(place, -0x8000000, 0x7FFFFFF);
        voice.addr = static_cast<int16_t>(place >> 12);
        voice.phase = static_cast<uint16_t>((place << 4) & 0xFFFF);
        voiceOut[n] = output;
    }

    // The echo: a delay line fed back on itself, its output smoothed over two samples
    int32_t echoOut = 0;
    if (m_echo.length > 0) {
        int32_t old = m_echo.delayLine[m_echo.delayPos];
        const int32_t last = m_echo.lastSample;
        m_echo.lastSample = static_cast<int16_t>(old);
        old = (old + last) >> 1;
        const int32_t fresh = echoIn + ((old * m_echo.feedback) << 2);
        m_echo.delayLine[m_echo.delayPos++] = static_cast<int16_t>(fresh >> 16);
        if (m_echo.delayPos >= m_echo.length) {
            m_echo.delayPos = 0;
        }
        echoOut = old;
    }

    for (int ch = 0; ch < 2; ch++) {
        int32_t sum = echoOut << 14;
        for (int n = 0; n < 16; n++) {
            const int place = std::clamp<int>(static_cast<int>(m_pan[n]) - kPanBase, 0, 32);
            sum += voiceOut[n] * kPan.left[ch == 0 ? place : 32 - place];
        }
        sum = std::clamp<int32_t>(sum, -0x1FFFFFFF, 0x1FFFFFFF);
        out[ch] = static_cast<int16_t>(std::clamp<int32_t>((sum + 0x1000) >> 13, -0x7FFF, 0x7FFF));
    }

    if (++m_counter > 5) {
        m_counter = 0;
        if (m_nextState == kStateInit1 || m_nextState == kStateInit2) {
            Init(m_nextState == kStateInit2);
        }
    }
}

void QSound::Serialize(Archive &ar) {
    ar.Mark(0x51534E44);
    ar.Pod(m_voice);
    ar.Pod(m_pan);
    ar.Pod(m_echo);
    ar.Pod(m_dataLatch);
    ar.Pod(m_nextState);
    ar.Pod(m_ready);
    ar.Pod(m_busy);
    ar.Pod(m_counter);
    ar.Pod(m_secondMode);
}

} // namespace cps2recomp
