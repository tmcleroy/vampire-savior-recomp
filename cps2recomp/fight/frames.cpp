// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of cps2recomp.
//
// Frame data (fight.h): what both players do at every step of the game (not every picture: at the faster speed
// settings a game takes more than one step in some pictures), and from that, for each move, its startup, active
// and recovery steps, and after a hit or a guarded one the advantage. A meter shows the same, a cell a step.
#include "fight.h"

#include "state.h"

#include <cstdio>
#include <cstring>

namespace cps2recomp::fight {

namespace {
    constexpr int kMeterIdle = 30; // steps with both players free after which the meter starts afresh
    constexpr int kShownFor = 300; // steps a move's numbers stay up
} // namespace

Doing Frames::Classify(int n, const StepView &view) {
    Track &t = m_g.track[n];
    // Frozen by a hit landing: such steps do not count (some animation frames go on through it: those do)
    const bool wasFrozen = t.frozen;
    t.frozen = view.control == StepView::Own && view.frozen;
    if (view.control != StepView::Own) {
        t.inMove = false; // (a move hit out of is not reported)
        if (view.control != StepView::Reeling) {
            return Doing::Held;
        }
        return t.guarded ? Doing::GuardStun : Doing::HitStun;
    }
    if (view.canAct) {
        if (t.inMove) {
            t.inMove = false;
            if (t.hadActive) {
                t.shownStartup = t.startup;
                t.shownActive = t.active;
                t.shownRecovery = static_cast<uint16_t>(t.total - t.lastActiveAt);
                t.shownAt = m_g.step;
            }
        }
        return Doing::Free;
    }
    if (view.jumping && !view.attacking && !t.inMove) {
        return Doing::Air;
    }
    if (!t.inMove) {
        t.inMove = true;
        t.hadActive = false;
        t.total = t.startup = t.active = t.lastActiveAt = 0;
    } else if (wasFrozen) {
        return t.doing;
    }
    t.total++;
    if (view.attackOut) {
        if (!t.hadActive) {
            t.hadActive = true;
            t.startup = t.total;
        }
        t.active++;
        t.lastActiveAt = t.total;
        return Doing::Active;
    }
    return t.hadActive ? Doing::Recovery : Doing::Startup;
}

void Frames::Step(int n, const StepView &view) {
    if (n == 0) {
        m_g.step++;
    }
    Track &t = m_g.track[n];
    const Doing before = t.doing;
    t.doing = Classify(n, view);
    if (t.doing == Doing::Free && before != Doing::Free && t.shownAt == m_g.step && print) {
        fprintf(stderr, "frames: player %d startup %u active %u recovery %u\n", n + 1, t.shownStartup, t.shownActive, t.shownRecovery);
    }
    if (t.doing == Doing::Free && before != Doing::Free) {
        t.freeAt = m_g.step;
        const bool wasHit = t.wasHit;
        t.wasHit = false;
        if (wasHit) {
            // Over a hit: the advantage is the other player's, once that one is free too
            m_g.pending = true;
            m_g.pendingVictim = n;
            m_g.pendingAt = m_g.step;
            m_g.pendingGuarded = before == Doing::GuardStun;
        }
    }
    if (n == 1) {
        BothStepped();
    }
}

void Frames::BothStepped() {
    const Doing doing[2] = {m_g.track[0].doing, m_g.track[1].doing};
    if (m_g.pending) {
        const int attacker = 1 - m_g.pendingVictim;
        if (doing[m_g.pendingVictim] != Doing::Free) {
            m_g.pending = false; // (hit again before the other was free)
        } else if (doing[attacker] == Doing::Free) {
            m_g.pending = false;
            m_g.advantageFor = attacker;
            m_g.advantage = static_cast<int32_t>(m_g.pendingAt) - static_cast<int32_t>(m_g.track[attacker].freeAt);
            m_g.advantageGuarded = m_g.pendingGuarded;
            m_g.advantageAt = m_g.step;
            if (print) {
                fprintf(stderr, "frames: player %d %+d on %s\n", attacker + 1, static_cast<int>(m_g.advantage), m_g.advantageGuarded ? "guard" : "hit");
            }
        }
    }
    // The meter stands still while both players are free, and starts afresh after a while of that
    if (doing[0] == Doing::Free && doing[1] == Doing::Free) {
        if (m_g.idle < kMeterIdle) {
            m_g.idle++;
        }
        return;
    }
    if (m_g.idle >= kMeterIdle) {
        m_g.meterLength = 0;
    }
    m_g.idle = 0;
    if (m_g.meterLength == kMeterCells) {
        for (int n = 0; n < 2; n++) {
            memmove(m_g.meter[n], m_g.meter[n] + 1, kMeterCells - 1);
        }
        m_g.meterLength--;
    }
    for (int n = 0; n < 2; n++) {
        m_g.meter[n][m_g.meterLength] = static_cast<uint8_t>(doing[n]);
    }
    m_g.meterLength++;
}

void Frames::Contact(int victim, bool guarded) {
    m_g.track[victim].guarded = guarded;
    m_g.track[victim].wasHit = true;
}

void Frames::Serialize(Archive &ar) {
    ar.Mark(0x4652414D);
    ar.Pod(m_g);
}

void Frames::Hud(bool numbers, bool meter, std::vector<HudLine> &lines, std::vector<HudBox> &boxes, float y, int top, int width, int left) const {
    if (numbers) {
        for (int n = 0; n < 2; n++) {
            const Track &t = m_g.track[n];
            const float x = n == 0 ? 0.03f : 0.97f;
            const int align = n == 0 ? -1 : 1;
            char text[64];
            if (m_g.advantageFor == n && m_g.step - m_g.advantageAt < kShownFor) {
                snprintf(text, sizeof(text), "%+d on %s", static_cast<int>(m_g.advantage), m_g.advantageGuarded ? "guard" : "hit");
                lines.push_back({x, y, align, text});
            }
            if (t.shownAt && m_g.step - t.shownAt < kShownFor) {
                snprintf(text, sizeof(text), "startup %u  active %u  recovery %u", t.shownStartup, t.shownActive, t.shownRecovery);
                lines.push_back({x, y + 0.065f, align, text});
            }
        }
    }
    if (meter && m_g.meterLength) {
        constexpr int kCell = 6;
        const int start = left + (width - kMeterCells * kCell) / 2;
        static constexpr struct {
            uint8_t r, g, b;
        } kColors[] = {{40, 44, 56},     // free
                       {60, 200, 110},   // startup
                       {240, 60, 60},    // active
                       {70, 120, 250},   // recovery
                       {250, 210, 60},   // reeling from a hit
                       {90, 220, 230},   // ... from a guarded one
                       {150, 150, 160},  // in the air
                       {200, 110, 230}}; // held
        boxes.push_back({start - 2, top - 2, kMeterCells * kCell + 3, 2 * kCell + 3, 10, 12, 18, 200});
        for (int n = 0; n < 2; n++) {
            for (int i = 0; i < m_g.meterLength; i++) {
                const auto &color = kColors[m_g.meter[n][i]];
                boxes.push_back({start + i * kCell, top + n * kCell, kCell - 1, kCell - 1, color.r, color.g, color.b, 255});
            }
        }
    }
}

// --- the move log ---

namespace {
    constexpr uint32_t kAdvantageWait = 60; // steps after a move ends that its advantage may still come in
}

MoveLog::~MoveLog() {
    if (m_file) {
        std::fclose(m_file);
    }
}

void MoveLog::Write(int player, const Frames::Advantage *advantage) {
    Pending &p = m_pending[player];
    if (!m_file && !m_tried) {
        m_tried = true;
        const std::string &settings = SettingsPath();
        const size_t slash = settings.find_last_of('/');
        const std::string path = (slash == std::string::npos ? std::string(".") : settings.substr(0, slash)) + "/frame_data.csv";
        m_file = std::fopen(path.c_str(), "a+");
        if (!m_file) {
            std::fprintf(stderr, "frame data log: cannot write %s\n", path.c_str());
        } else {
            std::fseek(m_file, 0, SEEK_END);
            if (std::ftell(m_file) == 0) {
                std::fprintf(m_file, "character,input,air,move,startup,active,recovery,advantage,on\n");
            }
            std::fprintf(stderr, "frame data log: %s\n", path.c_str());
        }
    }
    if (m_file && p.move.startup) {
        std::fprintf(m_file, "%s,%s,%s,%06X,%u,%u,%u,", p.start.character.c_str(), p.start.input.empty() ? "-" : p.start.input.c_str(),
                     p.start.air ? "yes" : "no", p.start.id & 0xFFFFFF, p.move.startup, p.move.active, p.move.recovery);
        if (advantage) {
            std::fprintf(m_file, "%+d,%s\n", static_cast<int>(advantage->steps), advantage->guarded ? "guard" : "hit");
        } else {
            std::fprintf(m_file, ",\n");
        }
        std::fflush(m_file);
    }
    p = Pending();
}

void MoveLog::Step(int player, const Frames &frames, StartFn start) {
    if (RunningAhead()) {
        return; // (frames that will be taken back: logged when they happen)
    }
    Pending &p = m_pending[player];
    const Doing now = frames.Now(player);
    const bool inMove = now == Doing::Startup || now == Doing::Active || now == Doing::Recovery;
    if (inMove && !p.open) {
        if (p.finished) {
            Write(player, nullptr);
        }
        p.open = true;
        p.start = start(player);
    }
    const Frames::Move move = frames.LastMove(player);
    if (p.open && move.at && move.at == frames.StepCount()) {
        p.open = false;
        p.finished = true;
        p.move = move;
        p.waited = 0;
    }
    const Frames::Advantage advantage = frames.LastAdvantage();
    if (advantage.at != m_advantageAt) {
        m_advantageAt = advantage.at;
        if (advantage.player >= 0 && m_pending[advantage.player].finished) {
            Write(advantage.player, &advantage);
        }
    }
    if (p.finished && ++p.waited > kAdvantageWait) {
        Write(player, nullptr);
    }
}

} // namespace cps2recomp::fight
