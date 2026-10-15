// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy
//
// Vampire Savior: the model viewer, a mode of the menu's "Alternative modes" page: one character alone, on a
// stage or on plain white or black, at a size from a quarter to twenty-five times its own, standing as it
// does in a fight (and moved with player 1's controls), or going through any one of its animations.
//
// It is a round of a two-player match, reached unseen as the stage viewer's is (modes.h), with the second
// player put out of the way: not followed by the camera, out of the picture, and never drawn. The picture
// is made in two parts. The frame's own is the stage with nothing on it. The character is then drawn by
// itself (the board's objects alone, and of them only player 1's and its shadow), and laid over the stage
// or the plain colour at the size wanted, about the place where the character stands. At its own size on a
// stage, what the stage has in front of the fighters is put back in front of the character: the picture is
// drawn a third time as the board would, and where the character's pixels are not what it shows, something
// stands in front of them.
//
// A character's animations are in three tables in the ROM, by the character's number (the game starts one
// at 027EB8, 027EC0 or 027EC8): each a list of how far on from the table an animation is. An animation is
// frames of 0x18 bytes: how many frames of the game it is shown for, what follows it (the next, a jump to
// the frame a long names, or the end), and where its picture is. Showing one here is putting its frame in
// the player's record each frame, with the time left kept high so that the game's own stepping never
// comes round; the game goes on believing the player stands.
#include "colours.h"
#include "modes.h"
#include "state.h"
#include "widescreen.h"

#include <algorithm>
#include <cstdio>
#include <iterator>
#include <string>
#include <vector>

namespace cps2recomp {

namespace {
    using namespace vsav;

    constexpr const char *kModel = "Model viewer";
    constexpr int kStartMost = 9000; // frames after which getting to a round is given up

    // The characters in the menu's order, by their numbers in the game (11 is nobody)
    constexpr const char *kCharacterNames[] = {"Bulleta", "Demitri", "Gallon", "Victor", "Zabel", "Morrigan", "Anakaris", "Felicia",
                                               "Bishamon", "Aulbath", "Sasquatch", "Q-Bee", "Lei-Lei", "Lilith", "Jedah"};
    constexpr int kCharacterNumbers[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 12, 13, 14, 15};
    // The "Background" choice: the twelve stages, then the plain colours
    constexpr int kWhite = kStages, kBlack = kStages + 1;
    // The "Size" choice, in quarters: the character's own size first (a choice's first is what it is when
    // nothing is chosen), then larger, and round to the smaller ones
    constexpr int kSizes[] = {4, 6, 8, 12, 16, 24, 32, 48, 64, 100, 1, 2, 3};
    // The "Animation" choice: standing as in a fight, or one of the three tables
    constexpr uint32_t kAnimationTables[3] = {0x0BCEFA, 0x0BCF7A, 0x0BCE7A};
    constexpr const char *kTableNames[3] = {"A", "B", "C"};
    constexpr int kMostAnimations = 256;
    constexpr int kFramesASecond = 60;
    constexpr int kNamedFor = 120; // frames an animation's name is shown for after it changes
    constexpr int kHeldFirst = 20, kHeldEvery = 8; // frames a direction is held before it repeats, and between repeats
    // The "Speed" choice: a frame of the game in this many pictures of the screen (as the menu's own speeds)
    constexpr int kSlowBy[] = {1, 2, 4, 8};
    constexpr uint32_t kFrameBytes = 0x18, kFramePicture = 0x04, kFrameJump = 0x18;
    constexpr uint32_t kTimer = 0x20;     // a player's record: frames the animation's frame is still shown for
    constexpr uint32_t kActionStep = 0x06; //   the action and its own step (a word): nothing, from its start

    struct State {
        bool on = false;
        bool starting = false;
        int32_t frames = 0;
        int32_t character = 0, background = 0; // the choices the round was started with
        // The animation being shown (0: none, the character stands): which table and which of it, where
        // its first frame and the frame shown are, and how long that one has left
        int32_t table = 0, number = 0;
        uint32_t first = 0, frame = 0;
        int32_t left = 0;
        int32_t named = 0; // frames the animation's name is still shown for
        int32_t stick = 0, stickHeld = 0; // player 1's left or right a frame ago, and for how many frames
        int32_t held = 0; // frames its last frame is still held for before it starts again (the "Repeat after" choice)
        // Where the character stood in the picture a frame ago (the picture a frame shows is the game's of
        // the frame before)
        int32_t anchorX = 0, anchorY = 0;
    } g;
    bool g_fast = false;
    bool g_noShadow = false; // the second player's shadow is not drawn (NoShadow)

