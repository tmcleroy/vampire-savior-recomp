// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of cps2recomp.
#include "driver.h"

#include "../runtime/state.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace cps2recomp {

namespace {
    // Where things are in the program ROM of this version
    enum : uint32_t {
        kVersionText = 0x0016,
        kChipSetupRegisters = 0x00D5, // twelve registers and the values they start with
        kChipSetupValues = 0x00E1,
        kPitchOctaves = 0x2183,     // for each of 108 semitones: its octave
        kPitchOctaveStarts = 0x21EF, // ... the octave's first semitone
        kPitchRates = 0x21F8,       // ... the chip's rate at the semitone
        kPitchFractions = 0x23A8,   // for each of 12 semitones, 256 steps to the next (for the lowest octave)
        kVolumes = 0x3685,          // tables of 128 words, by the numbers sequences use
        kLevels = 0x3785,
        kLoudness = 0x3885,
        kAttackRates = 0x3985,
        kDecayRates = 0x3A05,
        kLfoRates = 0x3A85,
        kVibratoDepths = 0x3B85,
        kTremoloDepths = 0x3C85,
        kFineTunes = 0x3D85,
        kSongTablePointer = 0x3FE0,
        kSampleTablePointer = 0x3FE2,
        kInstrumentTables = 0x3FE4,
    };
    // The chip's registers
    enum : unsigned {
        kVoiceBank = 0, kVoiceStart = 1, kVoiceRate = 2, kVoicePhase = 3, kVoiceLoop = 4, kVoiceEnd = 5, kVoiceVolume = 6,
        kPanRegisters = 0x80,
        kAdpcmPanRegisters = 0x90,
        kEchoFeedback = 0x93,
        kEchoSends = 0xBA,
        kAdpcmStart = 0xCA, kAdpcmEnd = 0xCB, kAdpcmBank = 0xCC, kAdpcmVolume = 0xCD,
        kAdpcmKeys = 0xD6,
        kEchoEnd = 0xD9,
        kNextState = 0xE3,
    };
    constexpr uint16_t kPanMiddle = 0x110; // the pan registers hold an address in the chip: place 0 is 0x110
    constexpr uint16_t kEchoShortest = 0x055A;
    // The program's time (Z80 cycles) for the things it does between writes, measured from the Z80 running
    // it: averages, good to a sample or so
    constexpr uint32_t kCommandTime = 700; // taking a command and finding its song
    constexpr uint32_t kCopyTime = 3200;   // a track's 104 bytes set up
    constexpr uint32_t kVoiceTime = 150;   // looking at a voice
    constexpr uint32_t kTrackTime = 350;   // a track's time counted
    constexpr uint32_t kEventTime = 250;   // a byte of a sequence acted on
    constexpr uint32_t kNoteTime = 700;
    constexpr uint32_t kPlaceTime = 400;   // a track's place between left and right moved on
    constexpr uint32_t kEnvelopeTime = 720; // ... its LFO, slide and envelope
    constexpr uint32_t kSampleTime = 1000; // a sample found in the table
    constexpr uint32_t kPanTime = 450;
    constexpr uint32_t kPitchTime = 780;   // a note turned into the chip's rate
    // Track::flags
    enum : uint8_t {
        kPitchChanged = 0x01,
        kShifts = 0x02,     // the pitch shift the game can ask for applies; the note restarts when a sound effect ends
        kVibrato = 0x04,
        kTremolo = 0x08,
        kTied = 0x10,       // the next note carries on from this one
        kLfoRestarts = 0x20,
        kLfoFalling = 0x40, // the LFO's triangle: second half of each half-wave
        kLfoNegative = 0x80,
    };
    enum : uint8_t { kAttack = 0, kDecay = 1, kSustain = 3, kRelease = 4, kStill = 5 };
    enum : uint8_t { kOff = 0, kSingle = 1, kRepeating = 0x80, kSequence = 0xFF };

    uint16_t Negative(uint16_t value) {
        return static_cast<uint16_t>(0 - value);
    }
    // Keeps a number sequences add to within -0x40..0x3F
    uint8_t Clamped(uint8_t value) {
        const uint8_t shifted = static_cast<uint8_t>(value + 0x40);
        if (shifted & 0x80) {
            return (shifted & 0x40) ? 0xC0 : 0x3F;
        }
        return value;
    }
    uint8_t PanPlace(uint8_t value) {
        return value > 0x20 ? 0x20 : value;
    }
    template <typename T>
    T Zeroed() {
        T value;
        memset(&value, 0, sizeof(value));
        return value;
    }
} // namespace

bool SoundDriver::Knows(const std::vector<uint8_t> &program) {
    static const char text[] = "2.01b /CPS2      1996  /APRIL";
    return program.size() >= 0x8000 && memcmp(&program[kVersionText], text, sizeof(text) - 1) == 0;
}

SoundDriver::SoundDriver(const std::vector<uint8_t> &program, uint8_t *shared, std::function<void(uint8_t, uint16_t, uint32_t)> chip)
    : m_rom(program.data())
    , m_size(program.size())
    , m_shared(shared)
    , m_chip(std::move(chip)) {
    Reset();
}

// 0000: the program says it is there, and waits for two sequence ticks before it sets the chip up
void SoundDriver::Reset() {
    m = Zeroed<State>();
    m.booting = 2;
    Share(0xFFF, 0x77);
}

// 0038: the interrupt counts out the work (of every four: commands on two, ADPCM on one, sequences on one)
// and takes a command the game has left. Then (016E) the work that is due is done.
void SoundDriver::Interrupt() {
    if (m.deaf) {
        m.deaf--;
        return;
    }
    m.ticks++;
    switch (m.ticks & 3) {
    case 0: m.sequenceDue++; break;
    case 2: m.adpcmDue++; break;
    default: m.commandsDue++; break;
    }
    if (!shadow && m_shared[0xFFD] == 0x88) {
        if (m_shared[0x00F] != 0xFF) {
            TakeCommand();
        }
        m.stereo = m_shared[0xFFE];
    }
    if (m.booting) {
        if ((m.ticks & 3) == 0 && --m.booting == 0) {
            StartUp();
        }
        return;
    }
    if (shadow) {
        return;
    }
    if (!m.started) {
        if (m_shared[0xFFF] != 0xFF) { // (the game says when)
            return;
        }
        m.started = true;
    }
    while (m.commandsDue || m.adpcmDue || m.sequenceDue) {
        m_spent = 0;
        if (m.commandsDue) {
            m.commandsDue--;
            Command();
        }
        if (m.adpcmDue) {
            m.adpcmDue--;
            AdpcmTick();
        }
        if (m.sequenceDue) {
            m.sequenceDue--;
            SequenceTick();
        }
    }
}

