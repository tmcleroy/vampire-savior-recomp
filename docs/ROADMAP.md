# Roadmap

Where the port is against where it is going: a native, readable program with nothing between the
game's rules and the player's machine.

## Where it stands

| Part | Today | |
| --- | --- | --- |
| The game's 68000 program | Translated ahead of time to C++ (about 107,000 instructions, 8,600 routines) ; code no run has reached yet runs in the port's own interpreter | No emulator. Three instructions go through the interpreter one at a time |
| Readable rewrites | Fourteen routines written by hand as named C++ (a frame's hit tests, pushing apart, damage, the round's frame and clock, guard range, whether a hit is guarded, reading the panel), each checked call by call | The rest of the game is still translated code |
| The Z80 sound driver | Translated too (4,498 instructions); the statements are calls of the interpreter's own primitives | |
| Picture | The port's own line renderer for the board's scroll screens and objects, shown through SDL's GPU API | A model of the hardware, not of the game's drawing calls |
| Sound | The port's own model of what the QSound chip's program does | No "Q1" stereo filter; volume and pan curves are approximations |
| Input | Read where the game's own routine reads the panel (a readable rewrite) | Native |
| Settings memory | A model of the board's serial EEPROM chip, kept in a file | A model of the hardware |
| The ROM set's cipher | MAME's implementation (BSD-3-Clause), the only file here from another emulator | To be written again from the published description of the cipher |

## Next

1. **Hand play on every system**: picture, sound, sticks and pads, speed, Steam Deck's Game Mode.
2. **Sound by ear** against a board or a trusted recording; the stereo filter if it can be done
   without the chip's internal ROM.
3. **Training mode**: frame data compared with published tables; throw ranges drawn; gauge and
   Dark Force numbers; an input history that reads motions; a buffered reversal.
4. **More of the fight as readable code**: what happens when a hit lands, the players' own state
   machines, things thrown against each other.
5. **Other releases of the game** (`vsav`, `vsavu`, `vsava`, `vsavh`) and the other CPS-2 games
   the toolkit can take.
6. **Video options**: integer scaling choices, a scanline or CRT shader, all off unless chosen.
