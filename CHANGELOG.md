# Changelog

The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/). Versions stay
pre-release (`-beta.N`) until every platform's build has been played by hand.

## Unreleased

## 0.1.0-beta.2 - 2026-10-13

- "Alternative modes" at the top of the in-game menu: Training mode and two new modes, each with
  a page of its own. Training's page has Start, Start over, its aids and the dummy's page, which
  were on the first page and on pages of their own.
- Attract mode: the game's own show on a loop with no coin in, with free play on too. All of it,
  the introduction only, or the demonstration fights only.
- Stage viewer: any stage with nobody on it and no panel; scrolled with the stick or by itself
  (slow, medium, fast); the stage's own tune or any other stage's.
- The viewer's "Infinite" stage: every stage in turn, each scrolling into the next, the tunes
  fading from one to the other.
- More pictures of the game in the README.

## 0.1.0-beta.1 - 2026-10-12

- Vampire Savior (set `vsavj`) boots and plays from the player's own ROM set: attract mode, the
  one-player ladder to the ending, two-player games, the operator's test menu.
- Launcher: the ROM set (checked, only read), features (Pure / Enhanced / Custom), controllers
  with keys and buttons that can be chosen for each player.
- In-game menu: save states, speed, coin, reset, pages of features.
- Free play (on in Enhanced), cheats, pause, frame step, slow motion.
- Training mode: direct entry, stage choice, start over, hit boxes, input display, combo readout,
  frame data and a frame meter, a dummy that guards, records and replays.
- Project files for a public repository: notices, contributing rules, the audit (`make audit`),
  CI on macOS, Linux and Windows, release scripts and the release process (`docs/RELEASING.md`).
- Release downloads hold the translated program, as a build from source does; they are built on a
  machine with the ROM set, never by CI.
- `make check-release`: the build with no translated code (the interpreters, what CI builds) checked
  against the translated build.
- The app and its window are called "Vampire Savior Recomp". Pictures of the game in the README.
- GPL-3.0-only, with a licence line on every source file.