void SoundDriver::Do(Work work) {
    if (m.booting) {
        return;
    }
    m_spent = 0;
    switch (work) {
    case Work::Commands: Command(); break;
    case Work::Sequences: SequenceTick(); break;
    case Work::Adpcm: AdpcmTick(); break;
    }
}

// A command is sixteen bytes at the start of the shared memory; the last is 0xFF when there is none
void SoundDriver::TakeCommand() {
    memcpy(&m.queue[m.queueIn], m_shared, 16);
    m.queueIn = static_cast<uint8_t>(m.queueIn + 16);
    Share(0x00F, 0xFF);
}

// 00AA: the chip's registers, everything silent, and the head of the song table
void SoundDriver::StartUp() {
    for (unsigned n = 0;; n++) {
        const uint8_t reg = Rom(kChipSetupRegisters + n);
        Chip(reg, Word(kChipSetupValues + n * 2));
        if (reg == kNextState) {
            break;
        }
    }
    SilenceAll();
    m = Zeroed<State>();
    uint32_t place = Word(kSongTablePointer);
    m.songs = NextWord(place);
    m.musicVolume = Next(place);
    m.masterVolume = Next(place);
    m.songTable = place + 1;
    m.echoLength = kEchoShortest;
    m.deaf = 2; // (the program is that long about it, with interrupts off)
}

// --- commands (025E) ---

void SoundDriver::Command() {
    if (m.queueOut == m.queueIn) {
        return;
    }
    uint8_t *command = &m.queue[m.queueOut];
    Spend(kCommandTime);
    m.queueOut = static_cast<uint8_t>(m.queueOut + 16);
    if (getenv("CPS2RECOMP_SOUND_COMMANDS")) { // (every command, to stderr)
        fprintf(stderr, "sound command");
        for (int n = 0; n < 13; n++) {
            fprintf(stderr, " %02X", command[n]);
        }
        fprintf(stderr, "\n");
    }
    if (command[0] == 0xFF) {
        Special(command);
        return;
    }
    // A song: its number, and for sound effects how it is placed (bytes 6 to 12)
    if (m.songs == 0) {
        return;
    }
    const uint16_t number = static_cast<uint16_t>(((command[0] << 8) | command[1]) % m.songs);
    uint32_t entry = m.songTable + static_cast<uint16_t>(number * 4);
    const uint8_t top = Next(entry);
    const uint16_t rest = NextWord(entry);
    if (top == 0 && rest == 0) {
        return;
    }
    m.song = (static_cast<uint32_t>(top) << 16) | rest;
    uint32_t place = m.song;
    const uint8_t kind = Next(place);
    if (kind == 0) {
        StartMusic(place);
    } else if (kind < 0x80) {
        StartEffect(place, kind, command);
    } else if ((kind & 0x7F) < 0x10) {
        m_voice = kind & 0x7F;
        StartSample(place, command);
    } else {
        m_voice = static_cast<uint8_t>((kind & 0x7F) - 0x10);
        StartAdpcm(place, command);
    }
}

// 04B9: the commands that are not songs
void SoundDriver::Special(uint8_t *command) {
    switch (command[1]) {
    case 0: // everything stops
        m.fade = 0;
        for (unsigned n = 0; n < 32; n++) {
            Track &track = n < 16 ? m.effects[n] : m.music[n - 16];
            if (track.state) {
                Silence(n & 15);
            }
            track.priority = 0;
            track.state = kOff;
        }
        StopAdpcm();
        break;
    case 1: // the sound effects stop
        for (m_voice = 0; m_voice < 16; m_voice++) {
            if (m.effects[m_voice].state) {
                EndEffect(m.effects[m_voice]);
            }
        }
        break;
    case 2: // one voice's sound effect stops
        m_voice = command[4];
        if (m_voice < 16 && m.effects[m_voice].state) {
            EndEffect(m.effects[m_voice]);
        }
        break;
    case 3: StopAdpcm(); break;
    case 4: // one ADPCM voice stops
        if (command[4] < 3) {
            Adpcm &voice = m.adpcm[command[4]];
            if (voice.left) {
                voice.priority = 0;
                voice.left = 0;
                SilenceAdpcm(command[4]);
            }
        }
        break;
    case 5: // the music stops
        for (uint8_t n = 0; n < 16; n++) {
            if (m.music[n].state) {
                m.music[n].state = kOff;
                if (!m.effects[n].state) {
                    Silence(n);
                }
            }
        }
        break;
    case 6: // the music's volume goes up to a level, or (7) down to one
    case 7: {
        const bool down = command[1] == 7;
        if (!m.musicFade) {
            m.musicFadeLevel = down ? 0xFFFF : 0;
        }
        m.musicFade = down ? 0xFF : 1;
        const uint16_t rate = static_cast<uint16_t>((command[3] << 8) | command[4]);
        m.musicFadeRate = down ? Negative(rate) : rate;
        m.musicFadeTarget = Word(kLevels + (command[2] & 0xFE));
        break;
    }
    case 8: { // the music faster or slower
        const uint16_t change = Middle(command[4], 0x0369);
        m.tempoAdjust = ++command[3] == 0 ? Negative(change) : change;
        break;
    }
    case 9: { // the music higher or lower, in sixteenths of a semitone
        const uint16_t change = static_cast<uint16_t>(command[4] << 4);
        m.pitchShift = ++command[3] == 0 ? Negative(change) : change;
        break;
    }
    case 12: // everything fades in, or (13) out
    case 13: {
        const bool out = command[1] == 13;
        if (!m.fade) {
            m.fadeLevel = out ? 0xFFFF : 0;
        }
        m.fade = out ? 0xFF : 1;
        const uint16_t rate = static_cast<uint16_t>((command[3] << 8) | command[4]);
        m.fadeRate = out ? Negative(rate) : rate;
        break;
    }
    default: break;
    }
}

