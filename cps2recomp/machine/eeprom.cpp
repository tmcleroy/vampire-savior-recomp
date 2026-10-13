// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of cps2recomp.

#include "eeprom.h"

#include "../runtime/state.h"

namespace cps2recomp {

Eeprom::Eeprom() {
    for (auto &word : words) {
        word = 0xFFFF; // (erased)
    }
}

void Eeprom::SetLines(bool select, bool clock, bool dataIn) {
    if (!select) {
        // Deselecting ends a command
        m_select = false;
        m_clock = clock;
        m_started = false;
        m_count = 0;
        m_shift = 0;
        m_reading = -1;
        m_writing = 0;
        m_out = true; // (ready)
        return;
    }
    const bool rising = clock && !m_clock;
    m_select = true;
    m_clock = clock;
    if (!rising) {
        return;
    }
    if (m_reading >= 0) {
        // A read sends its sixteen bits, high bit first, and goes on to the next word
        if (m_reading == 0) {
            m_address = (m_address + 1) & 63;
            m_sending = words[m_address];
            m_reading = 16;
        }
        m_reading--;
        m_out = (m_sending >> m_reading) & 1;
        return;
    }
    if (!m_started) {
        m_started = dataIn;
        return;
    }
    m_shift = (m_shift << 1) | (dataIn ? 1 : 0);
    m_count++;
    if (m_writing) {
        if (m_count == 16) {
            if (m_writable) {
                if (m_writing == 2) {
                    for (auto &word : words) {
                        word = static_cast<uint16_t>(m_shift);
                    }
                } else {
                    words[m_address] = static_cast<uint16_t>(m_shift);
                }
                changed = true;
            }
            m_writing = 0;
            m_started = false;
            m_count = 0;
            m_shift = 0;
        }
        return;
    }
    if (m_count < 8) {
        return;
    }
    // Two bits of command and six of address
    const unsigned command = (m_shift >> 6) & 3;
    m_address = m_shift & 63;
    m_count = 0;
    m_shift = 0;
    switch (command) {
    case 2: // read (a zero bit comes first)
        m_sending = words[m_address];
        m_reading = 16;
        m_out = false;
        break;
    case 1: m_writing = 1; break;
    case 3: // erase
        if (m_writable) {
            words[m_address] = 0xFFFF;
            changed = true;
        }
        m_started = false;
        break;
    default:
        switch (m_address >> 4) {
        case 0: m_writable = false; break;
        case 3: m_writable = true; break;
        case 1: m_writing = 2; break;
        default: // erase all
            if (m_writable) {
                for (auto &word : words) {
                    word = 0xFFFF;
                }
                changed = true;
            }
            break;
        }
        if (!m_writing) {
            m_started = false;
        }
        break;
    }
}

void Eeprom::Serialize(Archive &ar) {
    ar.Mark(0x45455052);
    ar.Pod(words);
    ar.Pod(m_select);
    ar.Pod(m_clock);
    ar.Pod(m_out);
    ar.Pod(m_writable);
    ar.Pod(m_started);
    ar.Pod(m_shift);
    ar.Pod(m_count);
    ar.Pod(m_reading);
    ar.Pod(m_sending);
    ar.Pod(m_address);
    ar.Pod(m_writing);
}

} // namespace cps2recomp
