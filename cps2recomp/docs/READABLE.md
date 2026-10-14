# Readable code: rewriting the game's routines

Translated code (`tools/recomp.py`) is the game's instructions one by one. Where a feature needs
the game's *rules* (what a hit is, how damage is taken, when the round ends), the routine is
written again by hand as ordinary C++ with names, in the game's sources (`games/<id>/src`), and
runs in the original's place. `runtime/lift.h` has the pieces.

```cpp
// 018AB0. Takes the damage of the hit being dealt off the health of the player at A1.
void TakeDamage(M68K &c) { ... }

AddRewrite(kTakeDamage, "TakeDamage", TakeDamage, kFreeFlags);
```

## The rule

A rewrite does exactly what the routine it replaces does: it leaves the same bytes in the game's
memory and the same values in the registers, and returns as the routine does with `rts` (the port
does the return). Registers the original changes and no caller reads can be named as *free*
(`kFreeD0`, `kFreeA2`, `kFreeFlags`...), and then need not match.

With every feature off, a rewrite is the game. A feature is a few lines inside it, at the place in
the rules it changes (`if (FeatureOn("no_damage")) ...`).

## What a rewrite can use

- The CPU: `c.d[n]`, `c.a[n]`, the flags; `c.Add`, `c.Sub` (the 68000's arithmetic with its
  flags, for the places where a caller reads them).
- Memory: `Ram8/16/32`, `SetRam...` (work RAM); `Mem8/16/32(c, address)` (anything, the ROM's
  tables too); a game's header gives the names (`vsav.h`: `Var(c, offset)` for the variables the
  game reaches from A5, the fields of the objects' records).
- Other rewrites: as plain C++ calls.
- The game's own routines that are not rewritten: `Call(c, address)` runs one to its return. It
  must not be one that waits (for the next frame, for a task switch).
- A routine that waits only in some state: `RewriteWhen(address, when)` after its `AddRewrite`; `when`
  is asked before each call, and when it says no the game's own code runs that time. The rewrites
  that would call it in that state have to stand aside the same way.

## Checking

`--check-rewrites` runs, at every call of a rewritten routine: the rewrite; then, with everything
put back, the game's own code for it (by the interpreter); and compares registers, flags and all
of work RAM (but for the stack below the stack pointer). A rewrite that writes video RAM (a scroll
screen's map, the row scroll table) names `kChecksVideo` beside its free registers, and that is compared too. The game goes on from what its own code
did. Differences are reported with the rewrite's name and the call's number:

    rewrite HitTest (017FF4), call 912, differs: [FF3459] 00 (the game's: 77, was 77)

and at the end each rewrite's calls and how many differed.

While a rewrite is being checked, the rewrites it uses are not checked again; so to check an inner
one by itself, leave the outer ones out: `CPS2RECOMP_SKIP_REWRITES=HitTest,AllHitTests`.
`--no-rewrites` runs the game's own code everywhere.

`make check-rewrites` runs the game's `check_rewrites.sh`: every movie, with the outer rewrites
left out layer by layer, so that every rewrite is checked on its own calls. The runs are each other's
business only at the end: `tools/at-once.sh` (sourced) runs them a few at a time and prints what each
wrote in order (`at_once run ...`, then `at_once_wait`).

`make check` (translated code against the interpreter run the same way) runs with the rewrites in
both, so it covers the hooks' and rewrites' calls being made at the same places either way.

## How to find and write one

1. Find the routine: `--watch ADDR` names the instruction that writes a value you know;
   `work/<id>/program.lst` is the disassembly (`>` marks where a block starts; a rewrite's address
   must be one).
2. Read it to its `rts`. Name what it reads and writes in the game's header.
3. Write it, keeping the order of anything a callee could see. Put in the registers what the
   original leaves there, or name them free if the callers do not read them (look at the callers).
4. `AddRewrite`, `make` (the translator picks the address up from the sources), then
   `--check-rewrites` on movies that exercise it.

## Vampire Savior's rewrites

| Routine | Address | File |
| --- | --- | --- |
| ReadControls: the control panel, once a frame | 001576 | `input.cpp` |
| RoundFighting: a frame of a round: knock-outs, the clock, the round's end | 0097DC | `fight.cpp` |
| AllHitTests, PlayersAgainstPlayers, ThrownAgainstPlayers: a frame's hit tests | 017F0C, 017F22, 0193B8 | `collision.cpp` |
| HitTest, AttackAgainst: does this attack hit that object, and where | 017FF4, 018054 | `collision.cpp` |
| BoxesOverlap, ClearHit | 019172, 01923C | `collision.cpp` |
| PushApart: the players cannot stand in each other | 01926A | `collision.cpp` |
| TakeDamage: a hit's damage off a player's health | 018AB0 | `fight.cpp` |
| HitLands: a hit has landed: guarded or not (by the attack's kind and how the player stands), armour | 018230 | `fight.cpp` |
| GuardRange, GuardRangeAttack: is an attack near enough to guard | 027694, 0276A6 | `fight.cpp` |
