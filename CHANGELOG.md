# Changelog

The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/). Versions stay
pre-release (`-beta.N`) until every platform's build has been played by hand.

## Unreleased

- Widescreen (a switch on the launcher's first page, under "Full screen", and on the in-game
  menu's Display page; off unless switched on, in every preset, because it changes how the game
  plays): fights are shown and played in 16:9, with more of the stage on each side. The players
  can go to the wide picture's edges (464 apart at most, where the board allows 336), the camera
  stops where the wide picture comes to the stage's ends, and the stage's pieces and the things
  thrown that the game only keeps while they are in the picture are kept while they are in the
  wide one. The bars stay in the middle. Survival, the endless battle, the ring out and the
  viewer's infinite stage are wide too. With it off the game is exactly the board's. Known: some
  pieces of a stage come in late at the wide picture's edge, and at a stage's very ends a few
  columns of the planes behind are not painted.
- Endless battle, the stages scrolling by: the next stage is made ready in time for the end of
  the one shown (in the wide picture a stage is crossed sooner, and at the fast speed the
  scrolling waited about a second at each stage's end).
- 55 of the 58 scripts of the stages' pieces are readable C++ too: every one a stage was seen to use, each checked against the game's own
  on its stage.
- The stage's code is readable C++ (58 routines: the camera, the scroll screens' placing, the row
  scroll table, what is in the picture, the players' edges, the stage's pieces), each checked
  against the game's own on every stage; widescreen is lines in it. With widescreen on, the lady
  under the bird cage on Feast of the Damned is there whenever she is in the wide picture.
- Widescreen: a piece of a stage that the game shows only while the camera is one side of a place
  (most of the people and things in the backgrounds) is shown whenever it is in the wide picture; the
  lady on Feast of the Damned went while part of her was still in it.
- Widescreen: no black bands at a stage's ends, or between stages in the viewer's infinite stage and
  the endless battle's scrolling stages: the wall and the planes behind keep inside their painting
  (what goes by more slowly than the camera goes by more slowly still), and the building under the
  helicopter on the Tower of Arrogance is where it belongs.
- Widescreen: on every stage, the pieces that the game leaves alone until the camera is near them
  are animated and shown whenever they are near the wide picture, and the three other pieces that
  are one thing or another by the camera's side (Concrete Cave, Tower of Arrogance, Abaraya) are
  both there.
- Widescreen: both groups of dancers on Feast of the Damned are there at once (the board shows one
  or the other, by which side of a place the camera is).

## 0.1.0-beta.4 - 2026-10-13

- Endless battle: how often the shuffled stage changes (1, 2, 3, 5 or 10 minutes; it was two) and
  how fast the scrolling stage goes by (slow, medium, fast) are chosen on its page.
- The launcher can be used with a gamepad alone, or with the arrow keys and Space or Enter: a ring
  shows what is chosen, A presses it, B goes back, the shoulder buttons change the page. With a
  gamepad, Choose opens a list of folders and files in the launcher's own window.
- In the in-game menu Space, Enter and a gamepad's A choose and Backspace and B go back, whatever
  the game's buttons are set to.
- Sound can be switched off: a "Muted" switch in the launcher (off unless switched on) and a
  "Sound" row in the in-game menu.
- A gamepad's left trigger goes on by a frame only once the game has been stopped (`P`, the right
  stick's click); it no longer stops the game itself, which a touch of it while playing did, with
  nothing on the trigger to start it again.

## 0.1.0-beta.3 - 2026-10-13

- Three more modes under "Alternative modes". Survival: one opponent after another on the health
  you have left, with the count at the end. Endless battle: two fighters the game plays, with no
  panel, the loser replaced; either can be taken over from the controls; on one stage, on stages
  that fade into one another, or on a floor that scrolls through them. Ring out: no health, a
  stage's ends are open, and the winner jumps down to the next stage.

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
