// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of cps2recomp.
//
// The input display, the combo readout, the dummy and the boxes drawn on the picture (fight.h).
#include "fight.h"

#include "state.h"

#include <algorithm>
#include <cstdio>

namespace cps2recomp::fight {

// --- what the players hold ---

std::string Notation(uint16_t held, bool facingLeft) {
    int column = 1, row = 1;
    bool left = held & kStickLeft, right = held & kStickRight;
    if (facingLeft) {
        std::swap(left, right);
    }
    if (left) {
        column = 0;
    } else if (right) {
        column = 2;
    }
    if (held & kStickDown) {
        row = 0;
    } else if (held & kStickUp) {
        row = 2;
    }
    std::string text(1, static_cast<char>('1' + row * 3 + column));
    static const char *const kNames[] = {"LP", "MP", "HP", "LK", "MK", "HK"};
    for (int b = 0; b < 6; b++) {
        if (held & (1u << (4 + b))) {
            text += ' ';
            text += kNames[b];
        }
    }
    return text;
}

void InputHistory::Push(int player, uint16_t held, bool facingLeft) {
    Held *history = m_held[player];
    held &= kSticks | kButtons;
    if (held == history[0].buttons && history[0].frames) {
        if (history[0].frames < 999) {
            history[0].frames++;
        }
        return;
    }
    for (int i = kLines - 1; i > 0; i--) {
        history[i] = history[i - 1];
    }
    history[0] = {held, 1, facingLeft};
}

void InputHistory::Hud(int player, std::vector<HudLine> &lines, float y, float spacing) const {
    const float x = player == 0 ? 0.03f : 0.97f;
    const int align = player == 0 ? -1 : 1;
    const Held *history = m_held[player];
    for (int i = 0; i < kLines && history[i].frames; i++) {
        char text[48];
        const std::string what = Notation(history[i].buttons, history[i].facingLeft);
        if (player == 0) {
            snprintf(text, sizeof(text), "%3u  %s", history[i].frames, what.c_str());
        } else {
            snprintf(text, sizeof(text), "%s  %3u", what.c_str(), history[i].frames);
        }
        lines.push_back({x, y + spacing * static_cast<float>(i), align, text});
    }
}

// --- damage ---

void Combos::Hit(int player, unsigned damage, unsigned comesBack, bool inCombo) {
    Taken &t = m_t[player];
    if (!(t.hits && inCombo)) {
        t.combo = t.comboBack = 0;
        t.hits = 0;
    }
    t.last = static_cast<uint16_t>(damage);
    t.combo = static_cast<uint16_t>(t.combo + damage);
    t.comboBack = static_cast<uint16_t>(t.comboBack + comesBack);
    t.hits++;
    t.best = std::max(t.best, t.combo);
    t.since = 0;
}

void Combos::Frame() {
    for (Taken &t : m_t) {
        if (t.since < (1 << 20)) {
            t.since++;
        }
    }
}

void Combos::Hud(int player, std::vector<HudLine> &lines, float y, bool detail, const std::string &extra, bool showBack) const {
    const Taken &t = m_t[player];
    const float x = player == 0 ? 0.03f : 0.97f;
    const int align = player == 0 ? -1 : 1;
    if (!t.hits || t.since >= kShownFor) {
        if (!extra.empty()) {
            lines.push_back({x, y, align, extra});
        }
        return;
    }
    char text[80];
    if (t.hits > 1) {
        snprintf(text, sizeof(text), "%u hits  %u", t.hits, t.combo);
    } else {
        snprintf(text, sizeof(text), "%u", t.last);
    }
    lines.push_back({x, y, align, text});
    float next = y + 0.06f;
    if (detail) {
        if (t.comboBack || showBack) {
            snprintf(text, sizeof(text), "%u for good, %u back   best %u", t.combo - t.comboBack, t.comboBack, t.best);
        } else {
            snprintf(text, sizeof(text), "best %u", t.best);
        }
        lines.push_back({x, next, align, text});
        next += 0.06f;
    }
    if (!extra.empty()) {
        lines.push_back({x, next, align, extra});
    }
}

// --- the dummy ---

namespace {
    DummyOptions g_options;
}

void RegisterDummy(const DummyOptions &options) {
    const size_t first = TheGame().features.size();
    g_options = options;
    Game &game = TheGame();
    game.features.push_back({"dummy_stand", "Dummy", "Stands", "Player 2 stands still, whatever its controls do.", kFeatureExclusive});
    game.features.push_back({"dummy_crouch", "Dummy", "Crouches", "Player 2 crouches.", kFeatureExclusive});
    game.features.push_back({"dummy_jump", "Dummy", "Jumps", "Player 2 jumps on the spot, again and again.", kFeatureExclusive});
    game.features.push_back({"dummy_record", "Dummy", "Record",
                             "Player 1's controls work player 2, and what they do is recorded in the slot chosen (20 seconds at "
                             "most). Switch to Replay when done.",
                             kFeatureExclusive});
    game.features.push_back({"dummy_replay", "Dummy", "Replay", "Player 2 does what was recorded in the slot chosen, over and over.",
                             kFeatureExclusive});
    game.features.push_back({"dummy_replay_any", "Dummy", "Replay any slot",
                             "Player 2 does what was recorded, over and over, each time from a slot picked at random.", kFeatureExclusive});
    game.features.push_back({"dummy_slot", "Dummy", "Recording slot", "Which of three recordings Record makes and Replay plays.", 0,
                             {"1", "2", "3"}});
    game.features.push_back({"dummy_guard", "Dummy", "Guards",
                             "Player 2 guards player 1's attacks, standing or crouching as each needs: always; only for a moment "
                             "after being hit (to see whether a combo really is one); or one attack in two, at random.",
                             0, {"Off", "Always", "After a hit", "At random"}});
    if (!options.guardActions.empty()) {
        std::vector<const char *> names = {"Nothing"};
        for (const auto &action : options.guardActions) {
            names.push_back(action.name);
        }
        names.push_back("At random");
        game.features.push_back({"dummy_guard_action", "Dummy", "While guarding", options.guardActionsWords, 0, names});
    }
    game.features.push_back({"dummy_reversal", "Dummy", "Answers with the recording",
                             "Player 2 plays the recording once as soon as it can act after guarding, after being hit (or knocked "
                             "down, or thrown), or both.",
                             0, {"Off", "After guarding", "After being hit", "After either"}});
    game.features.push_back({"dummy_reversal_random", "Dummy", "Answers now and then",
                             "With an answer chosen: player 2 answers only one time in two, with any slot that has a recording, "
                             "so you cannot learn when or how it will."});
    if (options.heldAction) {
        game.features.push_back({"dummy_tech", "Dummy", options.heldActionTitle, options.heldActionWords, 0, {"Off", "Always", "At random"}});
    }
    if (!options.getUp.empty()) {
        std::vector<const char *> names = {"Where it fell"};
        for (const auto &action : options.getUp) {
            names.push_back(action.name);
        }
        names.push_back("At random");
        game.features.push_back({"dummy_getup", "Dummy", "Gets up", options.getUpWords, 0, names});
    }
    // (all of them training aids: the dummy is player 2 only in a training session)
    for (size_t n = first; n < TheGame().features.size(); n++) {
        TheGame().features[n].flags |= kFeatureTraining;
    }
}

uint32_t Dummy::Random() {
    m_g.random ^= m_g.random << 13;
    m_g.random ^= m_g.random >> 17;
    m_g.random ^= m_g.random << 5;
    return m_g.random;
}

std::vector<uint16_t> &Dummy::Slot() {
    return m_recordings[std::clamp(FeatureValue("dummy_slot"), 0, kSlots - 1)];
}

// A recording keeps the stick as it would be with the dummy standing on the right, so that it works from either
// side: this turns left and right about when the dummy stands on the left (both ways the same)
uint16_t Dummy::Turned(uint16_t held, bool onRight) const {
    const uint16_t sides = held & (kStickLeft | kStickRight);
    if (!onRight && (sides == kStickLeft || sides == kStickRight)) {
        held ^= kStickLeft | kStickRight;
    }
    return held;
}

bool Dummy::StartReplay(bool any) {
    m_g.replaySlot = std::clamp(FeatureValue("dummy_slot"), 0, kSlots - 1);
    if (any) {
        int filled[kSlots], count = 0;
        for (int n = 0; n < kSlots; n++) {
            if (!m_recordings[n].empty()) {
                filled[count++] = n;
            }
        }
        if (count) {
            m_g.replaySlot = filled[Random() % static_cast<uint32_t>(count)];
        }
    }
    m_g.replayAt = 0;
    return !m_recordings[m_g.replaySlot].empty();
}

void Dummy::Afresh() {
    m_g.replayAt = 0;
    m_g.replayPause = 0;
    m_g.reversal = false;
    m_g.was = Doing::Free;
    m_g.stunFrames = 0;
    m_g.inRange = m_g.guardThis = false;
    m_g.down = false;
}

void Dummy::BeforeFrame(uint16_t &one, uint16_t &two, const DummyView &view) {
    const bool record = FeatureOn("dummy_record");
    const bool replayAny = FeatureOn("dummy_replay_any"), replay = FeatureOn("dummy_replay") || replayAny;
    if (record && !m_g.recording) {
        Slot().clear(); // (switched on: a new recording)
    }
    if (!record && m_g.recording) {
        // (switched off: without the standing about before the first press and after the last, which is only
        // the time it took to work the menu)
        std::vector<uint16_t> &made = m_recordings[m_g.recordSlot];
        while (!made.empty() && made.back() == 0) {
            made.pop_back();
        }
        size_t lead = 0;
        while (lead < made.size() && made[lead] == 0) {
            lead++;
        }
        made.erase(made.begin(), made.begin() + static_cast<std::ptrdiff_t>(lead));
    }
    if (replay && !m_g.replaying) {
        StartReplay(replayAny);
        m_g.replayPause = 0;
    }
    m_g.recording = record;
    m_g.replaying = replay;
    if (record) {
        m_g.recordSlot = std::clamp(FeatureValue("dummy_slot"), 0, kSlots - 1);
    }
    if (!view.fighting) {
        return;
    }
    constexpr uint16_t kHeld = kSticks | kButtons;
    const uint16_t keep = two & ~kHeld;
    if (record) {
        // Player 1's controls work player 2, and are kept
        if (Slot().size() < kRecordingMost) {
            Slot().push_back(Turned(one & kHeld, view.onRight));
        }
        two = static_cast<uint16_t>(keep | (one & kHeld));
        one &= static_cast<uint16_t>(~kHeld);
        return;
    }
    const uint16_t away = view.onRight ? kStickRight : kStickLeft, towards = view.onRight ? kStickLeft : kStickRight;

    // What the dummy has just come out of
    const Doing doing = view.doing;
    const bool reeling = doing == Doing::HitStun || doing == Doing::GuardStun || doing == Doing::Held;
    m_g.stunFrames = reeling ? m_g.stunFrames + 1 : 0;
    const int reversal = FeatureValue("dummy_reversal"); // 1 after guarding, 2 after being hit, 3 either
    if (doing == Doing::Free && m_g.was != Doing::Free && !m_g.reversal) {
        const bool guarded = m_g.was == Doing::GuardStun;
        const bool hit = m_g.was == Doing::HitStun || m_g.was == Doing::Held;
        const bool sometimes = FeatureOn("dummy_reversal_random");
        if (((guarded && (reversal & 1)) || (hit && (reversal & 2))) && (!sometimes || (Random() & 0x100)) &&
            StartReplay(replayAny || sometimes)) {
            m_g.reversal = true;
        }
    }
    m_g.was = doing;

    // What it does left alone
    uint16_t held = two & kHeld; // (nothing chosen: player 2's own controls)
    bool busy = false;           // doing something of a recording's
    if (m_g.reversal) {
        const std::vector<uint16_t> &recording = m_recordings[m_g.replaySlot];
        held = Turned(recording[m_g.replayAt], view.onRight);
        busy = true;
        if (++m_g.replayAt >= recording.size()) {
            m_g.reversal = false;
            m_g.replayAt = 0;
            m_g.replayPause = kReplayPause;
        }
    } else if (replay) {
        const std::vector<uint16_t> &recording = m_recordings[m_g.replaySlot];
        held = 0;
        if (recording.empty()) {
            StartReplay(replayAny);
        } else if (m_g.replayPause > 0) {
            m_g.replayPause--;
        } else {
            held = Turned(recording[m_g.replayAt], view.onRight);
            busy = true;
            if (++m_g.replayAt >= recording.size()) {
                StartReplay(replayAny);
                m_g.replayPause = kReplayPause;
            }
        }
    } else if (FeatureOn("dummy_crouch")) {
        held = kStickDown;
    } else if (FeatureOn("dummy_jump")) {
        held = kStickUp;
    } else if (FeatureOn("dummy_stand")) {
        held = 0;
    }

    // Guarding: when the game's own test says something is near enough to guard: away from player 1, standing or
    // crouching as the attack needs. (Holding away all the time would walk it into the corner.)
    const int guard = FeatureValue("dummy_guard"); // 1 always, 2 after the first hit, 3 at random
    const bool inRange = guard && !busy && view.attackInRange;
    if (inRange && !m_g.inRange) {
        m_g.guardThis = guard == 1 || (guard == 2 && view.sinceHit < kGuardAfterHit) || (guard == 3 && (Random() & 0x100));
    }
    m_g.inRange = inRange;
    if (inRange && (m_g.guardThis || doing == Doing::GuardStun)) {
        held = static_cast<uint16_t>(away | (view.guardStanding ? 0 : kStickDown));
    }
    // While guarding a hit
    if (doing == Doing::GuardStun) {
        const int actions = static_cast<int>(g_options.guardActions.size());
        int action = FeatureValue("dummy_guard_action");
        if (action == actions + 1) {
            // (at random: one of them, or nothing, chosen as each guarding begins)
            if (m_g.stunFrames == 1) {
                m_g.guardPick = static_cast<int32_t>(Random() % static_cast<uint32_t>(actions + 1));
            }
            action = m_g.guardPick;
        }
        if (action >= 1 && action <= actions) {
            const DummyOptions::Action &chosen = g_options.guardActions[action - 1];
            const uint16_t does = chosen.held(m_g.stunFrames, towards, away);
            held = chosen.whileGuarding ? static_cast<uint16_t>(held | does) : does;
        }
    }
    // Held (thrown)
    if (doing == Doing::Held && m_g.stunFrames == 1) {
        m_g.techPick = (Random() & 0x100) != 0; // (at random: decided as each throw begins)
    }
    const int tech = FeatureValue("dummy_tech");
    if (doing == Doing::Held && g_options.heldAction && (tech == 1 || (tech == 2 && m_g.techPick))) {
        held = g_options.heldAction(m_g.stunFrames, towards, away);
    }
    // Knocked down
    const bool down = view.knowsDown ? view.down : (doing == Doing::HitStun && m_g.stunFrames > g_options.downAfter);
    const int ways = static_cast<int>(g_options.getUp.size());
    if (down && !m_g.down) {
        m_g.getUpPick = static_cast<int32_t>(Random() % static_cast<uint32_t>(ways + 1)); // (at random: as it goes down)
    }
    m_g.down = down;
    if (down) {
        int getUp = FeatureValue("dummy_getup");
        if (getUp == ways + 1) {
            getUp = m_g.getUpPick;
        }
        if (getUp >= 1 && getUp <= ways) {
            held = g_options.getUp[getUp - 1].held(m_g.stunFrames, towards, away);
        }
    }
    two = static_cast<uint16_t>(keep | held);
}

void Dummy::Serialize(Archive &ar) {
    ar.Mark(0x44554D59);
    ar.Pod(m_g);
    for (auto &recording : m_recordings) {
        ar.Vec(recording);
    }
}

void Dummy::Hud(std::vector<HudLine> &lines, float y) const {
    if (!m_g.recording && !m_g.replaying) {
        return;
    }
    char text[48];
    const std::vector<uint16_t> &recording = m_recordings[m_g.recording ? m_g.recordSlot : m_g.replaySlot];
    if (m_g.recording) {
        snprintf(text, sizeof(text), "REC %d  %.1f s", m_g.recordSlot + 1, static_cast<double>(recording.size()) / 60.0);
    } else if (recording.empty()) {
        snprintf(text, sizeof(text), "nothing recorded");
    } else {
        snprintf(text, sizeof(text), "PLAY %d  %.1f / %.1f s", m_g.replaySlot + 1, m_g.replayAt / 60.0, static_cast<double>(recording.size()) / 60.0);
    }
    lines.push_back({0.5f, y, 0, text});
}

// --- boxes ---

HudBox ToPicture(const StageBox &box, int cameraX, int cameraY, int floorLine) {
    HudBox shown;
    shown.x = box.x - box.halfWidth - cameraX;
    shown.y = floorLine - (box.y - cameraY) - box.halfHeight;
    shown.width = box.halfWidth * 2;
    shown.height = box.halfHeight * 2;
    switch (box.kind) {
    case BoxKind::Attack: shown.r = 255, shown.g = box.thrown ? 140 : 40, shown.b = 40; break;
    case BoxKind::Push: shown.r = 60, shown.g = 220, shown.b = 90; break;
    case BoxKind::Throw: shown.r = 250, shown.g = 230, shown.b = 60; break;
    case BoxKind::ThrowRange: shown.r = 250, shown.g = 230, shown.b = 60, shown.fill = 0; break;
    default: shown.r = 70, shown.g = 130, shown.b = 255; break;
    }
    return shown;
}

} // namespace cps2recomp::fight