void SoundDriver::StopAdpcm() {
    for (uint8_t n = 0; n < 3; n++) {
        if (m.adpcm[n].left) {
            SilenceAdpcm(n);
        }
        m.adpcm[n].priority = 0;
        m.adpcm[n].left = 0;
    }
}

namespace {
    // 0442: a sequence's track as it starts
    template <typename Track>
    Track Fresh() {
        Track track = Zeroed<Track>();
        track.state = kSequence;
        track.flags = kLfoRestarts;
        track.target = 0xFF00; // (no note yet)
        track.expression = 0x60;
        track.envelope = kStill;
        track.sample = 0xFF00;
        track.pan.mode = 0xFF;
        track.pan.at = 0x1000;
        return track;
    }
} // namespace

// 0398: a song of kind 0 is the music: a track for each voice (where the song has one)
void SoundDriver::StartMusic(uint32_t place) {
    for (m_voice = 0; m_voice < 16; m_voice++) {
        Track &track = m.music[m_voice];
        const uint8_t was = track.state;
        track.state = kOff;
        if (const uint16_t offset = NextWord(place)) {
            track = Fresh<Track>();
            track.place = track.start = (m.song + offset) & 0xFFFFFF;
            Spend(kCopyTime);
            track.instruments = Word(kInstrumentTables);
        }
        Spend(kVoiceTime);
        if (!m.effects[m_voice].state) {
            WritePan(0x10);
            if (was) {
                Silence(m_voice);
            }
        }
    }
    m.musicTempo = 0;
    m.tempoAdjust = 0;
    m.pitchShift = 0;
    m.musicFade = 0;
}

// 0310: a song of kind 1 to 0x7F is a sound effect in sequences, and the kind its priority: each of its
// tracks takes its voice unless what is there matters more
void SoundDriver::StartEffect(uint32_t place, uint8_t priority, const uint8_t *command) {
    m_priority = priority;
    for (m_voice = 0; m_voice < 16; m_voice++) {
        const uint16_t offset = NextWord(place);
        Track &track = m.effects[m_voice];
        Spend(kVoiceTime);
        if (!offset || priority < track.priority) {
            continue;
        }
        track = Fresh<Track>();
        track.place = track.start = (m.song + offset) & 0xFFFFFF;
        Spend(kCopyTime);
        track.instruments = Word(kInstrumentTables);
        track.priority = priority;
        PlaceEffect(track, command);
    }
}

// 0846: a song of kind 0x80 to 0x8F is one sample on that voice: priority (bit 7: until stopped), length and
// gate in ticks, volume, sample (or instrument), note and tuning
void SoundDriver::StartSample(uint32_t place, const uint8_t *command) {
    Track &track = m.effects[m_voice];
    const uint8_t first = Next(place);
    m_priority = first;
    if ((first & 0x7F) < track.priority) {
        return;
    }
    track = Zeroed<Track>();
    track.time = NextWord(place);
    Spend(kCopyTime);
    track.gate = NextWord(place);
    const uint16_t volume = Table(kVolumes, Next(place));
    track.volume = Middle(volume, Table(kLoudness, static_cast<uint8_t>(m.masterVolume + 0x40)));
    track.sample = NextWord(place);
    track.pitch = static_cast<uint16_t>(Next(place) << 8);
    track.pitchOffset = static_cast<uint16_t>(static_cast<int8_t>(Rom(place)));
    track.state = kSingle;
    track.pending = true;
    m_priority = first & 0x7F;
    track.priority = first & 0x7F;
    track.hold = first & 0x80;
    PlaceEffect(track, command);
}

// 09F7: how the command wants the sound placed, then (0C45) the voice made ready
void SoundDriver::PlaceEffect(Track &track, const uint8_t *command) {
    if (SetUpPan(track.pan, command)) {
        // On two voices, moving apart: the next voice gets the same sound, a little later
        Track &other = m.effects[(m_voice + 1) & 15];
        if (m_priority >= other.priority) {
            other = track;
            const uint8_t from = static_cast<uint8_t>(track.pan.at >> 8), width = PanPlace(command[8]);
            other.pan.target = from > width ? static_cast<uint8_t>(from - width) : 0;
            other.pan.speed = Negative(static_cast<uint16_t>((command[9] << 8) | command[10]));
            other.pan.delay = 4;
        }
    }
    PlacePan(track);
}

// The eight ways (command byte 6): where it starts (7), how far it goes (8), how fast (9-10, and 11-12 for
// the second leg). True for the way that takes a second voice.
bool SoundDriver::SetUpPan(Pan &pan, const uint8_t *command) {
    const uint8_t from = PanPlace(command[7]), width = PanPlace(command[8]);
    const uint16_t speed = static_cast<uint16_t>((command[9] << 8) | command[10]);
    const uint16_t speed2 = static_cast<uint16_t>((command[11] << 8) | command[12]);
    const uint8_t up = PanPlace(static_cast<uint8_t>(from + width));
    const uint8_t down = from > width ? static_cast<uint8_t>(from - width) : 0;
    if (command[6] == 0 || command[6] > 7) { // stays
        pan.mode = 0xFF;
        pan.at = static_cast<uint16_t>((from << 8) | (pan.at & 0xFF));
        return false;
    }
    pan.at = static_cast<uint16_t>(from << 8);
    switch (command[6]) {
    case 1: // to and fro across everything, starting towards the far side
        pan.mode = 0;
        if (from < 0x11) {
            pan.step = 0xE0, pan.target = 0x20, pan.speed = speed;
        } else {
            pan.step = 0x20, pan.target = 0, pan.speed = Negative(speed);
        }
        break;
    case 2: // to and fro over a width
        pan.mode = 0;
        if (from < 0x11) {
            pan.step = static_cast<uint8_t>(0 - width), pan.target = up, pan.speed = speed;
        } else {
            pan.step = width, pan.target = down, pan.speed = Negative(speed);
        }
        break;
    case 3: // to the right, once
    case 7: // (and a second voice to the left)
        pan.mode = 1, pan.target = up, pan.speed = speed;
        break;
    case 4: // to the left, once
        pan.mode = 1, pan.target = down, pan.speed = Negative(speed);
        break;
    case 5: // to the right, then on to the end at another speed
        pan.mode = 2, pan.target = up, pan.speed = speed, pan.speed2 = speed2;
        break;
    default:
        pan.mode = 2, pan.target = down, pan.speed = Negative(speed), pan.speed2 = Negative(speed2);
        break;
    }
    return command[6] == 7;
}

