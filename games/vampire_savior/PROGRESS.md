# Vampire Savior: progress

The Japanese arcade release of 1997-05-19 (ROM set `vsavj`), on Capcom's CPS-2 board.

## Where it stands (2026-10-11, night)

- The game boots and plays on cps2recomp's own machine: region notice, attract mode, character
  select, the whole one-player ladder to the last boss, an ending and the credits, two-player
  games, the operator's test menu. Seen in pictures from headless runs, and played by hand on the Mac by the owner.
- 68000 program: 106,198 instructions (429 KB; the code is about 596 KB of the ROM) translated to
  C++ in 26,180 blocks and 8,537 routines. 8,355 entries came from made-up play
  (`tools/explore.sh`, with cheats on so that it wins its way up the ladder) and 721 from looking
  at the program (`tools/guess.py`). In a 40,000-frame run of the whole ladder no instruction
  falls back to the interpreter.
- Checks: `make check-blocks` over the ladder movie (336 million blocks) and made-up play, no
  difference; `make check` over the same 40,000 frames: translated code equals the interpreter
  run block by block, and two runs are alike. A state loaded goes on exactly as the run it was
  saved from.
- In-game menu (Esc, F1, the left stick's click, or a gamepad's Start and Select together; its "Quit game" is how the game is left), in pages: training mode, save states in nine
  slots, speed, insert coin, reset, and a page for each category of features.
- Stopping the game (P), frame by frame (O), slow motion (1/2, 1/4, 1/8): in the port, for any
  game. A run stopped and stepped ends in the same state as one left alone.
