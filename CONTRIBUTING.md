# Contributing

Thanks for helping. Reports from real machines, sticks and pads are the most valuable thing while
this is a beta; code, frame data checked against known tables, and docs are all welcome.

## The rules that keep this project legal

They are not negotiable. Pull requests that break them are closed.

1. **Never add anything from the game.** No ROM, zip or key file, nothing extracted from one, no
   graphics, sound or text dumps, no RAM dumps, no save states (a save state holds the game's
   memory), no screenshots beyond the few kept in `docs/screenshots/` for the README, and no code
   generated from the game: `work/` and `build/` (the deciphered program, the translator's output,
   the listing, the entries files) stay on your machine, and `.gitignore` covers them. The same
   goes for issues: describe what happened or attach an input movie, never game files.
2. **Never add keys or key values**, and never say where keys, ROM sets or other copyrighted files
   can be found.
3. **Nothing from Capcom's own materials or from a leak.** No source code, service or development
   tools, internal documents or symbol names that did not come from your own analysis of your own
   board or ROM set. Say in the pull request how you found what you add (a RAM diff, a write watch,
   a trace), as `games/vampire_savior/PROGRESS.md` does.
4. **Only add code you have the right to contribute**, under the GPL-3.0. Code from another project
   must have a compatible licence, keep its notice, and be listed in `THIRD_PARTY_NOTICES.md`.
   Take facts about hardware from an emulator if you like, and credit it; do not paste its code.

`tools/audit.sh` (`make audit`) checks what can be checked mechanically; CI runs it on every pull
request.

Hand-written C++ that does what a routine of the game does (`games/vampire_savior/src`, the
"readable" rewrites) is welcome: write it yourself from your own reading of the routine, the way
the existing files do, and prove it with `make check-rewrites`.

## What a change must keep

- **With every feature off the game runs exactly as the board does.** Anything that changes how
  the game plays, looks, sounds or is controlled is an optional feature
  (`cps2recomp/docs/FEATURES.md`), off in the launcher's Pure preset.
- **No added input lag and no lost speed.** A change to timing, frame pacing, input reading, sound
  buffering or presentation must be at least as fast and as responsive as before; measure it
  (`--stats`, frames from a press to the game's reaction) and say what you measured.
- **Keyboard and gamepad both work**, for both players, and both can be remapped in the launcher.
- **Nothing outside the frontend talks to a platform.** SDL 3 for the window, input, sound and the
  GPU; no macOS, Windows or Linux calls elsewhere. It must build on all three (CI does).
- **The toolkit is a copy.** `cps2recomp/` is made by the toolkit's `tools/sync-toolkit.sh` and
  `cps2recomp/recomp-common/` by `tools/sync-common.sh`; `make check-toolkit` and
  `make check-common` fail if either was changed here. Changes to them go to their own
  repositories. Anything about this one game belongs in `games/vampire_savior/`.

## Before you open a pull request

[docs/BUILDING.md](docs/BUILDING.md) explains the build with your own ROM set.

```sh
make audit                # always
make check FRAMES=40000 MOVIE=games/vampire_savior/movies/ladder.txt ARGS="--features free_play,no_damage,one_hit"
make check-blocks         # if the translator or the machine changed
make check-rewrites       # if a rewrite or the names header changed
make check-release FRAMES=40000   # if the interpreters, the machine or game code changed
```

Add a line to `CHANGELOG.md` under "Unreleased". Match the code around you: its naming, its
comments (plain words, the routine's address beside its name), C++20, 4 spaces, lines up to about
140 columns.

## Reporting bugs

Open an issue with your system (OS and version, CPU, GPU), your controller, the release or commit,
which features are on (Pure, Enhanced, or the list), what happened and what you expected, and how
to make it happen again. An input movie (`--record FILE`) that shows it is ideal: it holds only
what was pressed.

## Code of conduct and security

See [CODE_OF_CONDUCT.md](CODE_OF_CONDUCT.md) and [SECURITY.md](SECURITY.md).
