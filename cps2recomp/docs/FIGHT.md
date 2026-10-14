# Fighting games: the shared training parts

`fight/` holds what a training mode needs that does not depend on the game (`fight/fight.h`); a game's sources
say, from its own memory, what the players are doing, and these keep count and say what to show (the HUD's lines
and boxes, `runtime/game.h`).

| Piece | What it does | What the game tells it |
| --- | --- | --- |
| `Frames` | Startup, active and recovery of each move; the advantage after a hit or a guarded one; a meter of the last 60 steps | Each player once a step (a `StepView`: who is in charge of it, whether it can act, is frozen by a hit, attacking, in the air, has an attack box out), and each hit landing or guarded (`Contact`) |
| `MoveLog` | Every move's numbers as lines of `frame_data.csv` beside settings.ini (character, input, air, which move, startup, active, recovery, advantage) | Each player once a step after `Frames::Step`, and a function that tells a move's start (asked only as one begins) |
| `InputHistory`, `Notation` | What each player holds and for how long, the stick as a keypad number (from the player's side, if the game says which way it faces) | What the game read off the panel each frame |
| `Combos` | The last hit, the combo (hits landing before the player is free again), what of it comes back by itself, the round's best | Each hit's damage, and whether the player was still reeling |
| `Dummy`, `RegisterDummy` | Player 2 stands, crouches, jumps, records and replays (three slots, or any at random), answers with a recording, guards (always, after a hit, at random) high or low; the game's own actions while guarding, while held, getting up | Each frame a `DummyView` (fighting, what it is doing, which side, whether an attack is near enough to guard and must be guarded standing, whether it is knocked down); the game's own actions as `DummyOptions` |
| `StageBox`, `ToPicture` | A box in the stage drawn on the picture through the camera, coloured by its kind | The boxes, read the way the game's own hit tests read them |

The features the dummy registers have the same names in every game (`dummy_stand`, `dummy_guard`, ...), so a
player's `settings.ini` reads the same across the ports.

`runtime/spare.h` holds the machines a game's modes keep as save states and run beside the port's (a stage that
scrolls into the next, the next fight made ready while the last one ends): `RunSpare`, `Promote`, `Take`, `Give`,
and `SetSpareKeeping` for what a game keeps of its own around a spare's frame.