- Features: free play (on in the launcher's Enhanced preset); cheats (`src/cheats.cpp`): no
  damage for either player, one-hit knockouts, infinite time (all three lines in the rewritten
  rules), full special gauge for either player.
- Training mode (`src/training.cpp`, `src/frames.cpp`):
  - "Training mode" in the menu resets the board, works the coins and both Start buttons with the
    game running unseen (about 480 frames, a fraction of a second) and stops at the character
    choice. Player 1's controls choose for player 2 once player 1 has chosen. The fight has no
    clock and health stops at nothing left, so no round ends. It lasts until Reset.
  - "Stage" on the Training page: the game's own choice or any of the twelve stages by name. A
    hook where the game chooses a match's stage (`00AE7C`) puts the one asked for in its place, so
    it takes effect when the characters are next chosen.
  - "Start over" (Backspace, left shoulder): the machine as it was when the round's fighting
    began is kept and put back, then the players and every copy of the camera are placed: middle,
    left or right corner, sides swapped or not.
  - Combo readout (a combo is hits landing before the player hit is free again; damage lost for
    good and damage that comes back; the round's best), input display, hit boxes, health refills.
  - Frame data: each player is looked at once for every step of the game (a hook where its
    routine dispatches, `02244C`), not every picture: at the usual speed setting the game takes
    about 1.23 steps a picture. Startup is the number of the first step a move can hit on; steps
    in which the player is frozen by a hit landing do not count unless the animation frame is one
    that goes on through it. The advantage is the step the player hit became free minus the step
    the attacker did. A whiffed move and the same move hitting give the same numbers. A frame
    meter shows the last sixty steps. Not compared with published tables: the count may be one
    off from theirs in how startup is defined.
  - The dummy: stands, crouches, jumps; guards always, for a moment after a hit, or at random,
    standing for overheads and attacks from the air and crouching otherwise (from the attack's
    kind, as the game's own rule reads it); while guarding pushes the attacker away (buttons
    pressed one after another) or does a guard cancel (towards, down, down-towards with a punch or
    kick); softens throws (the throw's command while held: half the damage in the test); rolls
    towards or away on getting up (stick and a tapped button); record and replay in three slots,
    replay from a random slot, and the recording played once as an answer after guarding or being
    hit.
  Tested with scripted fights only (numbers printed with `VSAV_FRAMES=1`, pictures): each guard
  choice, a jump-in and a sweep guarded where a crouching and a standing dummy were hit, a guard
  cancel hitting the attacker, a throw softened, rolls both ways, answers after a hit and after a
  guard, every start-over place. Not played by hand. Known gaps: an answer starts on the frame
  after the dummy is free, not buffered before it; "knocked down" is taken to be reeling for more
  than 34 frames; guard cancels are the two usual commands, not every character's.
- Readable code ([cps2recomp/docs/READABLE.md](../../cps2recomp/docs/READABLE.md)): the rules the
  training features touch are written again as C++ with names and run in place of the game's
  code: a frame's hit tests from top to bottom (`src/collision.cpp`), pushing the players apart,
  a hit's damage, a frame of the round with its clock, the guard-range test (`src/fight.cpp`),
  reading the control panel (`src/input.cpp`), and what decides whether a hit is guarded
  (`HitLands`: the attack's kind against how the player stands, guarding in the air, armour).
  Fourteen routines; `make check-rewrites` runs
  each beside the code it replaces (430,000 calls over the movies, none differ). The damage and
  clock cheats are lines inside those rules now. Still the game's own code: what happens when a
  hit lands (reactions, sparks, sound), the players' own state machines (movement, health
  creeping back, the special gauge), things thrown against each other.
  How features and hooks work:
  [cps2recomp/docs/FEATURES.md](../../cps2recomp/docs/FEATURES.md).
- Speed: about 2,000 frames a second headless on an M-series Mac (33 times the board's rate).
- Sound: the Z80 driver runs and the QSound chip plays (levels look sane in a WAV dump, and the
  registers the driver writes are in the ranges the chip model expects); the owner finds it right by ear.
- Controls can be changed: the launcher's Controllers page gives each player's actions a key and
  a gamepad button or trigger (click, then press; a key or button in use changes places), kept in
  `settings.ini` (`[keyboard1]`, `[gamepad1]`, ...; `cps2recomp/port/bindings.h`, which the
  launcher and the game both read). Player 2 can have keys too. A key or button given to the game
  no longer does its other job while playing (stopping, starting over and so on). The file
  format and the game's reading of it are tested; choosing in the launcher's window is not (it
  needs a person pressing things).
- The in-game menu is now the one the recomp ports share (`cps2recomp/recomp-common/`, a copy of
  recomp-common; `port/menu.cpp` is a thin adapter). Its pictures were compared before and after in 353
  situations (every row of the first page, every page, Left/Right and a button on each) and are pixel
  for pixel the same; not seen in a window since.
- The in-game menu has the save states first and Resume last; the launcher starts the game in a
  window unless full screen is switched on; sound is kept a frame or two behind (it could pile up
  to 0.4 s before), with `CPS2RECOMP_AUDIO_LOG=1` printing what waits.
- Launcher and macOS app bundle build (`make app`); the launcher checks the ROM set the same way
  the game reads it.
- The launcher is now the one the recomp ports share (`cps2recomp/recomp-common/launcher/`;
  `launcher/src/description.cpp` is what this toolkit tells it), and the game reads and writes
  `[features]` through the same code as the launcher (`recomp-common/settings.h`). Before the game
  listed its choices, the new launcher's pictures were byte for byte those of the old one (every
  page, five situations, two window heights; taken with no window). Since then the features that
  are a choice (stage, start-over place, recording slot, the dummy's guard and so on) are dropdowns
  in the launcher, where they used to be switches that turned a chosen 2 or 3 into 1. Changing a
  feature in the game's menu now leaves the rest of `settings.ini` as it was. Not seen in a window;
  the dropdowns not clicked.

## Widescreen (2026-10-13)

`src/widescreen.cpp`, a feature whose switch is on the launcher's first page under "Full screen"
(`game.cfg`'s `display`) and on the in-game menu's Display page; off unless switched on, in every
preset (it changes how the game plays). A fight is 512 columns wide where the board shows 384, on
the toolkit's wider picture (`Video::wide`), and is played in that picture.

What is there:
- The picture is the board's with 64 columns more on each side; the panel's objects stay in the
  middle. The sides of the 8-pixel scroll screen are read from the stage's plan in the ROM, as the
  game's own routine reads it (`make check-widescreen`: none of 32 million tiles differ). While
  the red curtain is drawn, and wherever there is no stage, the picture is the board's.
- The players are kept inside the wide picture (`028192`, `02824C` run with 64 less in the
  player's half width). The camera's ends are 64 in from the board's (320 and 576; 224 and 672 on
  Fetus of God, whose board ends are 160 and 736).
- Five comparisons all the game's objects share ask of the wide picture: a thing thrown is done
  away with outside it (`0154AC`), a piece of the stage is drawn inside it (`015512`, `01552E`) or
  while the camera is 64 further to one side of a place (`0154E2`, `0154F4`).
- Survival, the endless battle, the ring out and the infinite stage are wide.
- Checked: the same controls with it off and on, on every stage, every frame where the two
  cameras agree: the middle 384 columns are the board's picture (a few pixels differ on three
  stages, where a piece is drawn sooner; on Iron Horse, Iron Terror much differs and has not been
  looked into).

(What was known when it stopped here, the planes' unpainted columns at a stage's ends and the pieces
coming in late, is dealt with below.)

**What was tried on top of this and taken out again** (commits 4ced3aa to 368d76e on the branch
`widescreen`, the owner's playing found each one glitching; all of it was hooks from outside into
code that is not readable yet):
- Always drawing a stage's pieces, and answering the stages' scripts' comparisons of the camera
  for them (`07D4DA` and a dozen more). Many of those comparisons decide where a piece is or how
  it moves, not whether it is seen: the desert camel's head came off; the lady under the bird
  cage on Feast of the Damned snaps between the poses of her fan arm with the camera at rest (the
  owner's recording, 2026-10-13: record `FFC880`, script `07D4DA`, whose arm is one picture in
  several colour sets shown by turns of the palette at `90C3C0`).
- Drawing both halves of a record that is one of two pieces by the camera's side (`07D438` the
  dancers, `07DE78`, `0809A6`, `080A20`), the other from a copy of its last entries.
- Holding each plane, each row of the 16-pixel scroll screen and each piece inside its painting
  near a stage's ends, by its rate. Facts worth keeping from it: each plane's travel across the
  camera's 384 (8-pixel and 32-pixel screens, by stage: 265/265, 318/339, 137/137, 77/114,
  206/309, 359/335, 306/218, 363/298, 400/352, 306/262, 384/384, 336/288); the row scroll table's
  rows are fixed from 768 and the register at CPS-A 0x20 moves with the camera's height; on each
  stage the wall's rows go at one rate under the camera's and the floor's ramp up to it (wall
  first/last row, rate in 256ths, floor's first rate, last slow row: 922/959/177/197/983,
  768/969/227/229/983, 912/912/93/95/983, 814/936/103/107/983, 768/959/183/201/983, none,
  768/974/233/235/983, 768/976/227/231/983, 768/959/235/193/983, 768/976/233/235/983, none,
  none); the table is a frame behind the camera and kept in four buffers by turns; an object on a
  row has the row in `+24` of its record and `01BBC2` takes the row's place off its own; an object
  whose `+0C` is 0 rides no plane and its script places it in the picture itself.

**Agreed with the owner (2026-10-13): make this code readable first, then do widescreen inside it.**
Rewrites with the checker (cps2recomp/docs/READABLE.md). Done so far, none differing from the game's code
over the movies and a training session on each of the twelve stages with the players moved by made-up controls, the camera going from end to end
(`make check-rewrites`):

| What | Routines | File |
| --- | --- | --- |
| The camera: where the players are, where it wants to be, its most for a frame, its ends | `01E36E`, `01E3F2`, `01E450`, `01E4A0`, `01E8F4`, `01E962` | `camera.cpp` |
| The other two scroll screens placed from the camera: a rate of its travel, or a row of the row scroll table; the scroll registers to be | `01DE40`-`01DF70`, `01DFC0`, `01E4F0`, `01E75E`-`01E876`, `01E8C6` | `planes.cpp` |
| The row scroll table made each frame (it was "not found yet": `01FDFE`, called after the camera's record is done) | `01FDFE` and each stage's part `01FEA0`-`020236`, the super move's wave `0202BE` | `rows.cpp` |
| What is in the picture: the shared tests and the frame's lists of things to draw | `01549A`, `0154CC`, `0154FE`, `01551A`, `015546`, `015572`, `01559E`, `015696` | `picture.cpp` |
| The players kept inside the picture, and the corner's push back | `028192`, `02824C` | `edges.cpp` |
| An object's place in the picture, for drawing it | `01BBC2`, `01BC3A`, `01BC88` | `picture.cpp` |
| The stage's pieces: the frame's run of their scripts, the animation's step, a record given back; the plain piece (every stage) and Feast of the Damned's candles, dancers, lady and thrower | `07D240`, `015030`, `015084`, `07D372`, `0170A6`, scripts 0-4 (`07D354`-`07D596`) | `pieces.cpp` |

Widescreen has no hooks left: the camera's ends, the picture's edges for the players and the tests of
what is in the picture each ask `WideMargin()` where the rule is. The modes' own place for the camera and
the ring out's open ends are lines in the same routines.

Facts from the rewrites:
- Each stage's choice of rates (across / up): the 8-pixel screen 1 / 3/4, 15/16 / 1, 1 / 1, 1/2 / stays,
  3/4 / 1/2, 15/16 / 1, 7/8 / 1, the row 4 above the stage's own / 1, the row 4 below / 1, 7/8 / 3/4,
  1 / 1 (the train, with its own start), 7/8 / 3/4 (Fetus of God, with a shake of its own); the 32-pixel
  screen 1 / 3/4, 1 / 1, 1 / 1, 3/4 / 1/2, the row 22 above / 1, 7/8 / 1, 5/8 / 1/2, 7/8 / 3/4, the row 8
  above / 1, 3/4 / 3/4, 1 / 1/2, 3/4 / 1/2. A plane's start is 448 (336 on Fetus of God; the 8-pixel
  screen's 192 on the subway, 128 on the chateau).
- The row scroll table: a row holds 192 and so many steps more (the floor, below the stage's own row at
  `7B0` bytes in) or less (the wall, above), a step being the camera's travel from its start over a number
  of the stage's (104, 128, 112, 80, 112, 192, 112, 72, 96, 88; the train and Fetus of God have no
  table). The table of `rows.cpp` says how many rows ramp and how many above are alike. It is made in four
  places in video RAM by turns (`90E000`, `90E800`, `90F000`, `90F800`).
- The camera's record: `+60` not zero holds it; `+40`/`+42` its ends up and down; `+50`/`+54` a shake
  added to its place; `+28`/`+2C` where it was a frame ago; `+58`/`+5A` added to the scroll registers
  only. A player with bit 7 of `+134` set (and `+19A` zero) is not followed.
- With the camera held (`+60`), the routine for its height sets its place across again and leaves the
  height alone (the game's own slip; kept).
- The camera's own scroll screen is never drawn as it goes (`+03` is zero in every fight seen), so the
  strip drawing in `01E450`/`01E4A0` is not reached; the 8-pixel screen's is.
- The frame's lists of things to draw: places at `7982(A5)` on, counts at `79C5(A5)` on; a thing thrown
  goes on two (its depth's and the hit tests'), a piece of the stage on one of four from `79AE(A5)` by its
  byte `+09`. A piece with a long at `+28` is drawn only while the camera is left of its first word, or
  (that word below nothing) right of its second.
- A rewrite's check compares video RAM too when asked (`kChecksVideo`, toolkit 4523083).

- A stage's pieces: twenty-four records of `80` bytes from `FFC800`; `+02` the script (58 of them, the
  table at `07D26C`: 0 every stage's plain piece, 1-6 Feast of the Damned's), `+04` its step, `+1C` the
  animation's frame (eight bytes a frame; the first four copied to `+20`: frames left, `FF` at the end, a
  spare, an event for the script). Animations by stage at `2776B0`.