    void Fast(bool on) {
        if (on != g_fast) {
            g_fast = on;
            SetFastForward(on);
        }
    }
    void End() {
        if (g.on) {
            g = State();
            Wanted() = MatchWanted();
            Fast(false);
            SetSlowMotion(1);
        }
    }

    uint32_t Read32(Machine &machine, uint32_t address) {
        return (static_cast<uint32_t>(machine.Read16(address)) << 16) | machine.Read16(address + 2);
    }
    bool InRom(uint32_t address) {
        return address >= 0x100000 && address < 0x400000 && !(address & 1);
    }
    // Is this an animation's frame? (Its picture is somewhere in the ROM, and it is shown for a while.)
    bool IsFrame(Machine &machine, uint32_t frame) {
        return InRom(frame) && machine.Read8(frame) != 0 && InRom(Read32(machine, frame + kFramePicture));
    }
    // Where an animation of the character's starts (0: there is none of that number)
    uint32_t AnimationAt(Machine &machine, int character, int table, int number) {
        const uint32_t list = Read32(machine, kAnimationTables[table] + static_cast<uint32_t>(character) * 4);
        if (!InRom(list)) {
            return 0;
        }
        const uint32_t frame = list + machine.Read16(list + static_cast<uint32_t>(number) * 2);
        return IsFrame(machine, frame) ? frame : 0;
    }
    // How many animations a table has for the character: the list is not counted in the ROM, so as many
    // as start with something that is a frame
    int Animations(Machine &machine, int character, int table) {
        int count = 0;
        while (count < kMostAnimations && AnimationAt(machine, character, table, count)) {
            count++;
        }
        return count;
    }

    // Objects by what they are: for the frame's own picture, the stage's alone (modes.h, NotTheStage); for
    // the character's, player 1's own and what it throws, and its shadow
    bool NotTheModel(uint32_t tile, uint16_t attributes) {
        if (IsShadow(tile, attributes)) {
            return false;
        }
        const unsigned set = attributes & 0x1F;
        return set < 5 || set > 15 || ThePanel(tile, attributes);
    }
    // and for the picture as the board would show it, everything but the panel and the second player
    bool NotInAFight(uint32_t tile, uint16_t attributes) {
        const unsigned set = attributes & 0x1F;
        return ThePanel(tile, attributes) || ((kPlayerSets[1] >> set) & 1);
    }

    void StartModel(Machine &machine) {
        End();
        EndModes();
        EndBattle();
        SetFeature("training_mode", false);
        g.on = true;
        g.starting = true;
        g.character = std::clamp(FeatureValue("model_character"), 0, static_cast<int>(std::size(kCharacterNumbers)) - 1);
        g.background = std::clamp(FeatureValue("model_background"), 0, kBlack);
        Wanted().stage = g.background < kStages ? g.background + 1 : 1;
        Wanted().characters[0] = kCharacterNumbers[g.character];
        Wanted().characters[1] = kCharacterNumbers[(g.character + 1) % std::size(kCharacterNumbers)];
        machine.Reset();
    }

    // The menu's "Reset options": every choice of the viewer's page back to what it is when nothing is chosen
    constexpr const char *kChoices[] = {"model_character", "model_background", "model_size", "model_animation",
                                        "model_speed",     "model_repeat",    "model_all",  "model_foreground"};
    void ResetChoices(Machine &) {
        for (const char *choice : kChoices) {
            SetFeatureValue(choice, 0);
        }
    }

    // The second player out of the way: the camera does not follow it (the game's own flag, which also
    // keeps it out of the hit tests and lets it leave the picture), and it stands far off to the right
    void PutAway() {
        SetRam8(Player(1) + kNotFollowed, 0x80);
        SetRam32(Player(1) + kX, (static_cast<uint32_t>(Ram16(Player(0) + kX)) + 0x300) << 16);
    }

