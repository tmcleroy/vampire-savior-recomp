# Checking translated code

The port has one 68000 interpreter (`machine/m68k.cpp`) and the translated code. They share the
arithmetic and flag routines (`machine/m68k.h`) and count the same cycles, so they can be compared
exactly. The same goes for the Z80 sound driver (`sound/z80.cpp`, `runtime/recomp_z80.cpp`):
`--cpu` sets how both CPUs are run, and both checks below cover both.

## Run modes (`--cpu`)

| Mode | What runs |
| --- | --- |
| `native` | Translated code; the interpreter for what has no translation. The default when the build has translated code |
| `blocks` | The interpreter, looking at time and interrupts only where translated code would (at block starts). Does exactly what `native` does, cycle for cycle |
| `interp` | The interpreter an instruction at a time. Interrupts land a few instructions earlier than in the other two, so its runs are not the same frame for frame |
| `check` | The check build only (below) |

## `make check-blocks` (FRAMES=n, MOVIE=path or ARGS="--auto SEED:FROM")

Builds `<game>-check`: the same sources with `CPS2RECOMP_CHECK`, in which translated code leaves
its routine after every block and its memory writes go to a list instead of memory. For each block
the run loop (`M68K::RunChecked`, `runtime/recomp.cpp`) runs the translated block on the side,
puts the registers back, runs the interpreter over the same block for real, and compares: every
register, the flags, the cycle count, where control went, and the writes (address, size, value, in
order). The first difference stops the run with both states printed.

This finds a wrong translation at the block where it first matters, whatever it would have led to.

## `make check` (FRAMES=n, MOVIE=path)

Runs the normal build three times with the same input (a movie, or `--auto 7:400`) and compares
the per-frame state hashes (`--hash-log`: CPU, RAM, video and object memory, registers, picture):
`blocks` against `native`, and `native` against itself. This covers what the block check cannot:
routines calling one another on the host's stack, and time running out inside them.

## Input

- `--auto SEED[:FROM]`: made-up play (coins, start, sticks and buttons at random); the same seed
  gives the same run.
- `--record FILE` / `--movie FILE`: what the players held, by frame.
- `tools/explore.sh FIRST RUNS FRAMES`: made-up play in several runs side by side, adding what
  they find to the entries file.

## Other aids

- `--shot-every N:PFX` (PNG), `--audio-dump FILE` (WAV), `--state-save F:FILE` / `--state-load`.
- `--stats`: speed, and how many instructions had no translation.
- `CPS2RECOMP_UNMAPPED=1`: reads and writes of addresses nothing answers at.
- `work/<id>/program.lst`: the disassembly of everything the translator decoded (`>` marks a
  block's start).

## The sound driver (`--sound`, `make check-sound`)

Capcom's QSound driver is one program in several versions. For version 2.01b (April 1996) the
toolkit has it as ordinary code (`sound/driver.cpp`), and a game whose sound ROM has that version
runs it in place of the Z80 (`--sound driver`, the default then; `--sound z80` for the program
itself). Other versions keep the Z80.

`--sound check` runs both. The Z80 does the real work; the driver, beside it, takes each command
when the Z80's interrupt takes it and does each piece of work (a command, a tick of the sequences,
a tick of the ADPCM voices) when the Z80 starts on it. Every write to the chip, from each, goes in
a list, and the lists must be the same: register, value and order. The first difference stops the
run with the writes around it and both sides' state for the voice.

`--sound-test SEED[:N]` gives the sound side a command every N frames in place of the game's: each
song in the ROM in turn, placed one of the eight ways with made-up numbers, and now and then one of
the other commands. It also puts made-up songs in the ROM's empty end of the kinds the ROM may have
none of (single samples, ADPCM samples, sequences with the rarer commands). Every fourth seed runs
in mono. With these, all of the driver's code runs under the check except its guards against
numbers out of range and one clamp of the ADPCM volume.

`make check-sound` (`tools/check_sound.sh`) runs eight sound tests and a run of made-up play under
`--sound check`, and compares a run with the driver against one with the Z80 by the game's side of
the state (`--hash-game`: the 68000, its memory, the memory the two share and the picture).

What is not the same as on the Z80 is when things happen, within a few thousandths of a second.
The Z80 takes time over its work (its multiplication alone is 700 cycles and more) and the chip
takes one write a sample, so voices that start on the same tick start a few samples apart, and when
the Z80 is behind it does things in another order than it would have. The driver is never behind,
and its writes reach the chip paced as the Z80's are: each write carries the cycles the program
would have taken to come to it (multiplications by their bits, the rest by averages measured from
the Z80: `CPS2RECOMP_SOUND_GAPS=1` with `--sound check` prints both by write), and the machine
makes at most one write a sample. Without the pacing, voices playing the same thing in unison came
out louder than on the board. With it, a 200-second run's sound is within 3% of the Z80's in
loudness in every second; it is not the same sample for sample.

`CPS2RECOMP_SOUND_COMMANDS=1` prints every command the sound side takes.
