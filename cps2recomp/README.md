# cps2recomp

[![CI](https://github.com/tmcleroy/cps2recomp/actions/workflows/ci.yml/badge.svg)](https://github.com/tmcleroy/cps2recomp/actions/workflows/ci.yml)
[![Release](https://img.shields.io/github/v/release/tmcleroy/cps2recomp)](https://github.com/tmcleroy/cps2recomp/releases)
[![License: GPL-3.0](https://img.shields.io/badge/license-GPL--3.0-blue)](LICENSE)

**A toolkit for making native PC ports of Capcom CPS-2 arcade games by static recompilation.** A
game's 68000 program is translated to C++ ahead of time and compiled into the port. There it runs on
a small machine written for this toolkit: the video, the QSound chip, the sound CPU (with its driver
translated too, or rewritten as ordinary code), the board's registers and its settings memory. No
emulator is part of a port, and no game data is in this repository or in a game's.

> cps2recomp is not affiliated with or endorsed by Capcom. "Capcom", "CPS-2" and "QSound" are names
> and trademarks of their owners, used here only to say what the toolkit works with. **No game is
> included**: a port needs the ROM set of a board its player owns.

## Ports made with cps2recomp

| Port | Game | What it adds |
| --- | --- | --- |
| [**Vampire Savior Recomp**](https://github.com/tmcleroy/vampire-savior-recomp) ([download](https://github.com/tmcleroy/vampire-savior-recomp/releases)) | *Vampire Savior: The Lord of Vampire* (Capcom, 1997) | A full training mode (hit boxes, frame data and a frame meter, input display, a dummy that records, replays and guards), widescreen, free play, save states, key and button remapping. macOS, Linux (Steam Deck included) and Windows |

[![Vampire Savior Recomp's training mode: hit boxes, input display, frame data and the frame meter](https://raw.githubusercontent.com/tmcleroy/vampire-savior-recomp/main/docs/screenshots/training.png)](https://github.com/tmcleroy/vampire-savior-recomp)

To play, download a port, not this toolkit. This repository is for people making or working on
ports.

## What a port gets from it

- **Translation ahead of time.** `tools/recomp.py` turns the game's 68000 program into C++, and
  `tools/recompz80.py` does the same for the sound CPU's. The code is found by reading the program
  and by made-up play (`tools/explore.sh`). Code it has not found yet runs in an interpreter, and is
  counted, so that the count can be driven to zero.
- **A machine of its own, not an emulator.** The 68000, the bus, interrupts and timing, the settings
  EEPROM, the ROM set and its cipher (`machine/`), the picture (`video/`), and the Z80 and QSound
  (`sound/`). Capcom's sound driver, version 2.01b, is also there as ordinary C++ (`sound/driver.cpp`),
  used in place of the Z80 for games that have that version.
- **A native picture.** SDL's GPU API (Metal, Vulkan or Direct3D 12), with SDL's renderer where
  there is no GPU device, and a wider picture (up to 16:9) for games whose sources ask for it. The
  Display page lets players choose how it fits the window: sharp, whole pixels or smooth; 4:3, square
  pixels or the whole screen; and the window's size.
- **Readable rewrites, with proof.** A game's routines can be rewritten by hand as ordinary C++. A
  checker runs the original and the rewrite on every call and compares memory, registers, timing and,
  when asked, video RAM ([docs/READABLE.md](docs/READABLE.md)).
- **A training mode's parts.** `fight/` has what fighting games share: frame data and a frame meter,
  a log of every move, the input display in numpad notation, the combo readout, a dummy that stands,
  crouches, jumps, guards, records and replays, and hit boxes drawn through the game's camera. A
  game's sources only say, from its own memory, what the players are doing
  ([docs/FIGHT.md](docs/FIGHT.md)).
- **Players' own colours.** Text files a character, or a ROM file changed with PalMod, recolour the
  characters, and a colour editor in the model viewer makes them; a game only says where its colours
  are (`fight/colours.h`).
- **Features players expect.** Save states, run-ahead (a press seen a frame or two sooner, with the
  game running exactly as without it), speed, pause and frame advance, slow motion, cheats and
  options a game declares once, and the in-game menu and launcher the recomp ports share
  ([recomp-common](https://github.com/tmcleroy/recomp-common); [docs/FEATURES.md](docs/FEATURES.md)).
  Every change to how a game plays is opt-in: with all features off, the game runs as the board.
- **Checks at every step.** Translated code is checked against the interpreter, the sound driver's
  every chip write against the Z80's, and runs of the same input movie must give the same per-frame
  hashes ([docs/VERIFYING.md](docs/VERIFYING.md)).
- **macOS, Linux (Steam Deck included) and Windows** through SDL 3. CI builds and starts the toolkit
  on all three.

## Trying it with no game

`make check` needs no game. It audits the files (`tools/audit.sh`: nothing from a game, no keys, no
personal paths), checks the copy of recomp-common, builds the program with no translated code and
the launcher for the stand-in game in `example/`, starts both, and tries `sync-toolkit.sh`.

```sh
git clone https://github.com/tmcleroy/cps2recomp
cd cps2recomp
make check
```

You need a C++20 compiler (Clang, GCC or MinGW-w64), CMake, Ninja, Python 3, zlib and SDL 3. On
macOS that is `brew install cmake ninja sdl3`. [CONTRIBUTING.md](CONTRIBUTING.md) has the rest, and
`.github/workflows/ci.yml` shows exactly what each system installs.

## How a game uses it

A game is its own repository:

```
cps2recomp/     a copy of this repository, made by cps2recomp/tools/sync-toolkit.sh (never changed there)
games/<id>/     game.cfg (the ROM set's files and checksums), src/ (names, features, rewrites), movies/
Makefile        GAME ?= <id>;  CR ?= cps2recomp;  include $(CR)/build.mk
```

`build.mk` has the targets (`make`, `make explore`, `make check`, `make check-blocks`,
`make check-rewrites`, `make launcher`, `make app`). The checks run `make check-toolkit`, which
fails if the game's copy of the toolkit is not what `sync-toolkit.sh` recorded. `example/` is a game
repository with no game in it, and
[Vampire Savior Recomp](https://github.com/tmcleroy/vampire-savior-recomp) is a complete one: start
from either.

The player's ROM set is only read. The program is deciphered with the set's own key file, as the
board does when it starts; no key is in this repository or in a game's.

## Working on the toolkit

Change it here, never in a game's copy. Build and check a game against this clone from the game's
repository:

```sh
make CR=../cps2recomp
make CR=../cps2recomp check
```

Such a build goes to `build/<id>-dev`, apart from the game's usual one (the check scripts follow:
`B=build/<id>-dev`). Commit here, then in each game: `cps2recomp/tools/sync-toolkit.sh`, run the
game's checks, commit. Anything about one game belongs in that game's `game.cfg` or `src/`, not here.

## Repository layout

| Path | What |
| --- | --- |
| `tools/` | The translators (`recomp.py`, `m68k.py`; `recompz80.py`, `z80.py`), `gamecfg.py`, the checks (`check.sh`, `check_sound.sh`, `at-once.sh`), finding a game's code by made-up play (`explore.sh`), reading the listing (`lst.py`, `tracechanges.py`, `ramfind.py`), `gen_shaders.py`, `sync-toolkit.sh`, `sync-common.sh`, `audit.sh` |
| `runtime/` | What translated code is written in terms of; hooks and readable rewrites (`lift.h`); features and settings (`game.h`); save states (`state.h`); spare machines a game's modes run beside the port's (`spare.h`) |
| `machine/` | The 68000, bus, interrupts, timing, settings EEPROM, ROM set and cipher |
| `video/`, `sound/` | The picture; the Z80 and QSound; Capcom's sound driver (version 2.01b) as ordinary code (`driver.cpp`) |
| `port/` | The frontend (SDL 3), putting the picture on the screen (`present.*`; shaders in `shaders/`, compiled into `shaders.inc` by `tools/gen_shaders.py`), the in-game menu adapter, the HUD, control bindings |
| `fight/` | What a fighting game's training mode has in common, and the players' own colours ([docs/FIGHT.md](docs/FIGHT.md)) |
| `recomp-common/` | A copy of [recomp-common](https://github.com/tmcleroy/recomp-common) (the in-game menu, the settings rules, the launcher), made by `tools/sync-common.sh`; never changed here |
| `launcher/` | What this toolkit tells the shared launcher (`recomp-common/launcher`; SDL 3 + Dear ImGui): `src/description.cpp` has the words for a ROM set, the check of the player's, the game's arguments and the buttons that can be chosen |
| `example/` | A stand-in for a game's repository, with no game: what `make check` here builds |
| `docs/` | `VERIFYING.md`, `READABLE.md`, `FEATURES.md`, `FIGHT.md` |

A game's copy leaves out this repository's own checks, the stand-in game and the project pages
(`.gitattributes`).

## Related projects

| Project | What |
| --- | --- |
| [satrecomp](https://github.com/tmcleroy/satrecomp) | The same kind of toolkit for Sega Saturn games |
| [recomp-common](https://github.com/tmcleroy/recomp-common) | The in-game menu, settings rules and launcher this toolkit and satrecomp share |
| [Muscle March Recomp](https://github.com/tmcleroy/muscle-march-recomp) | A native port of *Muscle March* (WiiWare), by the same author |
| [doaxvb-re](https://github.com/NoRain211/doaxvb-re) | A native Windows port of *Dead or Alive Xtreme Beach Volleyball* (Xbox), by NoRain211; not made with these toolkits |

## Contributing

Bug reports and pull requests are welcome: see [CONTRIBUTING.md](CONTRIBUTING.md). The hard rules:
nothing from any game, never a key or a tool that writes one, nothing from leaked material, and no
emulator's code beyond what the notices list. Security reports go by [SECURITY.md](SECURITY.md).

## License and credits

[GPL-3.0-only](LICENSE). [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) has the notices in full.

- The program cipher (`machine/cps2crypt.*`) is MAME's (BSD-3-Clause; its header says so), worked
  out by Andreas Naive and Nicola Salmoria. The QSound model follows MAME's.
- The launcher's look, and the files that make it, come from the launcher of
  [wiiparty-recomp](https://github.com/arelkair/wiiparty-recomp) by arelkair (GPL-3.0-or-later). They
  are in `recomp-common/launcher/`, with the notices in recomp-common's `THIRD_PARTY_NOTICES.md`.
- The launcher's build downloads SDL 3 (zlib license), Dear ImGui (MIT) and the Inter font (SIL OFL
  1.1, compiled into the program). A launcher given out as a binary must carry those three licenses.