    // The animation chosen in the menu, shown: a frame of it put in player 1's record
    // The "Animation" choice is standing, then every animation there could be: 256 of each of the three
    // lists, named by the list's letter and the number (A34). A character has fewer; the ones it has not
    // got are passed over.
    constexpr int kChoicesAList = kMostAnimations, kAnimationChoices = 1 + 3 * kChoicesAList;
    int Count(Machine &machine, int character, int list) {
        static int counts[kCharacters][3];
        static bool counted[kCharacters];
        if (!counted[character]) {
            counted[character] = true;
            for (int n = 0; n < 3; n++) {
                counts[character][n] = Animations(machine, character, n);
            }
        }
        return counts[character][list];
    }
    bool Has(Machine &machine, int character, int choice) {
        return choice >= 1 && choice < kAnimationChoices && (choice - 1) % kChoicesAList < Count(machine, character, (choice - 1) / kChoicesAList);
    }
    // The animation after `choice` (or before it) that the character has, going round from the last to the first
    int Step(Machine &machine, int character, int choice, int by) {
        for (int n = 0; n < kAnimationChoices; n++) {
            choice = 1 + (choice - 1 + by + (kAnimationChoices - 1)) % (kAnimationChoices - 1);
            if (Has(machine, character, choice)) {
                return choice;
            }
        }
        return 0;
    }

    // Which animations show which of the character's palettes, for the colour editor's Line row: found by
    // showing each frame of every animation on a copy of the viewer's machine (a spare: runtime/spare.h), a few
    // frames each frame of the viewer's, and counting the objects the game puts in the list in each of the
    // player's colour sets (its first, kPaletteSlot, is the set's first line). The pictures' own data says
    // which line a piece is drawn in, but not in a form read yet. For each line, the animation with the
    // frame that has the most objects in it.
    constexpr int kScanFramesEach = 8;      // frames of the copy run in a frame of the viewer's
    constexpr int kScanFramesMost = 48;     // frames of an animation looked at, at most
    struct LineScan {
        int character = -1;
        Spare spare;
        int choice = 0;                     // the animation being looked at (0: not begun)
        uint32_t first = 0, frame = 0;
        int frames = 0;
        bool done = false;
        int best[kPalettesInSet] = {};      // for each line, the animation found, and its objects in that line
        int most[kPalettesInSet] = {};
    } g_scan;

    // (in the copy: the objects in each line of the player's, from the list the game has just made)
    void CountLines(const Machine &machine, int counts[kPalettesInSet]) {
        const uint8_t *list = machine.ObjectsToCome();
        const int slot = Ram8(Player(0) + kPaletteSlot);
        for (int n = 0; n < 0x1000; n += 4) {
            const uint16_t y = static_cast<uint16_t>((list[n * 2 + 2] << 8) | list[n * 2 + 3]);
            const uint16_t attributes = static_cast<uint16_t>((list[n * 2 + 6] << 8) | list[n * 2 + 7]);
            if (y >= 0x8000 || attributes >= 0xFF00) {
                break; // (the list's end)
            }
            const int line = (attributes & 0x1F) - slot;
            if (line >= 0 && line < kPalettesInSet) {
                counts[line]++;
            }
        }
    }
    // A frame of the scan (only while the editor is on, and not for the characters whose other lines are
    // elsewhere: kFirstLineOnly)
    void Scan(Machine &machine) {
        const int character = kCharacterNumbers[g.character];
        if (g_scan.character != character) {
            g_scan = LineScan();
            g_scan.character = character;
            g_scan.done = (kFirstLineOnly >> (character & 31)) & 1;
            if (!g_scan.done) {
                Archive ar(g_scan.spare.state, true); // (the copy: the viewer's machine as it is)
                machine.Serialize(ar);
                g_scan.spare.ready = true;
            }
        }
        if (g_scan.done) {
            return;
        }
        RunSpare(machine, g_scan.spare, [](Machine &) { SetRam8(kClock, kClockFull); }, nullptr, [character](Machine &m) {
            for (int n = 0; n < kScanFramesEach && !g_scan.done; n++) {
                // (what the frame shown last made, counted; then the next frame, or the next animation)
                if (g_scan.choice) {
                    int counts[kPalettesInSet] = {};
                    CountLines(m, counts);
                    for (int line = 0; line < kPalettesInSet; line++) {
                        if (counts[line] > g_scan.most[line]) {
                            g_scan.most[line] = counts[line];
                            g_scan.best[line] = g_scan.choice;
                        }
                    }
                    const uint8_t follows = m.Read8(g_scan.frame + kFrameEnd);
                    const uint32_t next = follows & 0x80 ? Read32(m, g_scan.frame + kFrameJump) : follows & 0x40 ? 0 : g_scan.frame + kFrameBytes;
                    if (next > g_scan.frame && IsFrame(m, next) && ++g_scan.frames < kScanFramesMost) {
                        g_scan.frame = next;
                    } else {
                        const int choice = Step(m, character, g_scan.choice, 1);
                        if (choice <= g_scan.choice) { // (gone round: every animation looked at)
                            g_scan.done = true;
                            break;
                        }
                        g_scan.choice = choice;
                        g_scan.first = g_scan.frame = AnimationAt(m, character, (choice - 1) / kChoicesAList, (choice - 1) % kChoicesAList);
                        g_scan.frames = 0;
                    }
                } else {
                    g_scan.choice = Step(m, character, 0, 1);
                    g_scan.first = g_scan.frame = g_scan.choice ? AnimationAt(m, character, (g_scan.choice - 1) / kChoicesAList, (g_scan.choice - 1) % kChoicesAList) : 0;
                    if (!g_scan.first) {
                        g_scan.done = true;
                        break;
                    }
                }
                SetRam32(Player(0) + kFrame, g_scan.frame);
                SetRam32(Player(0) + kTimer, Read32(m, g_scan.frame));
                SetRam8(Player(0) + kTimer, 0x7F);
                m.players[0] = m.players[1] = 0;
                m.RunFrame();
            }
        });
    }