// 1A35, 1A79, 1A96 (and the same for ADPCM): a tick of movement. True if the place is to be written.
bool SoundDriver::MovePan(Pan &pan) {
    if (pan.mode & 0x80) {
        return false;
    }
    const uint32_t sum = static_cast<uint32_t>(pan.at) + pan.speed;
    uint8_t place = static_cast<uint8_t>(sum >> 8);
    // (going right: past the target; going left: at it or past it)
    const bool there = static_cast<uint8_t>(0xFF + (sum >> 16) + (pan.target < place ? 1 : 0)) == 0;
    if (there) {
        const uint8_t end = (pan.speed & 0x8000) ? 0 : 0x20;
        place = pan.target;
        if (pan.mode == 0) {
            pan.target = static_cast<uint8_t>(pan.target + pan.step);
            pan.step = static_cast<uint8_t>(0 - pan.step);
            pan.speed = Negative(pan.speed);
        } else if (pan.mode == 2 && pan.target != end) {
            pan.target = end;
            pan.speed = pan.speed2;
        } else {
            pan.mode = 0xFF;
        }
    }
    pan.at = static_cast<uint16_t>((place << 8) | (sum & 0xFF));
    return true;
}

// 0C45
void SoundDriver::PlacePan(Track &track) {
    const uint8_t place = m.stereo ? static_cast<uint8_t>(track.pan.at >> 8) : 0x10;
    Share(5, place);
    Chip(kPanRegisters + m_voice, static_cast<uint16_t>(kPanMiddle + place));
    Silence(m_voice);
}

// 1995
void SoundDriver::WritePan(uint8_t place) {
    Spend(kPanTime);
    if (!m.stereo) {
        place = 0x10;
    }
    Chip(kPanRegisters + m_voice, static_cast<uint16_t>(kPanMiddle + place));
}

// 07A4. (The last write is the program's: 0x1000 to the rate, where the phase may have been meant.)
void SoundDriver::Silence(uint8_t voice) {
    Spend(kWriteTime);
    const uint8_t base = static_cast<uint8_t>((voice << 3) | (voice >> 5));
    Chip(kEchoSends + voice, 0);
    Chip(base + kVoiceVolume, 0);
    Chip(base + kVoiceRate, 0);
    Chip(base + kVoiceRate, 0x1000);
}

// 0811
void SoundDriver::SilenceAdpcm(uint8_t voice) {
    Chip(kAdpcmVolume + voice * 4, 0);
    Chip(kAdpcmKeys + voice, 1);
}

// 077A
void SoundDriver::SilenceAll() {
    for (uint8_t voice = 0; voice < 16; voice++) {
        Silence(voice);
    }
    for (uint8_t voice = 0; voice < 3; voice++) {
        SilenceAdpcm(voice);
    }
    Chip(kEchoEnd, kEchoShortest);
}

// --- sequences (018C): every fourth interrupt, 62.5 times a second ---

void SoundDriver::SequenceTick() {
    for (m_voice = 0; m_voice < 16; m_voice++) {
        m_music = false;
        m_taken = false;
        Spend(kVoiceTime);
        Track &effect = m.effects[m_voice];
        if (effect.state) {
            if (effect.pan.delay) {
                if (--effect.pan.delay == 0) {
                    PlacePan(effect);
                    m_taken = true;
                }
            } else {
                if (effect.state & 0x80) {
                    RunSequence(effect, effect.tempo);
                } else {
                    RunSample(effect);
                }
                m_taken = effect.state != kOff;
            }
        }
        Track &music = m.music[m_voice];
        if (music.state) {
            m_music = true;
            if (m.pitchShift) {
                music.flags |= kPitchChanged;
            }
            RunSequence(music, static_cast<uint16_t>(m.musicTempo + m.tempoAdjust));
        }
    }
    Fades();
}

// 01FE: the two fades move on
void SoundDriver::Fades() {
    if (m.fade) {
        const uint32_t sum = static_cast<uint32_t>(m.fadeLevel) + m.fadeRate;
        const uint8_t over = static_cast<uint8_t>(((m.fade & 0x80) ? 0xFF : 0) + (sum >> 16));
        m.fadeLevel = static_cast<uint16_t>(sum);
        if (over) {
            // In: full, and the fade is over. Out: silent, and it stays so
            if (!(over & 0x80)) {
                m.fade = 0;
            }
            m.fadeLevel = 0;
        }
    }
    if (m.musicFade) {
        const uint8_t down = (m.musicFade & 0x80) ? 0xFF : 0;
        const uint32_t sum = static_cast<uint32_t>(m.musicFadeLevel) + m.musicFadeRate;
        uint16_t level = static_cast<uint16_t>(sum);
        bool there = static_cast<uint8_t>(down + (sum >> 16)) != 0;
        if (!there) {
            there = static_cast<uint8_t>(down + (level < m.musicFadeTarget ? 1 : 0)) == 0;
        }
        if (there) {
            level = m.musicFadeTarget;
            if (level == 0xFFFE) { // (back to full)
                m.musicFade = 0;
            }
        }
        m.musicFadeLevel = level;
    }
}

// 0C78: a single sample's tick
void SoundDriver::RunSample(Track &track) {
    Spend(kTrackTime);
    if (track.pending) {
        track.pending = false;
        uint16_t number = track.sample, level = 0xFFFF;
        if (number & 0x8000) {
            // An instrument (of the table in the high byte): its sample with tuning and an envelope
            uint32_t place = Table(kInstrumentTables, static_cast<uint8_t>(number >> 8)) + (number & 0xFF) * 8;
            number = Next(place);
            number = static_cast<uint16_t>(number | ((Next(place) & 0x7F) << 8));
            track.pitchOffset = static_cast<uint16_t>(track.pitchOffset + static_cast<int8_t>(Next(place)));
            track.attack = Next(place);
            track.envelopeRate = Table(kAttackRates, track.attack);
            track.decay = Next(place);
            track.sustain = Table(kLevels, Next(place));
            track.sustainRate = Next(place);
            track.release = Next(place);
            level = 0;
        }
        track.level = level;
        track.envelope = kAttack;
        SetSample(track, number);
        KeyOn(track);
        Update(track);
        if (!(track.sample & 0x8000)) {
            track.envelope = kStill;
            track.gate = 0;
        }
        return;
    }
    if (track.hold) {
        Update(track);
        return;
    }
    if (track.gate) {
        if (track.gate > 1) {
            track.gate--;
        } else if (track.envelope != kStill) {
            LetGo(track);
            track.gate = 0;
        }
    }
    track.time = (track.time - 1) & 0xFFFF;
    if (track.time) {
        Update(track);
    } else {
        EndEffect(track);
    }
}

