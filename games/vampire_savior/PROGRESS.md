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
(`game.cfg`'s `display`; the launcher keeps it in `launcher.ini` and says at every start whether
it is on) and on the in-game menu's Display page; off unless switched on (in the Enhanced preset
too: it changes how the game plays). A fight is 512 columns wide where
the board shows 384, 16:9 for 4:3, on the toolkit's wider picture (`Video::wide`), and is played
in that picture. (The first version changed the picture only; the owner asked for the game to be
played in it, the same day.)

- The picture is the board's with 64 columns more on each side. The panel's objects (bars, names,
  clock, gauges, the words of a round) stay in the middle. The sides of the 8-pixel scroll screen
  are read from the stage's plan in the ROM, as the game's own routine reads it;
  `make check-widescreen` compares the plan with what the game drew, where the board shows it (32
  million tiles over the ladder movie's controls, none differ). While the red curtain is drawn,
  and wherever there is no stage, the picture is the board's.
- The players are kept inside the wide picture: the game's two routines for that (`028192`,
  `02824C`) come to the picture's edges only through the player's half width, so they run with 64
  less in that word and it is put back where they return. Driven apart, two players stand 464
  apart (336 on the board); both still come to the stage's ends (280 and 1000).
- The camera's ends in its record are 320 and 576 (256 and 640 on the board), so the wide
  picture ends where the stage does. (If the camera is ever nearer an end than that, the stage is
  drawn to the side by what is missing: the first version's rule, still there.)
- What the game asks "is it in the picture?" of, in routines all its objects share: a thing
  thrown is done away with outside the wide picture and a margin (`0154AC`: one comparison, made
  by a hook with the wider numbers). A piece of the stage is always drawn: the comparisons that
  would leave it out (`015512`, `01552E`; `0154E2` and `0154F4`, by the camera's place) always say
  it is in.
- The stages' own scripts (`07D4xx`-`080Axx`) compare the camera with a place too. Eleven such
  comparisons only draw or move a piece while the camera is on one side (`07DF4C`, `07E05E`,
  `07E0BA`, `07F26E`, `07F896`, `07FB22`, `07FD4A`, `07FE26`, `0803B4`, `080424`, `0808A6`): they
  are answered the way that keeps the piece. Four scripts keep two pieces in one record, which is
  the left one while the camera is left of a place and the right one otherwise (`07D438`: Feast
  of the Damned's dancers and a piece on the right; `07DE78`, `0809A6`, `080A20`): in the wide
  picture both can be seen, so the piece the record is not is drawn from the entries the game
  last put in the object list for it, placed against its plane (`Video::objectsTaken`). The
  dancers move, so their record is kept the dancers once the other piece has been seen; the
  other three pairs do not move. Seen in a pan of each stage; the three other pairs not looked
  for one by one.
- The planes behind are kept inside what the board shows of them, each by how far it travels
  (`kTravel`, seen in the planes' records): a plane goes with the camera until 64 more columns
  on a side would show where nothing was painted, and stops there. (Before this the backdrop's
  colour showed in a strip at a stage's end on some stages, and where two stages meet in the
  endless battle's scroll.) A piece of the stage rides on one of the planes and is drawn to the
  side with it: the game says which plane as it works out the piece's place (`01BBC2`, `01BC3A`,
  by the byte at `+0C` of its record; `01BC88` for the panel), and hooks there note it for the
  entries of the object list that follow. Each machine a mode keeps has its own note.
- Survival, the endless battle, the ring out and the viewer's infinite stage are wide: every
  machine's frame is as wide as the port's that frame, and the pictures are put together at that
  width; the ring out's edge is the wide picture's.
- Hit boxes and the frame meter are drawn where the stage and the middle are in the wide picture;
  the readouts go to its edges. Training's "Start over" puts the camera at the wide picture's end.
- With the feature off: the same state hashes as before over 20,000 frames of the ladder movie,
  and the same speed.
- Seen in pictures only (every stage from end to end, the ladder movie's controls, a training
  session with the boxes on, each mode for a minute); not yet in a window, and nobody has played
  it. The movies do not replay with it on.
- Known, not done: each character's own code that knows the board's width (helpers and things
  that come in from the picture's edge, moves that go to an edge: dozens of places with 384,
  416 or 448 in them) still uses the board's, so such a thing comes in 64 columns inside the wide
  picture's edge. On the two planes that travel less than 128 (the desert's), the view shows a few
  columns more than the board ever does (25 and 7 on each side); nothing missing was seen there.

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
  behind them, and a super move's own backdrop is not shown; a fighter that has fallen leaves its
  shadow; the two "Best" counts are features, so a preset chosen in the launcher clears them.

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
