# Third-party notices

This project is licensed under the GNU General Public License v3.0 (see `LICENSE`). Most of it was
written for this project and its toolkit. What follows is everything that was not, and everything
a built program contains that is someone else's.

## Code in this repository from other projects

### MAME (the CPS-2 program cipher)

`cps2recomp/machine/cps2crypt.cpp` is MAME's `src/mame/capcom/cps2crypt.cpp`: the tables and the two
Feistel networks are as they are there, and only the entry point at the end of the file was written
here. The cipher was worked out by Andreas Naive and Nicola Salmoria. **BSD-3-Clause**:

```
Copyright (c) Paul Leaman, Andreas Naive, Nicola Salmoria, Charles MacDonald
All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted
provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this list of conditions
   and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright notice, this list of
   conditions and the following disclaimer in the documentation and/or other materials provided
   with the distribution.
3. Neither the name of the copyright holder nor the names of its contributors may be used to
   endorse or promote products derived from this software without specific prior written
   permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR
IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
```

It is compiled into the game program and the launcher, so both carry this notice.

Three more pieces were written here but follow MAME closely enough to be credited as coming from it
(all BSD-3-Clause there; the notice above covers the same terms, with these authors):

| Here | After | What |
|---|---|---|
| `cps2recomp/sound/qsound.cpp`, `qsound.h` | `src/devices/sound/qsoundhle.cpp`, by superctr and Valley Bell | What the QSound chip's program does with its registers: the voices, the echo, the two start-up states. Written again with this project's names; the pan curve is this project's own. |
| `cps2recomp/machine/romset.cpp`: `Unshuffle`, `DecodeKey` | `src/mame/capcom/cps2.cpp` | How the graphics ROMs' address lines are untangled, and the layout of a key file (about thirty lines). |

The rest of the machine (`machine/`, `video/`, `sound/z80.cpp`) is this project's code. It uses
facts about the hardware that MAME's CPS-1/CPS-2 drivers record (register layouts, priority rules,
timings); the files that do say so.

### wiiparty-recomp (the launcher's look)

`cps2recomp/recomp-common/` is a copy of [recomp-common](https://github.com/tmcleroy/recomp-common)
(GPL-3.0-only; the in-game menu, the settings rules and the launcher the recomp ports share). Its
launcher descends from the launcher of **wiiparty-recomp** by **arelkair**
(https://github.com/arelkair/wiiparty-recomp), licensed **GPL-3.0-or-later**:
`recomp-common/launcher/src/ui.cpp`, `ui.h` and `embedded.h` are that project's files nearly
unchanged and are marked so; `main.cpp`, parts of `app.cpp`, `game_process.cpp` and
`embed_fonts.cmake` derive from it. recomp-common's own `THIRD_PARTY_NOTICES.md` has the table file
by file.

## Libraries a build downloads or links

None of these is stored in this repository.

- **SDL 3** (3.4.16 in release builds): window, input, sound, the GPU API. **zlib License**.
  Copyright (C) 1997-2026 Sam Lantinga. Linked into the game and the launcher.
- **zlib**: reading a ROM set's zip and its checksums. **zlib License**. Copyright (C) 1995-2024
  Jean-loup Gailly and Mark Adler. The system's on macOS and Linux; `zlib1.dll` in the Windows archive.
- **Dear ImGui 1.92.9b**: the launcher's user interface, compiled into it. **MIT License**.
  Copyright (c) 2014-2026 Omar Cornut.
- **Inter 4.1 (Regular and SemiBold)**: the launcher's font, **compiled into the launcher
  program**. **SIL Open Font License 1.1**. Copyright (c) 2016 The Inter Project Authors.
- On Windows, the MinGW-w64 runtime (libstdc++, libgcc, winpthreads) is linked in: GPL-3.0 with the
  GCC Runtime Library Exception, and the MinGW-w64 runtime licence.

A release archive holds these licences in its `licenses/` folder (`tools/release/` puts them there
and `tools/release/check_archive.sh` fails without them).

## The font of the in-game menu

`cps2recomp/recomp-common/text.h` contains a 5x7 dot-matrix font for ASCII 32 to 126: the generic
dot-matrix character set of LCD and LED display code, typed in for recomp-common and not copied
from a file of any particular project.

## Nothing from any game

No game's code, data, text, graphics, sound or keys are in this repository or in its release
archives. The player supplies their own ROM set. See the README.

Code added later from another project must have a licence compatible with the GPL-3.0 and be listed
here, with its notice kept in the file. `tools/audit.sh` fails if a file marked as another
project's is not one of those listed above.