void SoundDriver::LetGo(Track &track) {
    if (track.envelope != kStill) {
        track.envelope = kRelease;
        track.envelopeRate = Table(kDecayRates, track.release);
    }
}

// 0DC5: a sequence's tick: time passes at the tempo, and when the wait is over the sequence is read on
void SoundDriver::RunSequence(Track &track, uint16_t tempo) {
    Spend(kTrackTime);
    if (track.time) {
        if (track.gate) {
            if (track.gate <= tempo) {
                LetGo(track);
                track.gate = 0;
            } else {
                track.gate -= tempo;
            }
        }
        const bool over = track.time <= tempo;
        track.time -= tempo;
        if (!over && !(track.time & 0x80000000)) {
            Update(track);
            return;
        }
    }
    ReadSequence(track);
}

// 0E58: bytes below 0x80 start a wait, 0x80-0xBF a note, the rest are commands
void SoundDriver::ReadSequence(Track &track) {
    uint32_t place = track.place;
    auto jump = [&]() {
        const int16_t offset = static_cast<int16_t>(NextWord(place));
        place = (place + static_cast<uint32_t>(static_cast<int32_t>(offset))) & 0xFFFFFF;
    };
    for (unsigned guard = 0; guard < 100000; guard++) {
        const uint8_t byte = Next(place);
        if (byte < 0x80) {
            Spend(kEventTime);
            // A wait: seven bits a byte, until a byte with bit 7 (which is the next thing)
            uint16_t low = byte;
            uint8_t top = 0;
            while (!(Rom(place) & 0x80)) {
                top = static_cast<uint8_t>(low >> 9);
                low = static_cast<uint16_t>((low << 7) | Next(place));
            }
            track.place = place;
            track.time += ((static_cast<uint32_t>(top) << 16) | low) << 8;
            Update(track);
            return;
        }
        if (byte < 0xC0) {
            Spend(kNoteTime);
            // A note: loudness in the first byte, the note in the second (bit 7: tied to the next), then
            // how long it is held
            const uint8_t loudness = static_cast<uint8_t>(byte * 2);
            const uint8_t second = Next(place);
            const uint8_t note = second & 0x7F;
            uint16_t from = static_cast<uint16_t>(note << 8);
            track.slide = 0;
            if (track.slideRate & 0xFF) {
                const uint8_t last = static_cast<uint8_t>(track.target >> 8);
                if (last != 0xFF && last != note) {
                    // Sliding from the last note: the rate takes the direction's sign
                    const uint8_t sign = static_cast<uint8_t>((track.slideRate >> 8) + (last < note ? 1 : 0));
                    if (sign == 0) {
                        track.slideRate = Negative(track.slideRate);
                    }
                    from = track.target;
                    track.slide = 0xFF;
                }
            }
            track.target = static_cast<uint16_t>(note << 8);
            track.pitch = from;
            if (!(track.flags & kTied)) {
                if (track.flags & kLfoRestarts) {
                    track.flags &= 0x3F;
                    track.lfoPhase = 0;
                }
                if (!m_taken) {
                    KeyOn(track);
                }
                track.envelope = kAttack;
                track.level = 0;
                track.envelopeRate = Scale(Table(kLevels, loudness), Table(kAttackRates, track.attack));
            }
            track.noteVolume = Table(kLoudness, loudness);
            uint16_t low = 0;
            uint8_t top = 0, more;
            do {
                more = Next(place);
                top = static_cast<uint8_t>(low >> 9);
                low = static_cast<uint16_t>((low << 7) | (more & 0x7F));
            } while (more & 0x80);
            track.flags = (track.flags & ~kTied) | kPitchChanged;
            track.gate = ((static_cast<uint32_t>(top) << 16) | low) << 8;
            if (second & 0x80) {
                track.flags |= kTied;
                track.gate = 0;
            }
            SetVolume(track);
            continue;
        }
        if (byte == 0xFF) {
            EndTrack(track);
            return;
        }
        // 110C: the commands
        Spend(kEventTime);
        switch (byte - 0xC0) {
        case 1: { // tempo
            const uint16_t tempo = NextWord(place);
            if (m_music) {
                m.musicTempo = tempo;
            } else {
                track.tempo = tempo;
            }
            break;
        }
        case 2: track.instruments = Table(kInstrumentTables, Next(place)); break;
        case 3: { // fine tuning
            const uint8_t amount = Next(place);
            const uint16_t tune = Word(kFineTunes + (amount >= 0x81 ? static_cast<uint8_t>(0 - amount) : amount) * 2);
            track.fineTune = (amount & 0x80) ? Negative(tune) : tune;
            SetPitchOffset(track);
            break;
        }
        case 4: { // instrument: sample, tuning, envelope
            uint32_t entry = track.instruments + Next(place) * 8;
            uint16_t number = Next(entry);
            number = static_cast<uint16_t>(number | (Next(entry) << 8));
            track.instrumentTune = Next(entry);
            track.attack = Next(entry);
            track.decay = Next(entry);
            track.sustain = Table(kLevels, Next(entry));
            track.sustainRate = Next(entry);
            track.release = Next(entry);
            if (number & 0x8000) {
                number &= 0x7FFF;
                track.flags |= kShifts;
            }
            if (track.sample != number) {
                track.sample = number;
                if (!m_taken) {
                    SetSample(track, number);
                }
            }
            track.envelope = kStill;
            SetPitchOffset(track);
            break;
        }
        case 5: // vibrato depth
            track.vibrato = Word(kVibratoDepths + Next(place) * 2);
            track.flags |= kPitchChanged;
            goto lfo;
        case 6: // the track's volume
            track.trackVolume = Table(kVolumes, Next(place));
            SetVolume(track);
            break;
        case 7: { // place between left and right (the music only)
            const uint8_t value = Next(place);
            if (m_music) {
                const uint8_t at = value == 0x7F ? 0x20 : (value >> 2) & 0x1F;
                track.pan.at = static_cast<uint16_t>((at << 8) | (track.pan.at & 0xFF));
                if (!m_taken) {
                    WritePan(at);
                }
            }
            break;
        }
        case 8: // expression
            track.expression = Next(place);
            SetVolume(track);
            break;
        case 9: { // slide between notes (portamento): the rate, keeping the sign it has
            const uint8_t rate = static_cast<uint8_t>(Next(place) * 2);
            const uint8_t low = (track.slideRate & 0x8000) ? static_cast<uint8_t>(0 - rate) : rate;
            track.slideRate = static_cast<uint16_t>((track.slideRate & 0xFF00) | low);
            break;
        }
        case 10: // from the start again
            track.state = kRepeating;
            place = track.start;
            break;
        case 11: // the end, the second time round
            if (track.state == kRepeating) {
                EndTrack(track);
                return;
            }
            break;
        case 12: // on from somewhere else, as the second time round
            track.state = kRepeating;
            jump();
            break;
        case 13: // ... the second time round only (which becomes the first)
            if (track.state == kRepeating) {
                track.state = kSequence;
                jump();
            } else {
                place += 2;
            }
            break;
        case 14: jump(); break;
        case 15: { // on from where another track starts
            const uint8_t other = Rom(place);
            place = (m_music || other >= 16 ? m.music[other & 15] : m.effects[other]).start;
            break;
        }
        case 16: case 17: case 18: case 19: // a loop starts here
            track.loopPlace[byte - 0xD0] = place;
            break;
        case 20: case 21: case 22: case 23: { // back to the loop's start: so many times in all (0: for ever)
            const unsigned loop = byte - 0xD4;
            const uint8_t times = Next(place);
            if (times) {
                const uint8_t left = --track.loopCount[loop];
                if (left == 0) {
                    break;
                }
                if (left & 0x80) {
                    track.loopCount[loop] = times;
                }
            }
            place = track.loopPlace[loop];
            break;
        }
        case 24: case 25: case 26: case 27: // out of the loop on its last time round
            if (track.loopCount[byte - 0xD8] == 1) {
                track.loopCount[byte - 0xD8] = 0;
                jump();
            } else {
                place += 2;
            }
            break;
        case 28: // transpose
            track.transpose = Next(place);
            SetPitchOffset(track);
            break;
        case 29:
            track.transposeAdd = Clamped(static_cast<uint8_t>(track.transposeAdd + Next(place)));
            SetPitchOffset(track);
            break;
        case 30: // volume, in steps added to the game's
            track.volumeOffset = Next(place);
            SetVolume(track);
            break;
        case 31:
            track.volumeAdd = Clamped(static_cast<uint8_t>(track.volumeAdd + Next(place)));
            SetVolume(track);
            break;
        case 32: // whether the LFO starts again with each note
            track.flags = static_cast<uint8_t>((track.flags & ~kLfoRestarts) | ((Next(place) & 1) ? kLfoRestarts : 0));
            break;
        case 33: // LFO rate
            track.lfoRate = Table(kLfoRates, Next(place));
            track.flags |= kPitchChanged;
            goto lfo;
        case 34: // tremolo depth
            track.tremolo = Word(kTremoloDepths + Next(place) * 2);
            goto lfo;
        lfo:
            track.flags &= ~(kVibrato | kTremolo);
            if (track.lfoRate) {
                if (track.vibrato) {
                    track.flags |= kVibrato;
                }
                if (track.tremolo) {
                    track.flags |= kTremolo;
                }
            }
            break;
        case 35: track.priority = Next(place); break;
        case 36: { // the echo's length
            const uint16_t length = NextWord(place);
            const uint16_t was = m.echoLength;
            m.echoLength = length;
            if (length < was) {
                Chip(kNextState, 0);
            }
            Chip(kEchoEnd, length);
            break;
        }
        case 37: Chip(kEchoFeedback, NextWord(place)); break;
        case 38: // how much of the voice goes to the echo
            track.echo = static_cast<uint16_t>(Word(kVolumes + Next(place) * 2) * 4);
            break;
        default: break; // (0: nothing)
        }
    }
    EndTrack(track);
}

