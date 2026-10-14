// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy
//
// Vampire Savior: names for what is known of the game's memory (found by experiment: PROGRESS.md says how).
#pragma once

#include "game.h"
#include "lift.h"

#include <cstdint>

namespace vsav {

// Where the game is: words from A5, each a step (in twos) of the one before. The first is 2 once a game is
// being played; the next 2 while the players choose their characters and 4 during the match
constexpr uint32_t kMode = 0xFF8000, kGameStep = 0xFF8004;
enum : uint16_t { kGameChoosing = 2, kGameMatch = 4 };
// ... then, in a match, 4 while a round is on, and under that 6 while it is being fought
constexpr uint32_t kMatchStep = 0xFF8008, kRoundStep = 0xFF800C;
enum : uint16_t { kMatchRound = 4, kRoundFought = 6 };
// The stage: the players can stand from one of these to the other, and the picture's left edge (the camera)
// goes from one of these to the other. A round starts with the players at kStartX.
constexpr uint16_t kStageLeft = 280, kStageRight = 1000, kCameraLeft = 256, kCameraRight = 640;
constexpr uint16_t kStartX[2] = {554, 728};
// Everywhere the camera's place across the stage is kept (the last is what the picture is drawn from)
constexpr uint32_t kCameraCopies[] = {0xFF8026, 0xFF82A0, 0xFF82A8, 0xFF8390, 0xFF8290};
// The stage a match is fought on: a word, the stage's number doubled (kStages of them). The game chooses it
// at kChooseStage when a match is made: between two players at random from sixteen for player 2's
// character, in a one-player game the next opponent's own. kPlayersThen is who was playing then, as kPlayersIn.
constexpr uint32_t kStage = 0xFF8100, kChooseStage = 0x00AE7C, kPlayersThen = 0xFF80AC;
constexpr int kStages = 12; // in the order of their numbers: Feast of the Damned (the castle's hall), Concrete Cave (the
// subway), Tower of Arrogance, Red Thirst (the desert), Deserted Chateau, Abaraya, Vanity Paradise, War
// Agony (the wooden town), Forever Torment, Green Scream, Iron Horse Iron Terror (the train), Fetus of God
// A stage's scroll screens have a record each. The one of 8-pixel tiles (the furthest back on most stages):
// where its picture's left edge is in the stage (a word: the scroll register is this less 64), and a byte
// that is not zero when its picture is wider than the map's 512 columns and the game draws it into the map
// as the camera moves, a 32-pixel strip at a time (routines 01EE22-01F06E). It is drawn from the stage's
// plan: eight bytes, two rows of four 256-pixel blocks, the lower row second (copied here from the ROM when
// the stage is made); a block is 8 x 8 words in the ROM, down then across, each the number of a 32-pixel
// cell; a cell is 4 x 4 tiles, down then across, two words a tile as the map has them.
constexpr uint32_t kBackLayer = 0xFF8200, kLayerDrawnAsItGoes = 0x03, kLayerX = 0x20;
// (the three planes' records follow one another: 8-, 16-, 32-pixel tiles; +10 and +14 are where the plane is
// shown, across and up, which is what an object riding on it is placed against)
constexpr uint32_t kLayerSize = 0x80, kLayerShownX = 0x10, kLayerShownY = 0x14;
constexpr uint32_t kBackPlan = 0xFF4302, kBackBlocks = 0x35AD18, kBackCells = 0x35C918;
// Who is playing: bit 0 player 1, bit 1 player 2
constexpr uint32_t kPlayersIn = 0xFF808D;
// In a player's record while choosing: a step (0 the character, 2 the speed and guard choices, 4 done)
constexpr uint32_t kChoosingStep = 0x04;
enum : uint8_t { kChosen = 4 };
// Sounds the game wants made wait in a ring of 256 entries of sixteen bytes (three longs are used: the first
// is the sound's number, a tune's or an effect's) until the sound CPU has taken the one before. The game adds
// at kSoundQueueEnd (a word: bytes into the ring) and sends from the word before it.
constexpr uint32_t kSoundQueue = 0xFF0E0E, kSoundQueueEnd = 0xFF1E0E;
// With no coin in, kMode steps through the game's show (modes.cpp names its parts). A byte counts its
// fights, two for each: it picks who fights next and where, and which ranking follows.
constexpr uint32_t kAttractFight = 0xFF1E2A;
// Coins paid for and not yet used
constexpr uint32_t kCredits = 0xFF8076;
// The round's clock: seconds left (99 at most), and the part of a second
constexpr uint32_t kClock = 0xFF8109;
constexpr uint8_t kClockFull = 99;

// A player's record; the second player's follows the first's
constexpr uint32_t kPlayer1 = 0xFF8400, kPlayerSize = 0x400;
// --- objects: the players and the things they throw have records laid out alike ---

constexpr uint32_t kActive = 0x00;        // byte: not zero when the record is in use
constexpr uint32_t kFacesRight = 0x0B;    // byte: not zero when turned about (box offsets go the other way)
constexpr uint32_t kFrame = 0x1C;         // long: the animation frame being shown (in the ROM)
constexpr uint32_t kFrameAttack = 0x0A;   //   in a frame, a byte: the number of its attack box, or 0
// The object's boxes are in tables in the ROM, eight bytes a box; the record has the tables' addresses and,
// for the boxes it can be hit in and its pushing box, the numbers of the ones in use now (0: none)
constexpr uint32_t kHeadBoxes = 0x80, kBodyBoxes = 0x84, kLegBoxes = 0x88, kAttackBoxes = 0x8C, kPushBoxes = 0x90; // longs
constexpr uint32_t kHeadBox = 0x94, kBodyBox = 0x95, kLegBox = 0x96, kPushBox = 0x97;                               // bytes
constexpr uint32_t kAttackBoxSize = 0x20; // (an attack's entry is a box and then what the attack does)

// The four player records (the two players, and two more for the helpers some characters have)
constexpr int kPlayerRecords = 4;
// The things thrown (fireballs and the like): thirty-two records after the players'
constexpr uint32_t kThrown = 0xFF9400, kThrownSize = 0x100;
constexpr int kThrownRecords = 32;

// Where the picture's left edge and bottom are in the stage
constexpr uint32_t kCameraX = 0xFF8290, kCameraY = 0xFF8294; // words

// Health: what a player has left, and what it can come back to (a hit takes some for good and some that
// comes back by itself; the bar shows both). Below zero: knocked out.
constexpr uint32_t kHealthCeiling = 0x50; // word
constexpr uint32_t kHealth = 0x52;        // word
constexpr uint16_t kHealthFull = 0x120;
constexpr uint32_t kAction = 0x06;        // byte: what the player is doing
enum : uint8_t { kActionNone = 0x00, kActionCrouch = 0x02, kActionJump = 0x06, kActionAttack = 0x0A };
constexpr uint32_t kX = 0x10;             // word: where the player stands, across the stage
constexpr uint32_t kY = 0x14;             // word: height; kGround when on the floor
constexpr uint16_t kGround = 0x28;
constexpr uint32_t kAttacking = 0x105;    // byte: not zero while an attack of the player's is out
constexpr uint32_t kStocks = 0x109;      // byte: full special gauges kept (99 at most)
constexpr uint32_t kGauge = 0x10A;       // word: the gauge being filled
constexpr uint8_t kStocksFull = 99;

// The game's variables are reached from A5 (FF8000). The damage of the hit being dealt, in two parts: what
// is lost for good, and what is lost besides but comes back
constexpr int32_t kDamageForGood = -0x4BBE, kDamageBesides = -0x4BBC; // words, from A5

// What a hit test leaves for the code that deals with the hit (words unless said), from A5:
constexpr int32_t kHitAttacker = -0x4BCA, kHitVictim = -0x4BC8;   // the two records' addresses (low words)
constexpr int32_t kHitAttacker2 = -0x4BC6, kHitVictim2 = -0x4BC4; //   (kept twice)
constexpr int32_t kHitVictimBoxes = -0x4BC2;                       // long: the victim's box numbers: head, body, legs, push
constexpr int32_t kOverlapDepth = -0x4BB8;                         // how far the two boxes reach into each other, across
constexpr int32_t kOverlapAttackX = -0x4BB6, kOverlapAttackY = -0x4BB4; // the middles of the two boxes last tested
constexpr int32_t kOverlapVictimX = -0x4BB2, kOverlapVictimY = -0x4BB0;
// For each part of the victim hit (head, body, legs): the two boxes' middles and how far apart they are
constexpr int32_t kHitMiddles[3] = {-0x4BAE, -0x4BA6, -0x4B9E};    // four words each: attack x, y, victim x, y
constexpr int32_t kHitDistance[3] = {-0x4B96, -0x4B94, -0x4B92};   // (FFFF: that part was not hit)
constexpr int32_t kHitParts = -0x4B90;                             // byte: bit 0 head, 1 body, 2 legs

// More of an object's record, as the hit test uses it
constexpr uint32_t kSide = 0x70;          // byte: which side the object is on (a player's own things share it)
constexpr uint32_t kHurtBoxes = 0x68;     // long: where the numbers of the boxes it can be hit in are (in the ROM)
constexpr uint32_t kHitBy = 0x6C;         // bytes, one for each side: the number of the attack that last hit it
// In an animation frame:
constexpr uint32_t kFrameKind = 0x0B, kFrameStrength = 0x14, kFrameCanBeHit = 0x08;
// In an attack's entry, after its box:
constexpr uint32_t kAttackNumber = 0x10, kAttackKind = 0x17;

// Routines of the game (addresses in the program)
constexpr uint32_t kBoxesOverlap = 0x019172; // do the boxes at A3 (of the object A6) and A2 (of A1) overlap?
constexpr uint32_t kHitTest = 0x017FF4;      // does the attack of the object A6 hit the object A1?
constexpr uint32_t kClearHit = 0x01923C;
constexpr uint32_t kAttackAgainst = 0x018054;       // the hit test from where the victim is looked at
constexpr uint32_t kAllHitTests = 0x017F0C;         // a frame's hit tests: everything against everything
constexpr uint32_t kPlayersAgainstPlayers = 0x017F22;
constexpr uint32_t kThrownAgainstPlayers = 0x0193B8;
constexpr uint32_t kThrownAgainstThrown = 0x01A728; // (the game's own code still)
constexpr uint32_t kPushApart = 0x01926A;           // keeps the two players from standing in each other
constexpr uint32_t kReadControls = 0x001576;        // (in the frame interrupt) reads the control panel

// More for the hit tests:
constexpr uint32_t kCanHit = 0x108;       // byte: which of the four player records this one's attacks can hit (a bit each)
constexpr int32_t kThrownInUse = -0x4EF2; // from A5: the end of a list of the records of things thrown now in use
constexpr int32_t kThrownInUseCount = 0x79C5; //   (16 bits each, the list going downwards), and how many (a byte)
constexpr int32_t kThrownListAt = -0x4B8A;    // long: where the hit tests have got to in that list
constexpr int32_t kNoHitTests = 0x137;        // byte: not zero: no hit tests this frame
constexpr uint32_t kPushCount = 0x11D;        // byte: the players' pushing boxes overlapped this frame

// The control panel as the game keeps it (from A5): a word a player, the buttons in the high byte (bits 0-2
// the punches, light to hard; 4-6 the kicks) and the stick in the low (bit 0 right, 1 left, 2 down, 3 up)
constexpr int32_t kControls[2] = {0x58, 0x5C};        // now
constexpr int32_t kControlsBefore[2] = {0x5A, 0x5E};  // the frame before
constexpr int32_t kStartsAndCoins = 0x60, kService = 0x64; // longs: a byte a frame, the newest first
constexpr uint32_t kTakeDamage = 0x018AB0; // takes the hit's damage off the health of the player A1 points at
constexpr uint32_t kRoundFighting = 0x0097DC; // a frame of a round being fought: the clock, and who has won
constexpr uint32_t kGuardRange = 0x027694;    // is an attack near enough to the player A6 for it to guard?
constexpr uint32_t kGuardRangeAttack = 0x0276A6; //   the same, whatever the player holds

// The round, from A5 (bytes unless said):
constexpr int32_t kRoundClock = 0x109;       // seconds left
constexpr int32_t kRoundClockPart = 0x10A;   // frames left of the second
constexpr int32_t kRoundResult = 0x10C;      // 0: still going; 1: player 1 has won; 2: player 2; 3: neither
constexpr int32_t kRoundTimeOver = 0x10D;    // not zero: the clock ran out
constexpr int32_t kRoundClockStopped = 0x15D;
constexpr int32_t kRoundPaused = 0x8A;

// Who a player is: the character's number (kept twice), which of its sets of colours, and whether the game
// itself plays it (kComputer 1). kPlays is 2 for a player that is in the game: at 0 the panel counts down
// for that side to come back in.
constexpr uint32_t kCharacter = 0x382, kCharacterAgain = 0x3E0, kColours = 0x3AE; // bytes
constexpr uint32_t kPlays = 0x380, kComputer = 0x3B4;                              // bytes
constexpr int kCharacters = 16; // numbers 0-15, of which 11 is nobody
inline constexpr const char *kCharacterNames[kCharacters] = {"Bulleta", "Demitri", "Gallon", "Victor", "Zabel", "Morrigan",
                                                            "Anakaris", "Felicia", "Bishamon", "Aulbath", "Sasquatch", nullptr,
                                                            "Q-Bee", "Lei-Lei", "Lilith", "Jedah"};
// The hidden characters' numbers (choosing.cpp): Gallon's and Bishamon's with bit 4
constexpr uint8_t kDarkGallon = 0x12, kOboroBishamon = 0x18;
// A character's colours, in the ROM: a long a character (by its number, 0-31) at kColourSets points to its
// sets, kColourSetSize bytes each: four palettes of 16 colours (words, 4 bits each of red, green and blue;
// the game keeps the brightness the palette RAM had). kColours picks the set: 0-5 the button the character was
// chosen with (LP MP HP LK MK HK), 6 two or more punches, 7 two or more kicks, 8-9 chosen for auto guard. The
// game keeps where the player's set is at kColourSet (a long) and copies its palettes from there into the
// palette RAM (02AD3C: the first palette at A0; 02ADC8 a palette of the set by number).
constexpr uint32_t kColourSets = 0x38C198, kColourSetSize = 0x80, kColourSet = 0x3A4;
constexpr int kColourSetsEach = 10, kPalettesInSet = 4, kColoursInPalette = 16;
// More of a character's colours: a long a character (by its number) at kEffectColours points to
// kEffectPalettes palettes of 16 colours for what is done to it (poisoned, a shock, burned, the mummy's curse
// and so on), each by the set: 02AD20 copies palette D0 of the character's (kColours added to the effect's
// first: 02ABC0).
constexpr uint32_t kEffectColours = 0x38C218;
constexpr int kColourNumbers = 32; // entries of the two tables (kEffectColours follows kColourSets' last)
// Where a player's colours go: the palettes the board shows are taken from a buffer in the video RAM (its
// place a long at kPaletteBuffer: 90C000), 16 words a palette; the player's first is its byte kPaletteSlot.
// The game puts the set's four palettes from there on, but for the characters whose bit is set in
// kFirstLineOnly (by number; 02ADB2), whose other three go elsewhere when their effects need them.
constexpr uint32_t kPaletteBuffer = 0xFF80D0, kPaletteSlot = 0x18B;
constexpr uint32_t kFirstLineOnly = 0xEF92EF96;
constexpr int kEffectPalettes = 66;
// How fast a player is moving, and how that changes each frame (longs: 65536 a pixel)
constexpr uint32_t kSpeedX = 0x40, kSpeedY = 0x44, kPullX = 0x48, kPullY = 0x4C;
// The camera's record: where it wants the picture's left edge and bottom (words), the ends it stops at
constexpr uint32_t kCameraRecord = 0xFF8280;
constexpr uint32_t kCameraWantsX = 0x20, kCameraWantsY = 0x24, kCameraMostX = 0x5C, kCameraMostY = 0x5E;
constexpr uint32_t kCameraEndLeft = 0x44, kCameraEndRight = 0x46; // words: kCameraLeft and kCameraRight on a stage
constexpr uint32_t kCameraEndTop = 0x40, kCameraEndBottom = 0x42; // words: the ends it stops at, up and down
// ... worked out each frame at these, which end by handing the place (D0) to the code at the second address
// (camera.cpp)
constexpr uint32_t kCameraAcross = 0x01E36E, kCameraAcrossSet = 0x01E450;
constexpr uint32_t kCameraUp = 0x01E3F2, kCameraUpSet = 0x01E4A0;
// The players' outer edges across the stage (D0 the left one, D1 the right), and their heights (D0 the
// lower, D1 the higher), for the camera
constexpr uint32_t kPlayersAcross = 0x01E8F4, kPlayersUp = 0x01E962;
constexpr uint32_t kCameraHeld = 0x60; // byte: not zero: the camera does not follow the players
// More of a scroll screen's record (the camera's is one: kLayerX and kLayerY are its kCameraWantsX and Y):
constexpr uint32_t kLayerY = 0x24;                             // word: where its picture's bottom is in the stage
constexpr uint32_t kLayerStartX = 0x08, kLayerStartY = 0x0C;   // words: its place when the camera is at its own start
constexpr uint32_t kLayerShakeX = 0x50, kLayerShakeY = 0x54;   // words: added to its place (a stage shaking)
constexpr uint32_t kLayerBeforeX = 0x28, kLayerBeforeY = 0x2C; // words: where it was the frame before
constexpr uint32_t kLayerNudgeX = 0x58, kLayerNudgeY = 0x5A;   // words: added to the scroll registers only
// bytes: which way it went this frame (across: 0C right, 08 left; up: 00 up, 04 down), and the line it last
// drew a strip of the map for (a bit, kept turned over)
constexpr uint32_t kLayerGoingX = 0x3B, kLayerGoingY = 0x3A, kLayerStripX = 0x38, kLayerStripY = 0x39;
constexpr uint32_t kFarLayer = 0xFF8300; // the record of the scroll screen of 32-pixel tiles
// The row scroll table's record follows the three: where the table is (a long), the row of it that is the
// stage's own plane at the camera's height (a word: bytes into the table), and a word added to the camera's
// place for the planes that go at a rate of it
constexpr int32_t kRowsTable = 0x380, kRowsAt = 0x3A0, kRowsLead = 0x398; // from A5
constexpr int32_t kStageDip = 0x13E; // from A5, a word: taken off every plane's height (a stage shaking)
// A player's record: half its width as the camera and the picture's edges take it (a word); bit 7 of a
// byte set when the camera is not to follow the player, unless the next byte is not zero
constexpr uint32_t kHalfWidth = 0x132, kNotFollowed = 0x134, kFollowedAnyway = 0x19A;

// More of a player's record:
constexpr uint32_t kOpponent = 0x32;         // word: the other player's record (in FFxxxx)
constexpr uint32_t kHealthFloor = 0x138;     // word: some attacks cannot take health below this
constexpr uint32_t kAutoGuard = 0x3B2;       // byte: not zero: guards by itself (the "Auto" choice)
constexpr uint32_t kHeld = 0x12B;            // byte: what the player holds; bit 0: away from the other player
// In an animation frame: bit 7 of this byte: nothing in it can hit; the frames of a move follow one another,
// 0x18 bytes each, and bits 6 and 7 of the second byte mark the last
constexpr uint32_t kFrameHarmless = 0x17, kFrameEnd = 0x01, kFrameSize = 0x18;
constexpr uint32_t kFrameThroughStop = 0x13; // byte: not zero: the animation goes on while the player is frozen by a hit
// In an attack's entry: bit 7 of these: the attack cannot take the health's ceiling, or what is left, below
// the victim's floor
constexpr uint32_t kAttackFloorCeiling = 0x08, kAttackFloorLeft = 0x09;
// The health of the player being hit, before the hit (words, from A5)
constexpr int32_t kHealthCeilingBefore = -0x4B82, kHealthBefore = -0x4B80;

// --- what a player is doing ---

// Four bytes of steps, each a step of the one before (as for the game itself). In a fight the first is 2;
// the second says who is in charge of the player: 0 the player (kAction then says what it is doing), 2 a
// hit (being hit, or guarding one: "reeling" below), 4 and up being thrown and other things done to it
constexpr uint32_t kState = 0x04;         // long: the four bytes
constexpr uint32_t kControl = 0x05;       // byte
enum : uint8_t { kControlOwn = 0x00, kControlReeling = 0x02 };
constexpr uint32_t kReeling = 0x02020000; // kState when a hit has just landed
// More values of kAction (with kControlOwn)
enum : uint8_t { kActionWalk = 0x04, kActionTurn = 0x08, kActionGuardPose = 0x0C };
constexpr uint32_t kIsThrown = 0x08;      // byte: not zero for a thing thrown (zero: a player)
constexpr uint32_t kAirborne = 0x38;      // byte: not zero while off the floor
constexpr uint32_t kHitStop = 0x5C;       // byte: frames the player stands frozen for after a hit lands
constexpr uint32_t kCrouching = 0x121;    // byte: not zero while crouching
constexpr uint32_t kGuarded = 0x54;       // byte: FF when the last hit was guarded
constexpr uint32_t kUnshakeable = 0x11F;  // byte: not zero: cannot guard, and is not armoured (being thrown and the like)
constexpr uint32_t kArmour = 0x15E;       // word: not zero while the player stands through hits (a "Dark Force" for some)
constexpr uint32_t kArmourAlways = 0x18F, kArmourTaken = 0x161, kArmourTimer = 0x162, kArmourMost = 0x15B; // bytes
// In an attack's entry: how it must be guarded is told by kAttackKind
constexpr uint32_t kAttackReaction = 0x0C, kAttackGuardReaction = 0x0D, kAttackArmourWeight = 0x1C;
constexpr uint32_t kAttackNoAirGuard = 0x1D; // byte: not zero: cannot be guarded in the air
// In an animation frame, kFrameKind is above zero when the frame is a guarding pose: 3 in the air
constexpr int8_t kGuardPoseAir = 3;
// From A5: 1 when the hit that has just landed was guarded
constexpr int32_t kHitWasGuarded = -0x4B8F;
constexpr uint32_t kHitLands = 0x018230;  // a hit has landed: is it guarded? and the start of what follows

// --- for rewrites of the game's routines: its memory and registers by name ---

// Memory anywhere the 68000 reads data from (the ROM's tables as well as RAM)
inline uint8_t Mem8(cps2recomp::M68K &c, uint32_t address) {
    return static_cast<uint8_t>(c.Read(address, 0));
}
inline uint16_t Mem16(cps2recomp::M68K &c, uint32_t address) {
    return static_cast<uint16_t>(c.Read(address, 1));
}
inline uint32_t Mem32(cps2recomp::M68K &c, uint32_t address) {
    return c.Read(address, 2);
}
// The game's variables, by their distance from A5
inline uint32_t Var(cps2recomp::M68K &c, int32_t offset) {
    return (c.a[5] + static_cast<uint32_t>(offset)) & 0xFFFFFF;
}
// A register's low word or byte set, the rest of it kept (as move.w and move.b do)
inline void SetLow16(uint32_t &reg, uint32_t value) {
    reg = (reg & 0xFFFF0000) | (value & 0xFFFF);
}
inline void SetLow8(uint32_t &reg, uint32_t value) {
    reg = (reg & 0xFFFFFF00) | (value & 0xFF);
}
// The size of a 16-bit difference, as the game takes it (negated when its top bit is set)
inline uint16_t Size16(uint16_t difference) {
    return (difference & 0x8000) ? static_cast<uint16_t>(0u - difference) : difference;
}

inline uint32_t Player(int n) {
    return kPlayer1 + static_cast<uint32_t>(n) * kPlayerSize;
}

// The player is free to do anything: in charge of itself, and standing, crouching, walking, turning about
// or holding a guarding pose
inline bool CanAct(int n) {
    using namespace cps2recomp;
    const uint8_t action = Ram8(Player(n) + kAction);
    return Ram8(Player(n) + kControl) == kControlOwn && (action == kActionNone || action == kActionCrouch || action == kActionWalk ||
                                                         action == kActionTurn || action == kActionGuardPose);
}

} // namespace vsav