- The dancers (script 2) are one record that is two pieces by turns: left of the camera at `1C0` the
  couples at (384, 48), right of it those at (920, 112); the frame the change is made nothing is drawn.
  The lady (script 3) does nothing at all while the camera is left of `170`; her fan arm's pose is a set
  of colours copied to `90C3C0` at an event of her animation. Nothing else was found writing those colours.

Widescreen behaviour put inside the rewrites so far, beyond the simple form:
- The lady under the bird cage is there (animated, her colours set, drawn) while the camera is right of
  `170` less the wide picture's 64, which in a wide picture is always. Traced over 3,500 frames of made-up
  play with the camera going from end to end: her animation steps on exactly the frames the candles' do.
  Not looked at in a window, and no picture was taken with the camera at its left end.

- The dancers on Feast of the Damned: in a wide picture the record is the left couples for good and a
  second record, made from it, is the group on the right (a byte of the script's own, `+27`, says which);
  switched off in a fight, the second is given back and the first goes by the camera again. Pictures at
  both ends and on the way, and the records looked at with the switch turned off and on. The lady was
  seen in the wide picture's right margin on the way (camera 417, 384, 353).

- A piece with a long at `+28` (drawn only while the camera is one side of a place) is, in a wide picture,
  drawn too whenever it is itself within 192 of the board's picture, by the place it is drawn at. The
  places are each piece's own and take it to go by as fast as the camera; one that rides a row or a plane
  going more slowly was still in the wide picture when they said it had left. Found from the owner's
  pictures: the lady's body is such a piece (record 7, script 0, camera above 384), her fan arm another
  record (script 3); the body went at camera 333 with 20 columns of her still in the picture. Every
  stage has such pieces (listed with a dump of `FFC800` on each stage: about a hundred between them).

- The scripts' own tests of the camera (23 of them in 18 of the 58 scripts), by what they are:
  - the piece does nothing at all while the camera is the other side of a place: scripts 3 (the lady),
    10, 26, 30, 34, 38, 39, 46, 47, 55. The test is written again (`pieces.cpp`, "PieceNNInView"); in a
    wide picture the place is 64 further off, and a piece that is itself within 192 of the picture is not
    out. The rest of each frame is the game's own code, from where the test ends.
  - the record is two things by turns: scripts 2 (the dancers), 8 (Concrete Cave), 56 (Tower of Arrogance,
    two records), 57 (Abaraya). In a wide picture a second record is made for the right one.
  - not done: 11 (Concrete Cave: a state of its own that the camera resets at `1D0`), 24 (Abaraya: colours
    loaded with the camera left of `108`), 48 (Abaraya: by its own place, `1F0`), and 43 (Fetus of God: its
    place worked out from the camera's, then the shared test).
- `VSAV_PIECES=1` prints, for every piece whose own place is inside the wide picture, whether its script
  drew it; counted up over a walk to both ends of each stage, what is left never drawn somewhere is
  script 29 on War Agony (it waits on the piece that made it, not on the camera) and script 41 on the
  train (no test of the camera in it). Pictures of both ends of Concrete Cave, Tower of Arrogance and
  Abaraya were looked at; none of this has been played.

- What is slower than the camera is slower again in a wide picture (`WideLag`, `stage.h`): the lag of a
  plane behind the camera, and the step of the wall's rows of the row scroll table, are greater in the
  proportion of the board's camera's travel to the wide one's (192 to 128; 288 to 224 on Fetus of God).
  So with the wide camera at its end the wide picture's edge is where the board's picture's edge is with
  its camera at its end, and nothing is seen beyond its painting. Before, the wall and the planes behind
  ran up to 64 columns past their painting at a stage's ends: black bands where the viewer's infinite
  stage puts two stages side by side (the owner saw them), and at the ends of a fight's stage. The
  floor's rows, which are faster than the camera, are as they were. Found on the way: the word at
  `398(A5)` that the planes add to the camera's travel is the wall's top row's lag (the row table's
  record `+18`), so planes at "the whole" of it go as the top of the wall does.
- Scripts 46, 47 and 43 place their piece in the picture itself (it rides no scroll screen) from the
  camera's travel. Their placing is written again; the travel is taken as `WideLag` has it and the place
  counted from where the board's picture's edge would be (script 46 is the building under the
  helicopter on the Tower of Arrogance, which stood 28 columns off at the stage's left end).
- Checked: the outermost column of the viewer's picture at both ends of all twelve stages, board against
  wide, is the same but for moving colours and the floor; 450 frames of the infinite stage (every 20th
  of 9,000, fast) have no column that is black above the floor, where 24 of 130 had before.

- Every piece script a stage was seen to use is written again whole (`pieces.cpp`), each checked on its
  stage: 0-41, 43-48, 51-57 (55 of the 58). The game's own code still: 42, 49 and 50, which nothing was
  seen to make or run. Scripts 19, 20 and 23 are for pieces that other pieces make (18 and 45; 22). The check is of the
  paths the made-up play reached: a script's rarer steps (script 11's second and third animations,
  script 38's events) may not have been among them. The two pieces the finder still names are explained
  by their scripts: 41 (the train) is not there until the round is on, and 29 (War Agony) waits, not
  drawn, on a byte of the piece that made it. Script 11 reads two longs from below its own record where
  script 24 reads the players' states (the game's slip, kept); script 48's piece is neither moved nor
  drawn again once the camera has taken its place below 496.
- A path a piece flies (scripts 16 and 17, `07E672`, `07E77E`): legs in the ROM of a word of frames, a
  start, and six longs of speed and what changes it; a word below nothing ends it. A record's own
  animations of colours (scripts 6 and 45) are still the game's code (`09190A`, `09192A`, `01538C`: they
  write the board's colours). A table the 68000 reads by its program counter is in the deciphered
  program; one read through an address register is the ROM as it is (`ProgramWord` for the first).
  Script 18's lists name a seventh place of six: the game reads on into its own code, and so does the
  rewrite.
- More of a piece's record: `+05` a step within the frame's step; `+30` the record that made it (a word);
  `+0D` set by pieces made by others; `+40`/`+44`/`+48`/`+4C` speed and pull (longs) for the ones that
  move by `015466`. From A5: `107` not zero from a knock-out until the next round (the round is decided: many pieces
  change then: the chateau's bats fly off, the train's and Fetus of God's pieces go to a last animation;
  a training session never sets it, so the check holds it on for a run of each stage), `B4` bit 0 turning over
  each frame (pieces drawn every other frame: Vanity Paradise's flames, the train's), `10B` ends Green
  Scream's script 32.

- Script 11 (Concrete Cave) was asleep with the camera left of `1D0`; written again, it is awake in a
  wide picture whenever it is near the picture, as the other such pieces are. The piece it makes
  (script 51) is drawn only on the frames of 11's animation that say so, so with 11 awake the finder
  now names it at some places of the camera: that is the piece working, not a piece missed.

Open: in the same made-up play the pieces' animations step on 75% of the frames with the board's picture
and 62% with the wide one, the same before and after the rewrites. The fights are not the same fights
(the players stand further apart), so this may be nothing; whether the wide picture leaves the game short
of time more often has not been measured.

**Next:** the rest of the scripts, Feast of the Damned's last two (5, 6) and the desert's first; the
object list's making (`01ABC8` and its family to `01BB74`: it writes object RAM at `708000`, which the
check does not compare yet); the three scroll screens' frame routines with each stage's
choice as a table (`01DB16`, `01DFEE`, `01E51E`, `01DA3C`); then each stage's scripts (`07D372`-`080A7A`),
Feast of the Damned and the desert first. Then: pieces always drawn, both halves of the shared records,
and the planes held at the ends, as plain lines in those routines, each tried against the owner's four
cases (the lady's arm, the camel, the foreground under the helicopter, the fire by the tent) in motion,
with the camera at rest and moving, before it is called done. Ways of checking that were built and work:
the same controls with the feature off and on, compared where the cameras agree; both corners of every
stage in training beside the board's corners.

## Invisible walls (2026-10-15)

`widescreen_walls`, on the Display page (the launcher's Features and the in-game menu, beside
Scaling and Picture shape) (a request): the 16:9 picture with the game played as on the board. It leaves
`WidePlay()` false and `WideMargin()` 0, so every rule in the rewrites is the board's (the players' edges, the
camera's ends, the picture tests, the pieces, `WideLag`); only `Video::wide` is set, and near a stage's end the view
is kept on the stage as before (the camera now goes there, so the board's picture is then to one side).
- `make check-widescreen` plays the ladder with widescreen and the walls on and with neither, and compares work RAM,
  video and object memory and the registers every 4,000 frames (9 places): all the same. (The frame hash takes the
  picture in, so `--hash-log` cannot compare them.)
- `movies/corners.txt`: a training session walked to the left end (frame 2500) and the right (3500). On all twelve
  stages the wide picture's 128 columns beyond the board's are painted at both ends (no black runs).
- Not hand-played; not looked at in a window. A piece that the board draws only once it is in its own picture
  comes in at the play area's edge, not the picture's.

## Modes (2026-10-12)

"Alternative modes", the first row of the menu (`src/modes.cpp`): training (moved there), attract
mode and a stage viewer, the last two with a page each under it (the shared menu's pages under
pages, `Game::pages`). Checked with no window only (pictures, traces of the game's memory, sound dumps); nobody
has yet used them in a window or listened.

- **Attract mode.** The board reset and no coin kept in (free play's credits are held back). "The
  intro only" and "the fights only" move the game's own step on where a wanted part ends, and for
  the fights count the game's fight counter on as the rankings would have. Traced over 16,000
  frames: the introduction loops about every 2,700 frames; the eight demonstration fights come round
  in order.
- **Stage viewer.** A two-player round on the stage chosen (the same hook as training's stage
  choice), reached unseen. The players stand 192 apart and are moved together, so the game's own
  camera follows; the picture leaves out every object that is not the stage's
  (`Video::hideObject`): colour sets 0-21, and the shadows' tiles in set 22. On all twelve stages
  the only pixels that differ from the picture with shadows are the shadows'. The camera goes the
  whole way (256 to 640) at each speed.
- **Music.** A tune is queued as the game queues one: `FF05`, then the tune's number.
- **The infinite stage** (2026-10-13). Two machines: the port's own shows a stage; the next
  stage's is kept as a save state (327 KB), loaded into the port's machine after every frame, run
  for a frame (three, until its round is on: about 320 frames) and put away. The picture is the
  shown stage moved left with the next beside it; the sound is the two mixed by how far the next
  has come in, the next's samples kept in a short queue because two machines' frames do not make
  quite the same number. When the next is all the way in, its machine becomes the port's. Over
  4,200 frames at the fast speed: five stages in order, no silence, no jump in the sound larger
  than a plain viewer run has; 1,016 frames a second on the Mac with both machines running.
- **Model viewer** (2026-10-13, `src/model.cpp`). One character by itself on a stage or on white or
  black, from a quarter to 25 times its size, standing (player 1's controls move it) or going through any
  of its animations. A two-player round reached unseen, the second player not followed by the camera
  (the game's own flag, `+134`), stood 768 to the right and never drawn. The frame's own picture is the
  stage alone; the character is drawn again by itself (objects only, marked) and laid over it at the size
  wanted. Nothing of it is in a save state (the states made before it still load; loading one ends it).
  Checked with no window: pictures of five characters, a stage, white and black, four sizes, animations
  of two lists; an animation switched on and off in mid view (the character stands again); walking with
  the controls; a wide picture. The menu's page has not been looked at, and nobody has used it.
  Added the same day: Speed (the toolkit's `SetSlowMotion`, the menu's own slow motion asked for by the
  mode), "Repeat after" (an animation's last frame held 0 to 10 seconds before it goes back: traced, the
  holds are 120 frames at 2 seconds) and "Reset options" (run in mid view: the viewer went to the first
  character and stage). The slow motion has not been watched: with no window a run takes no time.
  Then: one "Animation" choice of 769 (standing, A0-A255, B0-B255, C0-C255; one the character has not
  got goes on to the next it has), left and right stepping through them, and the name shown for 120
  frames after a change. The readout is set after the frame now: set before it, training's and the
  stage viewer's frames cleared it, so the first version's never showed. Not seen: no window.
- **The menu** (2026-10-13): Training mode's Start, Start over and aids, and the dummy's page, are
  under Alternative modes too.

- **Survival, the endless battle, the ring out** (2026-10-13, `src/battle.cpp`, `src/machines.cpp`,
  `src/modes.h`). Each is a two-player match that the mode sets up and follows with another. The
  next match is made ready on a spare machine (a save state run in the port's machine) while the
  last one's ending plays, and then takes its place: the game's own match set-up does all the
  loading of a character. Who fights is put in where the game chooses the stage; who the game
  plays is a byte of the player's record, changed while fighting. The endless battle's changing
  stages and the ring out draw the fighters alone on the port's machine (objects only, marked:
  `Video::markObjects`) and lay them over a stage that another machine shows with its camera put
  where the fighters' is (hooks on the camera's two routines). Up to four machines run in a frame;
  with three, 737 frames a second on the Mac. Checked with no window: pictures, traces, sound
  dumps. Nobody has played them yet.
- Known of them: with the stage behind, a stage's pieces that belong in front of the fighters are
  behind them, and a super move's own backdrop is not shown; the two "Best" counts are features, so a preset chosen in the launcher clears them.

Facts found for the model viewer:
- A character's animations: three tables of longs by the character's number, at `0BCEFA`, `0BCF7A` and
  `0BCE7A` (the game starts one of each at `027EB8`, `027EC0`, `027EC8`, the number in D0): each long is a
  list of words, an animation's distance from the list. The lists are not counted; by what is a frame,
  about 100, 130 and 60 a character. A frame is `18` bytes: frames of the game it is shown for, then a
  byte for what follows (0 the next, bit 7 a jump to the long at `+18`, bit 6 the end), its picture at
  `+04`, the number of its boxes at `+08`.

Facts found for these:
- A "round" is one of a player's two lives: `FF810C` (the round's result) is set at each
  knock-down, and the health of the one knocked down comes back for the next.
- In a player's record: `+382` the character (0 Bulleta, 1 Demitri, 2 Gallon, 3 Victor, 4 Zabel,
  5 Morrigan, 6 Anakaris, 7 Felicia, 8 Bishamon, 9 Aulbath, 10 Sasquatch, 12 Q-Bee, 13 Lei-Lei,
  14 Lilith, 15 Jedah; kept again at `+3E0`), `+3AE` its colours, `+3B4` 1 when the game plays it
  (225 places read it; changing it in a fight hands the fighter over at once), `+380` 2 for a
  player in the game (0: the panel counts down for that side). `+40`/`+44` speed, `+48`/`+4C` its
  change each frame.
- The camera's record is at `FF8280`: `+20`/`+24` where it wants to be, `+5C`/`+5E` the most it
  moves in a frame, `+44`/`+46` the ends. `01E36E` (across) and `01E3F2` (up) work those out and
  hand them to `01E450` and `01E4A0`.
- `028192` keeps the player at A6 inside the picture (`+132` half its width; `+116` 1 or 2 at an
  edge), and passes a cornered player's push back to the attacker. The stage's ends are only the
  camera's ends: a player is kept in the picture, not in the stage.
- The panel's objects: colour sets 0-4, and the faces beside the bars in the players' own sets,
  tiles `13800`-`143FF`. A player's own objects: sets 10-15 (the first) and 16-21.

- A stage's three scroll screens have a record each: `FF8200` the one of 8-pixel tiles, `FF8280`
  the one of 16 (the camera's record; the stage's own plane), `FF8300` the one of 32. `+20` is
  where its picture's left edge is in the stage (the scroll register is that less 64), `+08` its
  middle, `+03` not zero when the game draws its picture into the map as the camera moves. The
  routines at `01DE40`-`01DEEA` and `01E762`-`01E7F6` place a plane from the camera, one for each
  rate (a half, three quarters, seven eighths...). The scroll screens of 16 and 32 are whole
  pictures in the map on every stage. The one of 8 is only 512 columns there: on two stages (the
  subway, the chateau) its picture is that wide and goes round; on the other ten it is up to 1,024
  wide and is drawn a 32-pixel strip at a time (`01EE22`, `01EE58`, `01EF16`, `01F06E`) from the
  stage's plan: eight bytes at `FF4302` (two rows of four 256-pixel blocks), blocks of 8 x 8 cells
  at `35AD18`, cells of 4 x 4 tiles at `35C918`. The game keeps 64 columns up on each side of its
  picture, in strips, so what is beyond the picture's edges in the map is not to be trusted.
- The picture a frame shows is the one the game worked out the frame before: the scroll registers
  written and the object list taken at the frame's start are for the camera as it was a frame ago
  (the word at `FF804E` is the 16-pixel screen's register to come: the camera then, less 256).
- The red curtain between a match's screens: objects of colour set 3, tiles `2C600`-`2FDFF`.
- An object's place in the picture is worked out at `01BBC2` (against one of the three planes'
  records, chosen by the object's `+0C`), `01BC3A` and `01BC88` (the panel: its own place, plus 64);
  nothing there leaves an object out for being outside the picture.

- With no coin in, the word at `FF8000` steps through the show: `0C` its start, `10` the sound
  notice, `12`-`14` the maker's, `16`-`18` the introduction and title, `1A`-`20` a fight, `22`-`24`
  the score ranking, `26`-`36` the maker's notice and the title, `38`-`3E` a second fight, `40`-`46`
  the versus ranking, then `0C` again (7,864 frames). Below `0C`: starting up, or a game.
- `FF1E2A`: a byte counting the show's fights, two for each (added at `0054C6`, on the way to a
  ranking); it picks who fights and where. Eight fights before they repeat.
- Sounds wait in a ring at `FF0E0E` (256 entries of sixteen bytes, three longs used; the first is
  the number), added at the word at `FF1E0E` by `0031DA` and sent by `003136`. Tunes: `02` the
  character choice, `05` the versus screen, `09`-`14` the twelve stages in the order of their
  numbers. `FF05` is sent before a new tune.
- The camera (`01E36E`, the record at `FF8280`): with the players 192 or more apart it goes to
  their middle less 192; nearer than that it has a dead zone.
- Objects' colour sets in a fight: 0-6 the panel and sparks, 10 and 16-17 the players, 22-31 the
  stage's pieces; shadows are tiles `F8F7`-`F90A` in set 22.

## Hidden characters (2026-10-14)

`src/choosing.cpp`. What the board hides, read from its code (the character choice is `020974` on; a
player's record there: `+03` the character under the cursor, `0B` the "?" box; `+381` which player):
- Dark Gallon is character `0x12` (Gallon's number with bit 4). Gallon chosen while Start is held
  (bit 15 of `+394`), with two or three punches pressed together (his first colours) or kicks
  (his second): `020B9C`. He has only those two sets of colours, and in a one-player game Gallon is
  his rival as he is Gallon's.
- Oboro Bishamon is `0x18`, the eighth and last opponent of every one-player route (the table at
  `00B268`, eight bytes a route, sixty-four a character, read through a register so from the plain
  ROM; a route of `0x800` on for Shadow). He is reached when the game has gone well enough
  (`00A738`); no code on this set chooses him. The tables have rows for every character number from
  `0x10` to `0x1F`, `0x18`'s being Bishamon's, and he plays as Bishamon does when a person controls
  him (scripted: walking, the three-hit chain, the sweep, a charge special; his entrance is about 150
  frames longer than Bishamon's).
- Shadow is a way to play, not a character: Start pressed exactly five times on the "?" box (`+42`
  counts, `+43` is set at five and cleared at six), then a button. The player is the character the
  box was showing, or the one Start was pressed on three times before (`+44`, `+45`), with `+3BC`
  set (copied to `+3E3`, the character to `+3BD`); after each match a person playing Shadow wins, they
  become the opponent just beaten, in its colours (`009BB2`). In two-player games nothing shows it.
- `BF(A5)` stops all of the codes when set; nothing in this set sets it.
- Tables read with `(pc,d0)` come from the deciphered view (`program.bin`); read through an address
  register (`lea ...(pc),a0` then `(a0,d0)`) they come from the plain ROM (`program.bin.data`).

The "Hidden characters" feature (Game page; off in every preset; Off, Player 1, Player 2, Both) is
lines in rewrites of the two routines that read the codes, `ChooseColours` (`020B38`: the button that
chose gives the colours, two alike are told apart, Start held gives Dark Gallon) and `CountShadowCode`
(`020C98`): choosing Gallon gives Dark Gallon, Bishamon Oboro (a punch his first colours, a kick his
second), and the "?" box Shadow. The modes' fighters get the hidden forms at the stage hook, where a
mode's characters go in. Checked: the board's codes for Dark Gallon and Shadow work as before (both
rewrites beside the game's code, none differ); with the feature, each of the three for player 1, both
players in training, and the endless battle's fighters (Dark Gallon against Oboro), in pictures. Not
played by hand; Shadow's change of character through a one-player game not run.

## Release readiness (audit of 2026-10-12)

- A release's game program holds the translated code, as Muscle March's does (the owner's decision,
  2026-10-12): `tools/release/` builds it from `work/<game>/gen`, on a machine with the ROM set.
  With `NO_GAME_CODE=1` the scripts build the program with no translated code instead, which runs
  the ROM set in the port's interpreters: CI builds that, to prove everything compiles and starts.
  Over the 40,000-frame ladder movie it plays to the end at about half the translated build's speed;
  53 of its pictures differ, each for one frame (interrupts taken between instructions, not blocks).
  `make check-release FRAMES=40000` holds it to that, and prints the checksum of its picture hashes.
- Systems, by the ladder movie's picture hashes (`--hash-picture`), each kind of build against the
  Mac's: the release archives (translated code) for macOS arm64, macOS x86_64
  under Rosetta, Linux x86_64 on a Steam Deck and Windows x64 under Wine all write the translated
  build's file; the builds with no translated code for the same four write the interpreted build's.
  (A toolkit sync keeps the files' times, so a build folder made before it may not notice a changed
  file: `touch` it or start the folder again.) The owner plays the Mac build by hand and finds the sound right, and has played
  on a Steam Deck (SteamOS 3.9.2, from Game Mode). Not played by hand: the Intel slice and a real
  Windows PC.
- `machine/cps2crypt.cpp` is MAME's code (BSD-3-Clause), the one file here from an emulator; the
  QSound model follows MAME's. `THIRD_PARTY_NOTICES.md` says what and whose.
- `make audit` (`tools/audit.sh`); `tools/audit.sh --public` lists what is left before the
  repository can be public. `docs/RELEASING.md` is the release process.

## Custom colours (2026-10-14)

Feature `custom_colours`. A player asked for it after the release. Since 2026-10-14 the code is the toolkit's
(`fight/colours.h`, shared with SFA2) and `colours.cpp` here only describes where the colours are. Facts (vsav.h):
- `38C198`: a long for each character number 0-19 (16-19: 0x12 is Dark Gallon, the others repeat
  0, 1, 3) pointing at its colour sets. Each set is 0x80 bytes: four palettes of 16 words. There
  are 10 sets for every character; they follow each other at 0x500, with a gap of 0x400 after
  Sasquatch's that nothing is known to read.
- Player `+3AE` picks the set (020B38: 0-5 the button; 6 two or more punches; 7 two or more
  kicks; 020FC0: 8 + (set & 1) when auto guard was chosen; a mirror match gets the next set,
  Dark Gallon's only 0-1). `01BEC0`, `01C164` and `01C670` keep where the set is at `+3A4`.
  `02AD3C` copies a palette from A0 to the player's place in the palette RAM (`+18B` of the
  buffer at `D0(A5)`), keeping the brightness that was there; `02ADC8` copies palette D1 of the
  set; `02AD82` a palette of the effects table at `39A900` (some entries by `+3AE` too: 02A946).
- Each palette's 16th colour is the see-through pen for objects (video.cpp), never shown.

The feature writes the files' colours over the ROM image's (the game reads them with `movem`, so
its code is the same) and puts the game's back when it is switched off. Checked: an export of
every character, and Morrigan's LP body palette with its channels turned seen in the model viewer
(on and off). Not checked: the effect palettes (lines 2-4) and the `39A900` entries in a fight; a
change taken in the middle of a round (it waits for the next copy); nobody has made colours by hand.
- `38C218`: a long for each character number 0-31 pointing at 66 palettes for what is done to it
  (0x840 bytes; `02AD20` copies palette D0 of the player's character's; `02ABC0` adds `+3AE` to an
  effect's first, so effects are in tens by the set). They follow each other from `391BA0` to
  `39A7E0`; the shared effects table `39A900` comes after.

PalMod's own definitions (Preppy/PalMod, `VSAV_A_DEF.h`; facts only, its licence is not stated) know
this set's `vm3j.10b` (CRC `434518e9`) and name these palettes: a set's four are the character, then
for Bulleta "Critters", "Weapons", "Explosions"; the effects block holds Poisoned, Zapped, Burned,
Pharaoh's Curse, Midnight Bliss. The program files have each word's bytes swapped against
`RomSet::program` (checked over the whole of `vm3j.10b`).

A copy of `vm3j.10b` in `palettes` is compared with the game's: words changed inside the two
tables' places are taken, the rest are counted and left (the file holds the game's rules too), and a
file whose tables of places differ is refused. Checked with a made copy (Morrigan's LP body, one
effect palette, a word at `3AC0E0`); never with a file PalMod itself wrote. Not taken yet: the
portraits (character select, win screen; `3AC0E0` and around, `008216`) and `39A900`.

Colour editor (2026-10-14; now the toolkit's `fight/colour_editor.cpp`): the model viewer with a panel. Each frame it puts
the player's set at `+3AE`/`+3A4` and writes the set's palettes into the palette buffer (`D0(A5)`,
`90C000`) from the player's slot `+18B`, the brightness kept, so a change shows at once; only the
first line for the characters whose bit is set in `EF92EF96` (02ADB2: their other lines go
elsewhere when an effect needs them, so changes to those are seen only then, not in the viewer).
Edits go into the ROM image (`colours::SetColour`) and are saved to the character's file (only the sets
and lines that differ from the game's; a line that is `-` is the game's) 45 frames after the last
change and when the editor ends (`colours::BeforeFrame`, before every frame). Checked headless with
movies: the rows, the levels, copy and paste, the set changed (HP: Morrigan in red), the blink,
the character changed, the save when training mode takes over. Not checked by hand: holding a
direction (repeats), a gamepad, the effect lines in play.

Saved sets (2026-10-14): `palettes/library/<Character>/<Name N>.txt`, one set (four lines of 16) a
file, named by the next free number. Load tries a file over the set (its colours kept first, a `-`
keeps the set's own, the 16th of a line never changed) and puts them back on leaving the row unless
LP loaded it; MK twice (within 1.5 s) deletes the file. Checked headless with a movie: two saved,
each tried, back to none, one loaded, one tried and put back, one deleted; the files and the
character's file agree.

Tester feedback (2026-10-14, branch `tester-feedback`):
- Model viewer's second shadow: the second player stands 768 to the right and object x counts in 1024,
  so its shadow wrapped into the picture once player 1 stood right of about 256. `NoShadow` (machines.cpp)
  moves the pieces of shadow nearer that player's column (player x - camera + 64) off the picture when the
  board takes the list (`Video::objectsTaken`); the spares' frames run with it off. The ring out uses it
  for the fallen fighter. Checked headless: walk right and back, normal and wide; a forced ring out and a
  natural one.
- "Stage in front": at 1x on a stage the picture is drawn a third time with everything but the panel and
  player 2's sets; a pixel of the character that is not what that picture shows has something in front
  of it, and takes it. Differs from before on Abaraya (kettle and steam) and Fetus of God (a bulb) only,
  with Bulleta in the middle; other stages' front pieces are elsewhere.
- Line row: which animation shows which line is found by running (the pictures' own data has the line in
  some form not yet read: `+04` points to a header and 4-byte pieces with flag bytes 0x10-0x12, 0x02,
  0x24 before tile numbers): every frame of every animation put in player 1's record on a spare copy of
  the viewer's machine, eight a frame, counting objects in sets kPaletteSlot+0..3 of the list the game
  made. About 300 animations, done a few seconds after the editor shows the character. Morrigan: line 1
  A56, line 2 C2 (4 objects), line 3 A53. Not for the kFirstLineOnly characters.
- Ring out drop: rise 48 lines and fade to black over 32 frames, then the next stage from 48 below and
  from black over 32; sound crossfaded over the whole 64.

## Board settings (2026-10-15)

`board.cpp`: the test menu's operator settings (設定: System and Game) as features on the Board page, each
"As the board is set" unless chosen. The board's start reads the settings memory (001980, called at 001168)
and copies the record (FF0C66) into variables; the hook at 00116E, where the call returns, puts the choices
in them. The settings memory is not changed. The test switch (the menu's Test menu row, F2) puts the board's
own values back, since the test menu shows, changes and saves the variables; leaving it does not read the
settings again, so the choices go back in when a game next begins (00A4BA and 008F56, each before it copies
difficulty to FF80BE and continue to FF811F).

The record and the variables (from A5), with the values found by stepping each row of the test menu's pages:
+01 continue 98 (0 no, 1 yes), +03 demo sound 99, +06 self-censorship 8C (0 off); +10 difficulty A0 (0-7,
levels 1-8, a new board 1), +11 attack power A1 (0-3, 1), +12 timer speed A2 (0-3, 1), +13 game speed A3
(0 normal, 1-3 turbo 1-3, 4-6 free select 1-3; 5), +14 damage gauges A4 (0-3 for one to four; 1), +15 event
A5 (0 normal, 1 one match, which clears continue), +16 automatic BB (0 none, 1 free select). Checked headless:
the variables with and without choices, the test menu showing the board's, and difficulty 8 back in FF80BE
in the game after it.

## Approximate, or left to the interpreters

- The Z80 sound driver is translated too (4,498 instructions, 62 routines; 142 million of its
  blocks checked against the interpreter, no difference). Its statements are calls of the
  interpreter's own routines rather than plain arithmetic, which is all a sound driver needs.
- 24 instructions of the 68000 program (status register changes, `rte`, `trap`) are done by the
  interpreter one at a time (`c.Interpret`); they have to go back to the run loop anyway. (The game
  has no divisions.)
- QSound: no "Q1" stereo filter and no ADPCM voices (they need the DSP's internal ROM, which is
  not in the game's set). Volume and panning curves are our own.
- Timing is by the line: 1024 CPU cycles a line, interrupts at block starts.
- Code neither reached by a run nor guessed is not translated (it runs in the interpreter when
  reached, and `make run` notes it for the next build): most likely other characters' endings and
  rarely used moves.
- Transitions between screens draw a red curtain in two halves that slide in from the sides. The
  game's object list really does place it so (columns of gradient tiles, added a column at a time);
  it has not been compared with a real board.

## Facts about the game

- Reset: stack pointer `FF8000`, start `000926`. The start-up code writes the object registers at
  `400000`, then tests RAM.
- The program is enciphered up to `100000`; all code found so far is below that.
- The set's key file (`vsavj.key`, 20 bytes) holds the board's own cipher key, and the port needs it with
  the rest of the set: it is the player's, from their own board, like the ROMs. No key and no key
  values are kept in this repository.
- Inputs: six buttons a player; the first coin slot is key `5`.
- Memory (found by dumping RAM where a value on screen was known, `tools/ramfind.py`; names in
  `src/vsav.h`): the game's variables are reached from A5 = `FF8000`. Credits `FF8076`; the
  round's clock `FF8109` (seconds, 99 at most) with the part of a second at `FF810A`; the
  players' records at `FF8400` and `FF8800` (A6 while a player is worked on): health left at
  `+52`, what it can come back to at `+50` (`0x120` full, below zero knocked out; `+52` creeps
  back up to `+50` at `022448`); special gauge stocks at `+109`, the gauge being filled at `+10A`;
  where the player stands at `+10` (across) and `+14` (height, `0x28` on the floor); what the
  player is doing at `+06` (0 nothing, 2 crouching, 6 jumping, `0A` attacking); `+105` is not zero
  while an attack is out; what the player holds is copied about `+121` to `+12D`. (The last few
  from `--trace`: the records written out every frame of a scripted fight.)
- Routines: see the table in READABLE.md and the names in `src/vsav.h`. In short: the frame
  interrupt reads the panel into a word a player at `58(A5)` and `5C(A5)`; `017F0C` runs a
  frame's hit tests (things thrown against each other, against the players, the four player
  records against each other by the masks at `+108`); an attack's box is named by byte `0A` of the
  animation frame at `+1C` and found in the table at `+8C` (32 bytes an attack: the box, then
  what the attack does); the boxes an object can be hit in are named at `+94..96` (tables at
  `+80`, `+84`, `+88`) and its pushing box at `+97` (table `+90`); a box is four words: the
  middle's offsets (across, turned about with the object; up), half the width, half the height.
  Things thrown have records like the players', thirty-two of them from `FF9400`. The picture's
  left edge in the stage is at `FF8290`. `+3B2` is the "Auto" guard choice.
- Where the game is: words at `FF8000`, `FF8004`, `FF8008`, `FF800C`, each a step of the one
  before (a game on: 2; choosing characters 2, match 4; a round 4; being fought 6). `FF808D`: who
  is playing (a bit each). A player's record has the same four steps at `+04`: 2 in a fight; then
  `+05` who is in charge (0 the player, 2 reeling from a hit or a guarded hit, 6 being thrown);
  then `+06` what it is doing (0 standing, 2 crouching, 4 walking, 6 jumping, 8 turning, `0A` a
  normal attack, `0C` a guarding pose, more for specials). `+5C` frames frozen after a hit lands,
  `+38` off the floor, `+121` crouching. The player's routine dispatches on `+05` at `02244C`
  (table at `02245A`) and on `+06` at `0225C2` (table at `0225EE`).
- The stage: a word at `FF8100`, the stage's number doubled (0 to `16`: twelve stages, named in
  `src/vsav.h`). `00AE7C` chooses it when a match is made: between two players (`FF80AC` is 3) at
  random from a row of sixteen for player 2's character (table at `00C468`); in a one-player game
  it is the next opponent's.
- Guarding (`018230`): only from a guarding pose (the animation frame's byte `0B` above zero; 3
  in the air). Standing guards everything but attack kinds 02, 03, 38, 39 (low); crouching
  guards everything but kinds 4D, 4A, 2C, 37, 42, 48 (overheads) and kinds 00, 01 from a player
  in the air; kind 2D cannot be guarded.
- The stage: players stand from 280 to 1000 across, the camera goes from 256 to 640 (kept in
  five places), a round starts with the players at 554 and 728.
- Getting up: a direction with a button tapped while knocked down rolls about 100 across.
- The game runs tasks with stacks of their own (it moves the user stack pointer about), has no
  divisions, and talks to the sound driver through `movep` into the shared memory at `618000`.

## Next

1. Play it by hand: picture, sound, controls, speed.
2. More of training mode: throw ranges drawn, gauge and Dark Force numbers, an
   input history that reads motions; more of the fight as readable code (the players' own state
   machines: `022552` on, a table of routines by what the player is doing).