// 19BC
void SoundDriver::EndTrack(Track &track) {
    if (!m_music) {
        EndEffect(track);
        return;
    }
    track.state = kOff;
    if (!m_taken) {
        Silence(m_voice);
    }
}

// 19D1: a sound effect is over, and the music's track gets the voice back: its sample and place again; the
// note sounds again only for the tracks marked so
void SoundDriver::EndEffect(Track &track) {
    track.priority = 0;
    track.state = kOff;
    Silence(m_voice);
    Track &music = m.music[m_voice];
    if (music.state) {
        SetSample(music, music.sample);
        WritePan(static_cast<uint8_t>(music.pan.at >> 8));
        if (music.flags & kShifts) {
            KeyOn(music);
        } else {
            music.envelope = kStill;
        }
    }
}

// 1252: a sample's entry in the table: bank, start, loop start, end, the note it was recorded at
void SoundDriver::SetSample(Track &track, uint16_t number) {
    Spend(kSampleTime);
    uint32_t entry = Word(kSampleTablePointer) + static_cast<uint16_t>(number * 8);
    const unsigned base = m_voice * 8;
    Chip(base + kVoiceVolume, 0);
    Chip(base + kVoiceRate, 0);
    const uint8_t bank = Next(entry);
    uint16_t start = Next(entry);
    start = static_cast<uint16_t>(start | (Next(entry) << 8));
    track.sampleStart = start;
    Chip(base + kVoiceStart, start);
    uint16_t loop = Next(entry);
    loop = static_cast<uint16_t>(loop | (Next(entry) << 8));
    uint16_t end = Next(entry);
    end = static_cast<uint16_t>(end | (Next(entry) << 8));
    Chip(base + kVoiceEnd, end);
    track.sampleKey = static_cast<uint8_t>(0x3C - Rom(entry));
    Chip(base + kVoiceLoop, static_cast<uint16_t>(end - loop));
    Chip(((m_voice - 1) * 8) & 0x78, static_cast<uint16_t>(0x8000 | bank)); // (a voice's bank register is the one before)
}