    // The colour editor's question (colours.h, Description::animationForLine)
    int ForLine(int line) {
        if (line <= 0) {
            return 0; // (the body: standing shows all of it)
        }
        if (g_scan.character != kCharacterNumbers[g.character] || !g_scan.done) {
            return -2;
        }
        return line < kPalettesInSet && g_scan.best[line] ? g_scan.best[line] : -1;
    }

    const char *AnimationChoice() {
        return colours::EditorAnimation() ? colours::EditorAnimation() : "model_animation";
    }

    void ShowAnimation(Machine &machine) {
        const int character = kCharacterNumbers[g.character];
        int choice = std::clamp(FeatureValue(AnimationChoice()), 0, kAnimationChoices - 1);
        // (one the character has not got: the next it has)
        if (choice && !Has(machine, character, choice)) {
            choice = Step(machine, character, choice, 1);
            SetFeatureValue(AnimationChoice(), choice);
        }
        // ("Play all" with nothing chosen: the first animation the character has)
        const bool all = FeatureOn("model_all") && !colours::EditorAnimation(); // (the colour editor's own choice is one animation)
        if (all && !choice) {
            choice = Step(machine, character, 0, 1);
            SetFeatureValue("model_animation", choice);
        }
        const uint16_t stick = machine.players[0] & (kLeft | kRight);
        if (choice) {
            // Left and right go to the animation before and the one after: at a press, and while held,
            // after a third of a second, eight a second
            if (stick == kLeft || stick == kRight) {
                g.stickHeld = g.stick == stick ? g.stickHeld + 1 : 0;
                if (g.stickHeld == 0 || (g.stickHeld >= kHeldFirst && (g.stickHeld - kHeldFirst) % kHeldEvery == 0)) {
                    choice = Step(machine, character, choice, stick == kRight ? 1 : -1);
                    SetFeatureValue(AnimationChoice(), choice);
                }
            }
        }
        g.stick = stick;
        const int table = choice ? (choice - 1) / kChoicesAList + 1 : 0, number = choice ? (choice - 1) % kChoicesAList : 0;
        const uint32_t first = choice ? AnimationAt(machine, character, table - 1, number) : 0;
        if (!first) {
            if (g.table) {
                // (back to standing: the game's own "nothing" action from its start sets the animation again)
                SetRam16(Player(0) + kActionStep, 0);
                SetRam8(Player(0) + kTimer, 1);
            }
            g.table = 0;
            g.named = 0;
            return;
        }
        if (table != g.table || number != g.number || first != g.first) {
            g.table = table;
            g.number = number;
            g.first = g.frame = first;
            g.left = machine.Read8(first);
            g.held = 0;
            g.named = kNamedFor;
        } else if (g.held > 0) {
            g.held--; // (between repeats: the last frame stays)
        } else if (--g.left <= 0) {
            // (the frame's time is up: the next, by what the frame says follows it; the end is the start again)
            const uint8_t follows = machine.Read8(g.frame + kFrameEnd);
            uint32_t next = follows & 0x80 ? Read32(machine, g.frame + kFrameJump) : follows & 0x40 ? g.first : g.frame + kFrameBytes;
            if (!IsFrame(machine, next)) {
                next = g.first;
            }
            // (going back, to the start or to an earlier frame, is a repeat: after the wait chosen, if any.
            // The wait is counted once: when it is over the frame's time is up still, and this comes again.)
            // With "Play all", where it would start again the next animation starts instead (the last frame
            // stays for this frame; the next one sees the choice changed and starts it)
            const int wait = std::clamp(FeatureValue("model_repeat"), 0, 10) * kFramesASecond;
            if (next <= g.frame && wait && g.left == 0) {
                g.held = wait;
                g.left = -1;
            } else if (next <= g.frame && all) {
                SetFeatureValue("model_animation", Step(machine, character, choice, 1));
                g.left = 0;
            } else {
                g.frame = next;
                g.left = machine.Read8(next);
            }
        }
        SetRam32(Player(0) + kFrame, g.frame);
        SetRam32(Player(0) + kTimer, Read32(machine, g.frame));
        SetRam8(Player(0) + kTimer, 0x7F);
        machine.players[0] = 0; // (the controls would start moves of the game's own)
    }

