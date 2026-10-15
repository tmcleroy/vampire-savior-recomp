# Features, hooks and finding things out

A game's own sources live in `games/<id>/src` and are compiled into the port. What they can do is in
`runtime/game.h` and `runtime/lift.h`.

## Features

A feature is a switch the player sets: in the launcher's Features page, in the in-game menu
(`port/menu.cpp` over the shared `recomp-common/menu.cpp`: Esc, F1, a gamepad stick's click or its Start and Select together), or for one run with `--features a,b,-c`. The
choices are kept in `settings.ini` (`--settings`). With every feature off the game runs exactly as
the board does.

```cpp
TheGame().features.push_back({"free_play", "Game", "Free play", "Start works without a coin.", kFeatureEnhanced});
...
if (FeatureOn("free_play")) { ... }
```

`kFeatureEnhanced` puts a feature in the launcher's Enhanced preset; `kFeatureExclusive` allows one
of its category at a time. `--list-features` prints them all (the launcher reads that).

A feature can be a choice of several things instead of on or off (`choices` in its entry; the first
means off; `FeatureValue` gives the number chosen; `--features name=2`), can last for one run only
(`kFeatureSession`: not kept in settings.ini, off again at Reset), and the menu shows each category
as a page of its own.

Every game also has the port's own features (`RegisterPortFeatures`, `port/main.cpp`): run-ahead on
the Game page, and on the Display page how the picture is fitted to the window (`display_scaling`,
`display_shape`: `port/present.h`) and the size it opens at (`window_size`). The first choice of each
is how the port always drew it, so they are the same in every preset.

## Commands

`TheGame().commands` are things the game's sources do when asked: a row of the menu (on the first
page, or a category's), and a key while playing if it names one (`kKeyRestart`: Backspace or a
gamepad's left shoulder). A command gets the machine; it may reset it, load something it kept
(`machine.Serialize`), and ask for the game to run unseen until it says otherwise
(`SetFastForward`) while it works the controls from `beforeFrame`. Vampire Savior's "Training
mode" and "Start over" are commands (`src/training.cpp`).

## Stopping and slowing

The port can stop the game (`P`), go on a frame at a time (`O`) and run it at a half, a quarter
or an eighth of its speed (the menu's Speed row, `-` and `=`). The game's state is the same
whatever was done (`--keys TICK:KEY,...` presses these keys in a headless run for tests, sets
features with `fNAME=N` and runs commands with `cNAME`).

## Where a feature does its work

- **After every frame** (`TheGame().frame`), on the game's memory: `Ram8/16/32`, `SetRam8/16/32`
  take the 68000's addresses. Good for things that are a value to hold (credits, a gauge).
- **Before a frame** (`TheGame().beforeFrame`), with the players' input set: to change what the
  game is told is pressed.
- **In the game's own code, with a hook** (`lift.h`):

  ```cpp
  AddHook(0x018AB0, "TakeDamage", [](M68K &c) { ...; return false; });
  ```

  The function runs whenever the 68000 arrives at that address (the start of a routine or block),
  with the registers as the game has them. Returning false lets the game's code go on; returning
  true says the hook did the routine's work and left the CPU where the routine would (`Return(c)`
  is its `rts`). The translator reads hook addresses out of the sources, so after adding one,
  `make` again. Hooks run the same in every `--cpu` mode, so `make check` covers them.
- **By rewriting the routine** (`AddRewrite`): the game's rule as readable C++, with the feature a
  line inside it. See [READABLE.md](READABLE.md).
- **A readout over the picture**: `SetHud(lines)` shows short lines of text as small panels (a
  training display), `SetBoxes(boxes)` draws rectangles (hit boxes). `TheGame().state` saves the sources' own variables with save states.

Vampire Savior's sources: `vsav.h` (names for what is known of the game), `cheats.cpp` (free play;
damage and the clock through hooks), `training.cpp` (damage readout, input display, health that
refills, and the dummy: `beforeFrame` changes what the game is told player 2 presses).

- **A wider picture** (`machine.video.wide`, video/video.h; set before every frame): `side` more
  columns on each side of the board's 384, up to 64 (512 columns, the board's whole raster, is 16:9
  where 384 is 4:3). Nothing about the game changes, only what is drawn of it, so the sources say
  how: `scroll[n]` draws a scroll screen moved to the right, `object` says how far each object is
  moved (the stage's go with the view, a panel's stay), and `tile` gives the tiles beyond the
  board's own columns for a scroll screen whose map the game only keeps up as far as its own
  picture shows. `TheGame().widestPicture` tells the window what shape to start in. Vampire Savior's
  `widescreen.cpp` is the example.

- **A switch on the launcher's first page.** A feature with `kFeatureFirstPage` is left out of
  the list the launcher's Features page is made from; the game's `game.cfg` puts its switch under
  "Full screen" with `display = name | Title | What it says` (more of them with `||` between). The
  launcher keeps the switch in its own `launcher.ini` and starts the game with `--features name` or
  `--features -name`, which goes before what `settings.ini` has. The in-game menu still has it.

## Finding things out

- `--dump-ram F:FILE` writes work RAM after frame F (and the video memory and registers beside
  it); `tools/ramfind.py DUMP=VALUE ...` lists the addresses that held known values in several
  dumps. That is how the credit counter, the clock and the health words were found.
- `--poke ADDR=BYTE,...` holds bytes at values, to try a guess.
- `--trace LOW-HIGH:FILE` writes a stretch of work RAM after every frame; with a movie that does
  known things at known frames (walk, punch, crouch, jump) the bytes that change with each show
  what they are. That is how the players' positions and the attack flag were found.
- `--watch LOW[-HIGH]` reports every write the 68000 makes to an address range with the address of
  the instruction that made it; `work/<id>/program.lst` is the disassembly to read it against.
  That is how the damage routine and the clock's tick were found.
- `--shot-every N:PFX` and `tools/sheet.sh` make a contact sheet of a run; `CPS2RECOMP_LAYERS=mask`
  draws only some layers (bit 0 objects, bits 1-3 the scroll screens); `CPS2RECOMP_WIDE=n` draws n
  more columns on each side of the picture (64 at most), to see what a game has beyond its edges.
- `--state-save F:FILE` / `--state-load F:FILE` and movies (`--record`, `--movie`) get back to a
  moment exactly.

## The menu is shared

The menu itself (its look, its rules, the pages made from the features and commands) is the one the
recomp ports share: `recomp-common/menu.{h,cpp}` and `recomp-common/text.h` are a copy of the `recomp-common`
repository's `src/`, made by `tools/sync-common.sh` (which records the commit and the files' hashes in
`recomp-common/COMMIT`) and never changed here. `make check-common` fails if the copy is not what was
recorded; `make check` and `make check-rewrites` run it first. To change the menu, change
recomp-common, run its tests, commit there, then run `tools/sync-common.sh` here and commit the copy.

`port/menu.{h,cpp}` is what is this toolkit's about it: the first page's rows in their three groups
(save states; the pages, Speed, Insert coin, Player 2 joins; Reset, Quit game, Resume), CPS-2's
buttons turned into the menu's six, the speeds, and the games' features and commands handed over.
A game needs nothing more: its features and commands show up from `TheGame()`. A game with pages
that are under another page, or a page that belongs at the head of the menu before the save states
(a page of ways to run the game, say), names them in `TheGame().pages` (`runtime/game.h`, `Page`).