// 106E: the sample from its start
void SoundDriver::KeyOn(Track &track) {
    track.flags |= kPitchChanged;
    const unsigned base = m_voice * 8;
    Chip(base + kVoiceVolume, 0);
    Chip(base + kVoiceRate, 0);
    Chip(kEchoSends + m_voice, track.echo);
    Chip(base + kVoiceStart, track.sampleStart);
    Chip(base + kVoicePhase, 0x8000);
}

// 13BA: what is added to every note: transposition in semitones, fine tuning, the instrument's tuning
void SoundDriver::SetPitchOffset(Track &track) {
    track.flags |= kPitchChanged;
    const uint8_t semitones = Clamped(static_cast<uint8_t>(track.transpose + track.transposeAdd));
    track.pitchOffset = static_cast<uint16_t>(track.fineTune + (semitones << 8) + static_cast<int8_t>(track.instrumentTune));
}

// 1429: the track's volume from its own settings, the game's, and the note's loudness
void SoundDriver::SetVolume(Track &track) {
    Spend(kEventTime);
    uint16_t volume = 0;
    if (const uint8_t expression = static_cast<uint8_t>(track.expression * 2)) {
        volume = Middle(track.trackVolume, Word(kLoudness + expression));
        // (each sum is looked at for running over before the next is added)
        uint8_t steps = static_cast<uint8_t>(track.volumeOffset + track.volumeAdd);
        uint8_t shifted = static_cast<uint8_t>(steps + 0x40);
        if (!(shifted & 0x80) && m_music) {
            steps = static_cast<uint8_t>(steps + m.musicVolume);
            shifted = static_cast<uint8_t>(steps + 0x40);
        }
        if (!(shifted & 0x80)) {
            shifted = static_cast<uint8_t>(steps + m.masterVolume + 0x40);
        }
        if (shifted & 0x80) {
            shifted = (shifted & 0x40) ? 0 : 0x7F;
        }
        if (const uint8_t index = static_cast<uint8_t>(shifted * 2)) {
            volume = Middle(volume, Word(kLoudness + index));
            volume = Middle(volume, track.noteVolume);
            if (volume >= 0x4000) {
                volume = 0x3FFF;
            }
        } else {
            volume = 0;
        }
    }
    track.volume = volume;
}

// 1A1B: a tick of everything that moves by itself (place, LFO, slide, envelope), and the voice's volume and
// rate written
void SoundDriver::Update(Track &track) {
    Spend(kPlaceTime);
    if (MovePan(track.pan)) {
        const uint8_t place = m.stereo ? static_cast<uint8_t>(track.pan.at >> 8) : 0x10;
        Share(5, place);
        Chip(kPanRegisters + m_voice, static_cast<uint16_t>(kPanMiddle + place));
    }

    // The LFO is a triangle wave: the phase runs up, and each overflow starts the next quarter
    Spend(kEnvelopeTime);
    const uint32_t phase = static_cast<uint32_t>(track.lfoPhase) + track.lfoRate;
    track.lfoPhase = static_cast<uint16_t>(phase);
    if (phase > 0xFFFF) {
        track.flags = static_cast<uint8_t>(track.flags + 0x40);
    }

    if (track.slide) {
        if (track.slide == 1) {
            track.slide = 0;
        } else {
            const uint16_t next = static_cast<uint16_t>(track.pitch + track.slideRate);
            const bool down = track.slideRate & 0x8000;
            if (next == track.target || down == (next < track.target)) {
                track.pitch = track.target;
                track.slide = 1;
            } else {
                track.pitch = next;
            }
        }
    }

    uint16_t level = track.level;
    switch (track.envelope) {
    case kAttack: {
        const uint32_t sum = static_cast<uint32_t>(level) + track.envelopeRate;
        level = static_cast<uint16_t>(sum);
        if (sum > 0xFFFF) {
            level = 0xFFFF;
            track.envelope = kDecay;
            track.envelopeRate = Table(kDecayRates, track.decay);
        }
        break;
    }
    case kDecay:
        if (level >= track.envelopeRate && track.sustain < static_cast<uint16_t>(level - track.envelopeRate)) {
            level = static_cast<uint16_t>(level - track.envelopeRate);
        } else if (track.sustain == 0) {
            track.envelope = kStill;
            level = 0;
        } else {
            track.envelope = kSustain;
            track.envelopeRate = Table(kDecayRates, track.sustainRate);
            level = track.sustain;
        }
        break;
    case kSustain:
    case kRelease:
        if (level > track.envelopeRate) {
            level = static_cast<uint16_t>(level - track.envelopeRate);
        } else {
            track.envelope = kStill;
            level = 0;
        }
        break;
    default:
        track.flags &= ~kPitchChanged;
        return;
    }
    track.level = level;
    if (m_taken) {
        track.flags &= ~kPitchChanged;
        return;
    }

    uint16_t wave = track.lfoPhase;
    if (track.flags & kLfoFalling) {
        wave = static_cast<uint16_t>(0xFFFF - wave);
    }
    uint16_t volume = level;
    if (track.state & 0x80) {
        if (track.flags & kTremolo) {
            volume = Scale(static_cast<uint16_t>(0xFFFF - Scale(wave, track.tremolo)), volume);
        }
        if (m_music && m.musicFade) {
            volume = Scale(volume, m.musicFadeLevel);
        }
    }
    if (m.fade) {
        volume = Scale(volume, m.fadeLevel);
    }
    Chip(m_voice * 8 + kVoiceVolume, Scale(volume, track.volume));

    uint16_t pitch = track.pitch;
    if (track.flags & kVibrato) {
        const uint16_t swing = Scale(wave, track.vibrato);
        pitch = static_cast<uint16_t>(pitch + ((track.flags & kLfoNegative) ? Negative(swing) : swing));
    } else if (!(track.flags & kPitchChanged) && !track.slide) {
        return;
    }
    Spend(kPitchTime);
    // The note, counted from the one the sample was recorded at, to the chip's rate: a table by semitone,
    // and between semitones a table of fractions for the lowest octave, doubled for each octave above
    pitch = static_cast<uint16_t>(pitch + (track.sampleKey << 8) + track.pitchOffset);
    if (track.flags & kShifts) {
        pitch = static_cast<uint16_t>(pitch + m.pitchShift);
    }
    uint8_t note = static_cast<uint8_t>(pitch >> 8);
    if (note >= 0x6C) {
        note = static_cast<uint8_t>(note - (note >= 0xC0 ? 0x94 : 0x6C));
    }
    const uint8_t octave = Rom(kPitchOctaves + note);
    const uint8_t semitone = static_cast<uint8_t>(note - Rom(kPitchOctaveStarts + octave));
    const uint16_t fraction = static_cast<uint16_t>(Rom(kPitchFractions + ((semitone << 8) | (pitch & 0xFF))) << octave);
    Chip(m_voice * 8 + kVoiceRate, static_cast<uint16_t>(Word(kPitchRates + note * 2) + fraction));
    track.flags &= ~kPitchChanged;
}

