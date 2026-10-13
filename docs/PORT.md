# How the port works

Vampire Savior's 68000 program is translated to C++ ahead of time and compiled into the port, where
it runs on a small machine of the toolkit's own (`cps2recomp/`): its video renderer, sound chip,
sound CPU and hardware registers. No emulator is part of it. The toolkit's own notes are in
[cps2recomp/README.md](../cps2recomp/README.md) and `cps2recomp/docs/`; what is known of the game is
in [games/vampire_savior/PROGRESS.md](../games/vampire_savior/PROGRESS.md).

| Path | What |
| --- | --- |
| `cps2recomp/` | The reusable toolkit, a copy of its own repository's (never changed here): the translators (`tools/`), what translated code is written in terms of (`runtime/`), the machine (`machine/`: 68000, bus, interrupts, timing, settings EEPROM, ROM set and cipher), video, sound (Z80, QSound), the frontend (`port/`, SDL 3), the launcher's description, and `recomp-common/` (the in-game menu, settings rules and launcher the recomp ports share) |
| `games/vampire_savior/` | The game: `game.cfg` (the ROM set's files and checksums), `src/` (names, features, readable rewrites), `movies/` (recorded input for the checks), `PROGRESS.md` |
| `work/`, `build/` | Generated files and builds. Never committed: they are derived from the game |

## Two ways the game's program runs

- **Translated**: `make` writes C++ for every routine found in your ROM set's program and compiles it
  in. This is the port: what the checks run, what a build from source gives you, and what a release
  download holds.
- **Interpreted**: the same port built with no translated code runs the program in its own 68000 and
  Z80 interpreters (`machine/m68k.cpp`, `sound/z80.cpp`), on the same machine, with the same features
  and readable rewrites. It needs no ROM set to build, so it is what a fresh checkout is before
  `make explore`, and what CI builds to prove the code compiles and starts on every system. It is
  slower (about half the speed with no window) and takes interrupts between instructions rather than
  between blocks; `make check-release` holds it to the translated build's pictures.

## Translation, the machine, picture and sound

- **The ROM set.** `machine/romset.cpp` reads the set from a folder or zip (files found by
  checksum), checks every file against `game.cfg`, and lays the files out as the board's chips see
  them. A CPS-2 board deciphers its program as the 68000 fetches it, with a key held in
  battery-backed memory; `machine/cps2crypt.cpp` (MAME's, BSD-3-Clause) does that once, with the
  set's key file.
- **Translation.** `tools/recomp.py` starts from the reset vector and the *entries file* (places the
  game was seen to reach by computed jumps, interrupts and returns) and decodes every instruction
  that fixed branches and calls lead to; jump tables in the compiler's usual form are followed too.
  Instructions are cut into blocks and blocks gathered into routines; each routine becomes a C++
  function over the CPU's state, the game's branches as gotos, with the original instruction beside
  each statement:

  ```cpp
  // 006552: addq.w #2,0x8(a5)      546D 0008
  {
      const uint32_t ea1 = c.a[5] + 0x8;
      const uint32_t before = R16(c, ea1);
      W16(c, ea1, c.Add(before, 2, 1, false, false));
      c.cycles += 20;
  }
  ```

  Flags are worked out only where something reads them before the block ends. Work RAM and ROM are
  reached directly, everything else through the machine.
- **Calls.** A call of the game's pushes its return address on the game's stack and calls the
  target routine's C++ function; if that comes back anywhere but the return address (time ran out,
  the game switched stacks, it dropped its return address) the caller returns too, and the run loop
  goes on from wherever the CPU says. Nothing lives on the host's stack that is not also in the
  CPU's state, so this is always right.
- **Code nobody reached.** Routines the game was never seen to enter are translated as well, where
  the program itself shows them: what follows a routine's end, and what tables of addresses point
  at (`tools/guess.py`). A wrong guess costs nothing, since a block only runs when the CPU arrives
  at its start.
- **What has no translation.** Code the build has not seen runs in the port's 68000 interpreter
  (`machine/m68k.cpp`), so the game always works; with `--record-entries` the port notes where
  that happened and the next build translates it. A handful of rare instructions (changes of the status
  register, returns from exceptions, traps) are left to the interpreter one at a time.
- **Time.** A raster line is 1024 CPU cycles; cycle counts are the interpreter's, instruction for
  instruction. Time and interrupts are looked at only where a block starts.
- **The machine** (`machine/machine.cpp`): the memory map, the vertical blanking and raster
  interrupts, inputs, the settings EEPROM (kept in a file), the Z80 and the QSound chip. The
  hardware facts follow MAME's cps2 driver (BSD-3-Clause); the code is cps2recomp's.
- **Picture** (`video/video.cpp`): three scroll screens and the objects, with the CPS-2's object
  priorities, drawn a line at a time so that raster effects land on the right lines.
- **Sound.** The game's Z80 sound driver is translated the same way (`tools/recompz80.py`,
  `tools/z80.py`): its instructions become calls of the routines the port's Z80 interpreter is made
  of (`sound/z80.cpp`), so the two cannot differ in what an instruction does, and the block check
  covers it too. It drives `sound/qsound.cpp`, which does what the QSound DSP's program does with
  the registers (after MAME's high-level QSound). The DSP's internal ROM is not part of a game's
  ROM set, so the parts that need its tables are left out: the stereo "Q1" filter (plain panning
  instead).

## Features and the in-game menu

A game's own sources (`games/<id>/src`) can add optional features: switches the player sets in the
launcher's Features page or the in-game menu (`recomp-common/menu.cpp`, the menu the recomp ports share;
`port/menu.cpp` tells it what a CPS-2 game's rows and buttons are), kept in `settings.ini`. With none
on, the game runs exactly as the board does. Vampire Savior has free play and a few cheats so far
(`games/vampire_savior/src/cheats.cpp`); `--list-features` lists them, `--features a,b` switches
them on for a run.

## Readable code

Where a feature needs the game's rules, the routine is written again by hand as C++ with names
and runs in the original's place, checked call by call against it (`--check-rewrites`,
`make check-rewrites`; [cps2recomp/docs/READABLE.md](../cps2recomp/docs/READABLE.md)). Vampire
Savior: a frame's hit tests, pushing, damage, the round's clock, guard range, the control panel
(`games/vampire_savior/src/collision.cpp`, `fight.cpp`, `input.cpp`).

