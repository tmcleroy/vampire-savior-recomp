// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of cps2recomp.
//
// A training mode's parts that every fighting game on the board has in common, for a game's sources to put
// together: frame data and its meter, an input display in the notation players write, a combo readout, a dummy
// that stands, crouches, jumps, guards, records and replays, and hit boxes drawn on the picture. Nothing here
// knows a game's memory: a game's sources say, a step at a time, what each player is doing, and these keep
// count and say what to show (as the HUD's lines and boxes: game.h).
#pragma once

#include "game.h"

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace cps2recomp {
class Archive;
}

namespace cps2recomp::fight {

// --- what a player is doing at a step of the game ---

enum class Doing : uint8_t {
    Free,      // can do anything
    Startup,   // in a move, before it can hit
    Active,    // in a move that can hit now
    Recovery,  // in a move, after it could hit
    HitStun,   // reeling from a hit
    GuardStun, // reeling from a guarded hit
    Air,       // jumping (and not attacking)
    Held,      // being thrown, and the other things done to a player
};

// What a game's sources tell of a player at a step (seen as the player's own routine starts on that step: its
// record is as the last step left it, which is what the picture showed and the hit tests tested)
struct StepView {
    enum Control : uint8_t { Own, Reeling, Held } control = Own;
    bool canAct = true;     // free to start anything (standing, crouching, walking, jumping without attacking)
    bool frozen = false;    // stopped by a hit landing, on a frame that does not go on through it
    bool attacking = false; // the game's flag for an attack being out (or a move being done)
    bool jumping = false;   // in the air
    bool attackOut = false; // its attack box is out this step
};

// --- frame data ---

// Watches both players a step at a time and tells, for each move: how many steps before it can hit (startup:
// the number of the first step it can hit on), how many it can hit for (active), and how many after that before
// the player can act (recovery); after a hit or a guarded one, who can act first and by how many steps. A meter
// shows the last sixty steps.
class Frames {
public:
    // Each player once a step, player 1 first
    void Step(int player, const StepView &view);
    // A hit has landed on a player, or been guarded
    void Contact(int victim, bool guarded);
    Doing Now(int player) const {
        return m_g.track[player].doing;
    }
    uint32_t StepCount() const {
        return m_g.step;
    }
    void Reset() {
        m_g = State();
    }
    void Serialize(Archive &ar);
    // The numbers as lines of text, at the players' sides (y: the first line's height, a part of the picture);
    // the meter as boxes, two rows of cells `top` pixels down, in the middle of a picture `width` wide
    void Hud(bool numbers, bool meter, std::vector<HudLine> &lines, std::vector<HudBox> &boxes, float y = 0.72f, int top = 193,
             int width = 384, int left = 0) const;
    // Every move's numbers as they are known, to stderr (for tests)
    bool print = false;
    // The last move a player finished: its startup, active and recovery, and the step it ended on (0: none)
    struct Move {
        uint16_t startup = 0, active = 0, recovery = 0;
        uint32_t at = 0;
    };
    Move LastMove(int player) const {
        const Track &t = m_g.track[player];
        return {t.shownStartup, t.shownActive, t.shownRecovery, t.shownAt};
    }
    // The last advantage worked out: for which player (-1: none yet), how many steps, on a guard or a hit
    struct Advantage {
        int player = -1;
        int32_t steps = 0;
        bool guarded = false;
        uint32_t at = 0;
    };
    Advantage LastAdvantage() const {
        return {m_g.advantageFor, m_g.advantage, m_g.advantageGuarded, m_g.advantageAt};
    }