// --- the ADPCM voices: every fourth interrupt ---

// 2FA8: a song of kind 0x90 and up is one ADPCM sample on that voice: priority (bit 7: repeats), volume, sample
void SoundDriver::StartAdpcm(uint32_t place, const uint8_t *command) {
    if (m_voice >= 3) {
        return;
    }
    Adpcm &voice = m.adpcm[m_voice];
    const uint8_t first = Next(place);
    m_priority = first;
    if ((first & 0x7F) < voice.priority) {
        return;
    }
    voice = Zeroed<Adpcm>();
    voice.starting = 0xFF;
    voice.priority = first & 0x7F;
    voice.repeats = first & 0x80;
    const uint16_t volume = Table(kVolumes, Next(place));
    voice.volume = static_cast<uint16_t>(Middle(volume, Table(kLoudness, static_cast<uint8_t>(m.masterVolume + 0x40))) * 4);
    if (voice.volume & 0x8000) {
        voice.volume = 0x7FF4;
    }
    uint32_t entry = Word(kSampleTablePointer) + static_cast<uint16_t>(NextWord(place) * 8);
    voice.bank = Next(entry);
    entry++;
    voice.start = Next(entry);
    voice.start = static_cast<uint16_t>(voice.start | (Next(entry) << 8));
    voice.end = Next(entry);
    voice.end = static_cast<uint16_t>(voice.end | (Next(entry) << 8));
    voice.length = voice.left = static_cast<uint16_t>(voice.end - voice.start);
    if (SetUpPan(voice.pan, command)) {
        Adpcm &other = m.adpcm[m_voice == 2 ? 0 : m_voice + 1];
        if (m_priority >= other.priority) {
            other = voice;
            const uint8_t from = static_cast<uint8_t>(voice.pan.at >> 8), width = PanPlace(command[8]);
            other.pan.target = from > width ? static_cast<uint8_t>(from - width) : 0;
            other.pan.speed = Negative(static_cast<uint16_t>((command[9] << 8) | command[10]));
            other.pan.delay = 4;
        }
    }
    SilenceAdpcm(m_voice);
}

// 33CD
void SoundDriver::AdpcmTick() {
    for (m_voice = 0; m_voice < 3; m_voice++) {
        if (m.adpcm[m_voice].left) {
            RunAdpcm(m.adpcm[m_voice]);
        }
    }
}

// 33F1
void SoundDriver::RunAdpcm(Adpcm &voice) {
    Spend(kTrackTime);
    if (voice.pan.delay) {
        if (--voice.pan.delay == 0) {
            SilenceAdpcm(m_voice);
        }
        return;
    }
    bool start = voice.starting;
    if (!start) {
        voice.left = voice.left < 0x40 ? 0 : static_cast<uint16_t>(voice.left - 0x40);
        if (voice.left == 0) {
            if (!voice.repeats) {
                voice.priority = 0;
                SilenceAdpcm(m_voice);
                return;
            }
            voice.left = voice.length;
            start = true;
        }
    }
    bool write = false;
    if (start) {
        const unsigned base = m_voice * 4;
        Chip(kAdpcmStart + base, voice.start);
        Chip(kAdpcmEnd + base, voice.end);
        Chip(kAdpcmBank + base, static_cast<uint16_t>(0x8000 | voice.bank));
        Chip(kAdpcmVolume + base, m.fade ? Scale(voice.volume, m.fadeLevel) : voice.volume);
        Chip(kAdpcmKeys + m_voice, 1);
        write = voice.starting;
        voice.starting = 0;
    }
    if (write || MovePan(voice.pan)) {
        const uint8_t place = m.stereo ? static_cast<uint8_t>(voice.pan.at >> 8) : 0x10;
        Share(5, place);
        Chip(kAdpcmPanRegisters + m_voice, static_cast<uint16_t>(kPanMiddle + place));
    }
}

void SoundDriver::Describe(unsigned voice) const {
    for (int n = 0; n < 3; n++) {
        const Adpcm &a = m.adpcm[n];
        fprintf(stderr, "  driver ADPCM %d: starting %02X repeats %02X priority %02X left %04X length %04X volume %04X bank %02X start %04X "
                        "end %04X pan mode %02X at %04X step %02X target %02X speed %04X %04X delay %u\n",
                n, a.starting, a.repeats, a.priority, a.left, a.length, a.volume, a.bank, a.start, a.end, a.pan.mode, a.pan.at,
                a.pan.step, a.pan.target, a.pan.speed, a.pan.speed2, a.pan.delay);
    }
    for (int which = 0; which < 2; which++) {
        const Track &t = which ? m.music[voice & 15] : m.effects[voice & 15];
        fprintf(stderr, "  driver %s %u: state %02X priority %02X time %08X gate %08X place %06X flags %02X pan mode %02X at %04X step %02X "
                        "target %02X speed %04X %04X delay %u envelope %u level %04X volume %04X stereo %02X\n",
                which ? "music" : "effect", voice & 15, t.state, t.priority, t.time, t.gate, t.place, t.flags, t.pan.mode, t.pan.at,
                t.pan.step, t.pan.target, t.pan.speed, t.pan.speed2, t.pan.delay, t.envelope, t.level, t.volume, m.stereo);
    }
}

void SoundDriver::Serialize(Archive &ar) {
    ar.Mark(0x53445256);
    ar.Pod(m);
}

} // namespace cps2recomp