    // The animation's name over the picture for a while after it changes (set after the frame, when the
    // other modes' own readouts have been cleared)
    void NameAnimation() {
        if (g.table && g.named > 0) {
            g.named--;
            char text[32];
            snprintf(text, sizeof(text), "%s%d", kTableNames[g.table - 1], g.number);
            SetHud({{0.5f, 0.04f, 0, text}});
        } else {
            SetHud({});
        }
    }

    // The frame's picture made: the stage (or a plain colour), and the character over it at the size wanted
    void Compose(Machine &machine) {
        static std::vector<uint32_t> model;
        const int width = machine.PictureWidth(), height = Video::kHeight;
        uint32_t *picture = machine.Picture();
        model.resize(static_cast<size_t>(width) * height);
        // (the character by itself: the objects alone, the model's only, and marked so that their pixels
        // are told from what is behind them)
        Video &video = machine.video;
        const unsigned layers = video.layerMask;
        const bool marked = video.markObjects;
        const auto hidden = video.hideObject;
        video.layerMask = 1;
        video.markObjects = true;
        video.hideObject = NotTheModel;
        for (int line = 0; line < height; line++) {
            video.RenderLine(line, &model[static_cast<size_t>(line) * width]);
        }
        // (the stage's pieces in front of the character: at its own size, on a stage, where it is not moved
        // aside for the colour editor's panel; the picture as the board would draw it)
        static std::vector<uint32_t> fight;
        const int size = kSizes[std::clamp(FeatureValue("model_size"), 0, static_cast<int>(std::size(kSizes)) - 1)];
        const bool front = size == 4 && g.background < kStages && !colours::EditorShift() && FeatureValue("model_foreground") == 0;
        if (front) {
            fight.resize(model.size());
            video.layerMask = 0xF;
            video.markObjects = false;
            video.hideObject = NotInAFight;
            for (int line = 0; line < height; line++) {
                video.RenderLine(line, &fight[static_cast<size_t>(line) * width]);
            }
        }
        video.layerMask = layers;
        video.markObjects = marked;
        video.hideObject = hidden;

        if (g.background >= kStages) {
            std::fill(picture, picture + static_cast<size_t>(width) * height, g.background == kWhite ? 0xFFFFFFu : 0u);
        }
        // Each pixel of the picture looks at the character's picture, so many quarters to a pixel, about
        // where the character stands (its feet for the smaller sizes; for the larger ones a place in its
        // middle, put in the middle of the picture, so that it is not all legs)
        const bool large = size > 8;
        const int fromX = g.anchorX, fromY = g.anchorY - (large ? 56 : 0);
        const int toX = (large ? width / 2 : g.anchorX) - colours::EditorShift(), toY = large ? height / 2 : g.anchorY;
        auto back = [size](int to, int about, int from) { // (rounded down, for places left of or above `about` too)
            const int quarters = (to - about) * 4 + 2;
            return from + (quarters >= 0 ? quarters / size : -((-quarters + size - 1) / size));
        };
        for (int y = 0; y < height; y++) {
            const int sourceY = back(y, toY, fromY);
            if (sourceY < 0 || sourceY >= height) {
                continue;
            }
            for (int x = 0; x < width; x++) {
                const int sourceX = back(x, toX, fromX);
                if (sourceX >= 0 && sourceX < width) {
                    const uint32_t pixel = model[static_cast<size_t>(sourceY) * width + sourceX];
                    if (pixel & 0x80000000) {
                        const uint32_t shown = front ? fight[static_cast<size_t>(sourceY) * width + sourceX] : pixel;
                        picture[static_cast<size_t>(y) * width + x] = (shown & 0xFFFFFF) == (pixel & 0xFFFFFF) ? pixel & 0xFFFFFF : shown;
                    }
                }
            }
        }
    }