    static constexpr int kMeterCells = 60;

private:
    struct Track {
        Doing doing = Doing::Free;
        bool inMove = false, hadActive = false, frozen = false;
        uint16_t total = 0, startup = 0, active = 0, lastActiveAt = 0;
        uint16_t shownStartup = 0, shownActive = 0, shownRecovery = 0;
        uint32_t shownAt = 0;
        uint32_t freeAt = 0;
        bool guarded = false;
        bool wasHit = false;
    };
    struct State {
        Track track[2];
        uint32_t step = 0;
        bool pending = false;
        int pendingVictim = 0;
        uint32_t pendingAt = 0;
        bool pendingGuarded = false;
        int advantageFor = -1;
        int32_t advantage = 0;
        bool advantageGuarded = false;
        uint32_t advantageAt = 0;
        uint8_t meter[2][kMeterCells] = {};
        int meterLength = 0, idle = 0;
    } m_g;
    Doing Classify(int player, const StepView &view);
    void BothStepped();
};

// Every move's numbers as lines of a CSV file (frame_data.csv beside settings.ini): the character, what was held
// as the move began, in the air or not, which move it was (the game's own number for it: its first animation
// frame, say), startup, active, recovery, and the advantage after it when one comes within a second. Opened on the
// first line; nothing is written while not asked to `Step`.
class MoveLog {
public:
    // A move's start, as the game's sources tell it (asked only on the step a move begins)
    struct Start {
        std::string character, input;
        bool air = false;
        uint32_t id = 0;
    };
    using StartFn = Start (*)(int player);
    // Each player once a step, after Frames::Step
    void Step(int player, const Frames &frames, StartFn start);
    ~MoveLog();

private:
    struct Pending {
        bool open = false, finished = false;
        Start start;
        Frames::Move move;
        uint32_t waited = 0;
    };
    void Write(int player, const Frames::Advantage *advantage);
    Pending m_pending[2];
    uint32_t m_advantageAt = 0;
    std::FILE *m_file = nullptr;
    bool m_tried = false;
};

// --- what the players hold ---

// A player's controls as the board's bits (machine.h: right, left, down, up, three punches, three kicks)
enum : uint16_t {
    kStickRight = 1 << 0,
    kStickLeft = 1 << 1,
    kStickDown = 1 << 2,
    kStickUp = 1 << 3,
    kSticks = 0x0F,
    kButtons = 0x3F0,
};
// What is held as players write it: the stick as a number on a keypad (5 in the middle, 6 forward, 4 back,
// 2 down...; `facingLeft` turns it so that 6 is always towards the way the player faces), then the buttons
// (LP MP HP LK MK HK)
std::string Notation(uint16_t held, bool facingLeft = false);

// The last few things a player held, and for how many frames each
class InputHistory {
public:
    static constexpr int kLines = 7;
    void Push(int player, uint16_t held, bool facingLeft);
    void Clear() {
        *this = InputHistory();
    }
    // Newest first, at the player's side of the picture, from height y down
    void Hud(int player, std::vector<HudLine> &lines, float y = 0.30f, float spacing = 0.062f) const;

private:
    struct Held {
        uint16_t buttons = 0;
        uint16_t frames = 0;
        bool facingLeft = false;
    };
    Held m_held[2][kLines];
};

// --- damage ---

// What hits have done to a player: the last hit, the combo (hits that landed before the player got free), how
// much of it comes back by itself, and the most a combo has done
class Combos {
public:
    // A hit's damage is being taken; `inCombo`: the player had not got free since the hit before
    void Hit(int player, unsigned damage, unsigned comesBack, bool inCombo);
    void Frame(); // after every frame (counts the time since the last hit)
    int Since(int player) const {
        return m_t[player].since;
    }
    unsigned Hits(int player) const {
        return m_t[player].hits;
    }
    void Clear() {
        *this = Combos();
    }
    // Under the player's health bar: the hit or the combo, then (detail) what of it is lost for good, comes back,
    // and the best (only the best when nothing comes back, unless `showBack`); `extra`: a line of the game's own
    // after those (stun, say)
    void Hud(int player, std::vector<HudLine> &lines, float y = 0.17f, bool detail = true, const std::string &extra = {},
             bool showBack = false) const;
    static constexpr int kShownFor = 240; // frames a readout stays up after its last hit

private:
    struct Taken {
        uint16_t last = 0, combo = 0, comboBack = 0, hits = 0, best = 0;
        int32_t since = 1 << 20;
    } m_t[2];
};

// --- the dummy (player 2) ---

// The features a dummy is worked by, with the names every port gives them (Register puts them in the game's
// list, under the category "Dummy"): what it does left alone (one of stand, crouch, jump, record, replay,
// replay any), the recording slot, guarding, what it does while guarding, answering with the recording, and the
// game's own choices (`guardActions`, `heldAction`, `getUp`).
struct DummyOptions {
    // What it can do while guarding a hit (beyond nothing): a name, its description's words, and what it holds at
    // each frame of guarding (from 0), given the stick's "towards the attacker" and "away" bits
    struct Action {
        const char *name;
        uint16_t (*held)(int frame, uint16_t towards, uint16_t away);
        bool whileGuarding = false; // what it holds is added to the guard (buttons pressed while still guarding)
    };
    std::vector<Action> guardActions;
    const char *guardActionsWords = "";
    // What it does while held (being thrown): the throw's own escape, if the game has one
    uint16_t (*heldAction)(int frame, uint16_t towards, uint16_t away) = nullptr;
    const char *heldActionTitle = nullptr, *heldActionWords = nullptr;
    // Ways to get up after a knock-down (beyond where it fell): a name, and what is held at each frame of lying
    std::vector<Action> getUp;
    const char *getUpWords = "How player 2 gets up after being knocked down.";
    int downAfter = 34; // frames of reeling after which the dummy is taken to be knocked down
};
void RegisterDummy(const DummyOptions &options);

// What the dummy needs to know of the fight each frame, from the game's sources
struct DummyView {
    bool fighting = false;      // a round is being fought (the dummy does nothing otherwise)
    Doing doing = Doing::Free;  // what the dummy is doing (Frames::Now(1))
    bool onRight = true;        // the dummy stands right of player 1
    bool attackInRange = false; // something of player 1's is near enough to guard (the game's own test)
    bool guardStanding = false; //   and it must be guarded standing (an overhead, an attack from the air)
    int sinceHit = 1 << 20;     // frames since the dummy was last hit (Combos::Since(1))
    // Knocked down: when the game's sources can tell (knowsDown), from the game's own state; otherwise taken to be
    // reeling for longer than DummyOptions::downAfter
    bool knowsDown = false, down = false;
};

class Dummy {
public:
    // Before a frame: what the game is told the players hold (player 1's controls work player 2 while recording)
    void BeforeFrame(uint16_t &one, uint16_t &two, const DummyView &view);
    void Serialize(Archive &ar);
    // A round starting, or started over: a recording being replayed starts from its beginning
    void Afresh();
    // The HUD's line about recording and replaying, if either is going on
    void Hud(std::vector<HudLine> &lines, float y = 0.17f) const;
    bool Recording() const {
        return m_g.recording;
    }

