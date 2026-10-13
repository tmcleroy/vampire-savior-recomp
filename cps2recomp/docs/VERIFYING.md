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
