# cps2recomp

A toolkit for native ports of Capcom CPS-2 arcade games by static recompilation. A game's 68000
program is translated to C++ ahead of time and compiled into the port, where it runs on a small
machine of the toolkit's own: video, the QSound chip, the sound CPU (its driver translated too),
the board's registers and settings memory. No emulator is part of a port, and no game data is
part of this repository or of a game's.

| Path | What |
| --- | --- |
| `tools/` | The translators (`recomp.py`, `m68k.py`; `recompz80.py`, `z80.py`), `gamecfg.py`, the checks (`check.sh`, `check_sound.sh`), finding a game's code by made-up play (`explore.sh`), reading the listing (`lst.py`, `tracechanges.py`, `ramfind.py`), `sync-toolkit.sh`, `sync-common.sh` |
| `runtime/` | What translated code is written in terms of; hooks and readable rewrites (`lift.h`); features and settings (`game.h`) |
| `machine/` | The 68000, bus, interrupts, timing, settings EEPROM, ROM set and cipher |
| `video/`, `sound/` | The picture; the Z80 and QSound; Capcom's sound driver (version 2.01b) as ordinary code (`driver.cpp`), used in place of the Z80 where a game has that version |
| `port/` | The frontend (SDL3), putting the picture on the screen (`present.*`: SDL's GPU API, so Metal, Vulkan or Direct3D 12, with SDL's renderer where there is no device; shaders in `shaders/`, compiled into `shaders.inc` by `tools/gen_shaders.py`), in-game menu adapter, HUD, control bindings |
| `recomp-common/` | A copy of [recomp-common](https://github.com/tmcleroy/recomp-common) (the in-game menu, the settings rules, the launcher), made by `tools/sync-common.sh`; never changed here |
| `launcher/` | What this toolkit tells the launcher the recomp ports share (`recomp-common/launcher`; SDL3 + Dear ImGui): `src/description.cpp` has the words for a ROM set, the check of the player's, the game's arguments and the buttons that can be chosen |
| `fight/` | What a fighting game's training mode has in common: frame data and its meter, the input display, the combo readout, the dummy, boxes on the picture (`docs/FIGHT.md`) |
| `docs/` | `VERIFYING.md`, `READABLE.md`, `FEATURES.md`, `FIGHT.md` |

## How a game uses it

A game is its own repository:

```
cps2recomp/     a copy of this repository, made by cps2recomp/tools/sync-toolkit.sh (never changed there)
games/<id>/     game.cfg (the ROM set's files and checksums), src/ (names, features, rewrites), movies/
Makefile        GAME ?= <id>;  CR ?= cps2recomp;  include $(CR)/build.mk
```

`build.mk` has the targets (`make`, `make explore`, `make check`, `make check-blocks`,
`make check-rewrites`, `make launcher`, `make app`). The checks run `make check-toolkit`, which
fails if the game's copy of the toolkit is not what `sync-toolkit.sh` recorded.

## Working on the toolkit

Change it here, never in a game's copy. Build and check a game against this clone from the game's
repository:

```sh
make CR=../cps2recomp
make CR=../cps2recomp check
```

Commit here, then in each game: `cps2recomp/tools/sync-toolkit.sh`, run the game's checks, commit.
Anything about one game belongs in that game's `game.cfg` or `src/`, not here.

## Checking the toolkit itself

`make check` in this repository needs no game: it audits the files (`tools/audit.sh`: nothing from a
game, no keys, no personal paths), checks the copy of recomp-common, builds the program with no
translated code and the launcher for the stand-in game in `example/`, starts both, and tries
`sync-toolkit.sh`; CI does the same on macOS, Linux and Windows. See [CONTRIBUTING.md](CONTRIBUTING.md).
A game's copy leaves out these checks, the stand-in game and this repository's project pages.

## License and credits

GPL-3.0-only. The cipher (`machine/cps2crypt.*`) is from MAME (BSD-3-Clause; its header says so), and the
QSound model follows MAME's; [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) has the notices in full.

cps2recomp is not affiliated with or endorsed by Capcom. "Capcom", "CPS-2" and "QSound" are names and
trademarks of their owners, used here only to say what the toolkit works with. No game is included: a port
needs the ROM set of a board its player owns.

The launcher's look, and the files that make it, come from the launcher of
[wiiparty-recomp](https://github.com/arelkair/wiiparty-recomp) by arelkair (GPL-3.0-or-later); they are in
`recomp-common/launcher/`, with the notices in recomp-common's `THIRD_PARTY_NOTICES.md`. Its build downloads
SDL 3 (zlib license), Dear ImGui (MIT) and the Inter font (SIL OFL 1.1, compiled into the program): a
launcher given out as a binary must carry those three licenses.