    // (the names of the "Animation" choice)
    const std::vector<const char *> &AnimationNames() {
        static std::vector<std::string> text;
        static std::vector<const char *> names;
        if (names.empty()) {
            text.push_back("Standing (you control it)");
            for (const char *list : kTableNames) {
                for (int n = 0; n < kChoicesAList; n++) {
                    text.push_back(list + std::to_string(n));
                }
            }
            for (const std::string &one : text) {
                names.push_back(one.c_str());
            }
        }
        return names;
    }
} // namespace

void StartModelViewer(Machine &machine) {
    StartModel(machine);
}
// The character being shown, by its number in the game; and the number of a choice of "Character" (-1: none)
int ModelCharacter() {
    return kCharacterNumbers[g.character];
}
int ModelCharacterNumber(int choice) {
    return choice >= 0 && choice < static_cast<int>(std::size(kCharacterNumbers)) ? kCharacterNumbers[choice] : -1;
}

// The animation after `choice` (by 1) or before it (by -1) that the character being shown has, with standing
// (0) before the first and after the last; and the choices' names
int ModelAnimationStep(Machine &machine, int choice, int by) {
    const int character = kCharacterNumbers[g.character];
    const int next = Step(machine, character, choice, by);
    if (!next || (choice && (by > 0 ? next <= choice : next >= choice))) {
        return 0; // (gone round)
    }
    return next;
}
int ModelAnimationForLine(Machine &, int line) {
    return ForLine(line);
}
const std::vector<const char *> &ModelAnimationNames() {
    return AnimationNames();
}

bool ModelViewer() {
    return g.on;
}
void EndModel() {
    End();
}

void ModelBeforeFrame(Machine &machine) {
    // (the second player stands 768 to the right, and the object list counts across in 1024: its shadow
    // would come round into the picture, on the floor, when the character walks to the right)
    const bool noShadow = g.on && !g.starting;
    if (noShadow != g_noShadow) {
        g_noShadow = noShadow;
        NoShadow(machine, noShadow ? 1 : -1);
    }
    if (!g.on) {
        return;
    }
    Wanted().stage = g.background < kStages ? g.background + 1 : 1;
    if (g.starting) {
        Fast(true);
        if (ToRound(machine, g.frames)) {
            g.starting = false;
            Fast(false);
            // (the character in the middle of the stage)
            SetRam32(Player(0) + kX, static_cast<uint32_t>((kStageLeft + kStageRight) / 2) << 16);
        } else if (g.frames > kStartMost) {
            End();
        }
        return;
    }
    machine.players[1] = 0;
    PutAway();
    if (colours::EditorAnimation()) {
        Scan(machine);
    }
    colours::EditorBeforeFrame(machine); // (takes player 1's controls, when it is on)
    SetSlowMotion(kSlowBy[std::clamp(FeatureValue("model_speed"), 0, static_cast<int>(std::size(kSlowBy)) - 1)]);
    SetRam8(kClock, kClockFull);
    ShowAnimation(machine);
}

void ModelFrame(Machine &machine) {
    if (!g.on) {
        return;
    }
    // (the board reset, by the menu or by another mode starting: the viewer is over)
    if (!g.starting && Ram16(kMode) == 0) {
        End();
        return;
    }
    if (g.starting) {
        return;
    }
    // Another character or background chosen in the menu: the viewer starts again with it
    if (std::clamp(FeatureValue("model_character"), 0, static_cast<int>(std::size(kCharacterNumbers)) - 1) != g.character ||
        std::clamp(FeatureValue("model_background"), 0, kBlack) != g.background) {
        StartModel(machine);
        return;
    }
    machine.video.hideObject = NotTheStage;
    Compose(machine);
    NameAnimation();
    colours::EditorFrame(machine);
    g.anchorX = static_cast<int16_t>(Ram16(Player(0) + kX)) - static_cast<int16_t>(Ram16(kCameraX)) + WideStage();
    g.anchorY = 239 - (static_cast<int16_t>(Ram16(Player(0) + kY)) - static_cast<int16_t>(Ram16(kCameraY)));
}

// (Nothing of the viewer is kept in a save state, so that the states made before there was one still load:
// a state loaded while viewing ends the viewer, and what is loaded is the fight it holds.)
void ModelState(Archive &ar) {
    if (!ar.Saving() && !RunningAhead()) { // (not when run-ahead puts the frame back: runtime/game.h)
        End();
    }
}

void RegisterModel() {
    Game &game = TheGame();
    game.pages.push_back({kModel, "Alternative modes",
                          "One character by itself, on a stage or on white or black, at any size, standing or going through any "
                          "of its animations."});
    game.commands.push_back({"model_start", kModel, "Start",
                             "Shows the character chosen below by itself. Player 1's controls move it while it stands. Reset ends it.",
                             [](Machine &machine) {
                                 colours::EndEditor();
                                 StartModel(machine);
                             }});
    game.commands.push_back({"model_reset", kModel, "Reset options",
                             "Puts every choice of this page back: the first character and stage, its own size, standing, full "
                             "speed, repeating at once, one animation only. (While viewing, the viewer goes to that character and stage.)",
                             ResetChoices});
    game.features.push_back({"model_character", kModel, "Character", "Who is shown. Changing it while viewing goes to the new one.", 0,
                             {std::begin(kCharacterNames), std::end(kCharacterNames)}});
    game.features.push_back({"model_background", kModel, "Background",
                             "What is behind the character: one of the stages, or plain white or black. Changing it while viewing "
                             "goes to the new one.",
                             0,
                             {"Feast of the Damned", "Concrete Cave", "Tower of Arrogance", "Red Thirst", "Deserted Chateau", "Abaraya",
                              "Vanity Paradise", "War Agony", "Forever Torment", "Green Scream", "Iron Horse, Iron Terror", "Fetus of God",
                              "White", "Black"}});
    game.features.push_back({"model_foreground", kModel, "Stage in front",
                             "What the stage has in front of the fighters (a table, a fence, a fire): in front of the character "
                             "too, as in a fight, or behind it so that all of it is seen. In front only at the character's own "
                             "size, and not in the colour editor.",
                             0, {"In front", "Behind"}});
    game.features.push_back({"model_size", kModel, "Size", "How large the character is drawn, from a quarter of its own size to 25 times.", 0,
                             {"1x", "1.5x", "2x", "3x", "4x", "6x", "8x", "12x", "16x", "25x", "0.25x", "0.5x", "0.75x"}});
    game.features.push_back({"model_animation", kModel, "Animation",
                             "What the character does: stands as in a fight, moved with player 1's controls, or goes through one "
                             "of its animations over and over. They are named by the game's three lists of them and a number "
                             "(A34); the ones a character has not got are passed over. While one is showing, left and right go "
                             "to the one before and the one after, and its name is shown.",
                             0, AnimationNames()});
    game.features.push_back({"model_speed", kModel, "Speed",
                             "How fast the viewer runs: slow motion for looking at an animation frame by frame. (The menu's own "
                             "speed, and the - and = keys, slow it further.)",
                             0, {"Full", "1/2", "1/4", "1/8"}});
    game.features.push_back({"model_repeat", kModel, "Repeat after",
                             "How long an animation's last frame is held before it starts again.", 0,
                             {"At once", "1 second", "2 seconds", "3 seconds", "4 seconds", "5 seconds", "6 seconds", "7 seconds",
                              "8 seconds", "9 seconds", "10 seconds"}});
    game.features.push_back({"model_all", kModel, "Play all",
                             "Goes through all of the character's animations, one after another: each is shown once (and its "
                             "last frame held for \"Repeat after\") and then the next starts, round from the last to the first. "
                             "Left and right still step through them.",
                             0});
}

} // namespace cps2recomp