## Checking

See [cps2recomp/docs/VERIFYING.md](../cps2recomp/docs/VERIFYING.md). In short: the check build runs
every translated block on the side, beside the interpreter, and compares registers, flags, cycle
counts and every write; and the normal build's state, hashed every frame, must equal that of the
interpreter run block by block.

## The launcher and the settings

The launcher (`make launcher`) is the one the recomp ports share: `cps2recomp/recomp-common/launcher/`, a copy of
[recomp-common](https://github.com/tmcleroy/recomp-common)'s made by `tools/sync-common.sh` and never changed
here. What is this toolkit's is `cps2recomp/launcher/src/description.cpp`: the words for a ROM set, the check of the player's (the same reading the game does), what the game is
started with, and the buttons that can be chosen (`port/bindings.h`).
The launcher and the game read and write `settings.ini` by the same rules (`recomp-common/settings.h`): the
`[features]` section (a preset, and a value for each feature the player set) and the `[keyboard1]` /
`[gamepad1]` sections (which key and which gamepad button does what). Changing one feature, in the launcher
or in the game's menu, leaves every other line of the file as it was.
A feature that is a choice of several things (the dummy's guard, the recording slot) is a dropdown in the
launcher, and its number is kept.

The launcher's pages can be pictured with no window, to compare before and after a change:

```sh
SDL_VIDEO_DRIVER=dummy SDL_RENDER_DRIVER=software VAMPIRE_SAVIOR_HOME="$HOME_FOR_TEST" build/vampire_savior/vampire_savior-launcher --screenshot "$PREFIX"
```

The launcher's look, and the files that make it, come from the launcher of
[wiiparty-recomp](https://github.com/arelkair/wiiparty-recomp) by arelkair (GPL-3.0-or-later); they are in
`cps2recomp/recomp-common/launcher/`, with the notices in recomp-common's `THIRD_PARTY_NOTICES.md`. Its build
downloads SDL 3 (zlib license), Dear ImGui (MIT) and the Inter font (SIL OFL 1.1, compiled into the
program): a launcher given out as a binary must carry those three licenses.