    static constexpr int kSlots = 3;
    static constexpr size_t kRecordingMost = 20 * 60;
    static constexpr int kReplayPause = 60;
    static constexpr int kGuardAfterHit = 90;

private:
    struct State {
        bool recording = false, replaying = false;
        uint32_t replayAt = 0;
        int32_t replayPause = 0;
        int32_t recordSlot = 0, replaySlot = 0;
        bool reversal = false;
        Doing was = Doing::Free;
        int32_t stunFrames = 0;
        bool inRange = false, guardThis = false;
        uint32_t random = 0x2545F491;
        // (what "at random" chose: the action while this guarding lasts, the escape from this throw, the way up)
        int32_t guardPick = 0, getUpPick = 0;
        bool techPick = false, down = false;
    } m_g;
    std::vector<uint16_t> m_recordings[kSlots];
    uint32_t Random();
    std::vector<uint16_t> &Slot();
    bool StartReplay(bool any);
    uint16_t Turned(uint16_t held, bool onRight) const;
};

// --- boxes ---

enum class BoxKind : uint8_t { Hurt, Attack, Push, Throw, ThrowRange };
// A box in the stage: its middle and half sizes (y upwards from the floor)
struct StageBox {
    BoxKind kind;
    int x, y, halfWidth, halfHeight;
    bool thrown = false; // it belongs to something thrown, not to a player
};
// The HUD's box for one in the stage, with the camera's place (the picture's left edge and bottom in the stage)
// and the picture's line that the floor is on with the camera at the bottom
HudBox ToPicture(const StageBox &box, int cameraX, int cameraY, int floorLine);

} // namespace cps2recomp::fight
