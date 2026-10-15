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

## Custom colours and the colour editor

`fight/colours.h` gives a game the players' own colours for its characters and an editor to make them, as
Vampire Savior had them first. A game registers a `colours::Description` (in its `RegisterGame`) and calls
`colours::BeforeFrame` before its viewer's own work each frame; its model viewer calls `EditorBeforeFrame`
(after its round is on, before it shows its animation), `EditorFrame` (after its picture is made),
`EditorShift` (the character drawn that far left) and `EditorAnimation` (the choice to use for "Animation"
while the editor is on), and `EndEditor` when its own Start is chosen.

| The game tells | What for |
| --- | --- |
| `characters` (number, name, the name abroad), `setNames`, `palettesInSet`, `lineNames` | The files' names and sections, the editor's rows |
| `setAddress`, `ranges` | Where a set is in the ROM image (`Program`), and every place of the characters' colours (sets and effects) |
| `romFile`, `romFileAt`, `romFileSize`, `same`, `romFileSwapped`, `romFileWhose` | The ROM file PalMod changes: only its words inside `ranges` are taken (the rest are counted and left), and the file is refused unless `same` is the game's |
| `featureDescription`, `originalHelp` | The feature's words in the menu, and the remark at the head of each file "Write the game's colours" writes |
| `showSet` | The editor's preview: player 1 made to use the set, and where its palettes are in the buffer the board shows from (each frame the set is written there, the brightness kept) |
| `startViewer`, `viewerOn`, `viewerCharacter`, `characterNumber`, `animationStep`, `animationNames`, `backgrounds` | The model viewer the editor works in; it uses the viewer's `model_character`, `model_background` and `model_size` choices |
| `animationForLine` (optional) | The animation that best shows a line of the set, for the Line row to go to (0 standing, -1 none, -2 not known yet: asked again the next frame). Vampire Savior finds them by showing every frame of every animation on a spare machine, a few a frame, and counting the objects in each of the player's palettes |

The files, the same in every game: `palettes/<Character>.txt` (a `[SET]` section a set, up to a line a palette
of 16 `#RRGGBB` or `#RGB` colours, `-` for the game's own; a `#` with no hex digit after it starts a remark),
`palettes/original/` (the game's own, written by the command), `palettes/library/<Character>/<Name N>.txt` (one
set a file). The editor saves a character's file 45 frames after the last change and when it ends, with only
the sets and lines that are not the game's.
