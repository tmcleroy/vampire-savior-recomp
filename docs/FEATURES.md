# Features

Every change to how the game plays, looks or is controlled is a switch, set in the launcher's
Features page or in the in-game menu (Esc, `F1`, a gamepad's left stick click) and kept in
`settings.ini`. With every one off the game runs exactly as the arcade board does. The launcher's
presets: **Pure** (all off), **Enhanced** (the default: free play on), **Custom**.

This list is what `vampire_savior --list-features` prints. How features are written is in
[cps2recomp/docs/FEATURES.md](../cps2recomp/docs/FEATURES.md).

## Alternative modes

"Alternative modes", the first row of the in-game menu, has ways of running the game that the
board does not have. Each mode has a page there with a Start row and its choices. Each mode ends when another is started or with the menu's Reset.

- **Training mode**: straight to the character choice with two players and a fight with no clock
  that nobody loses (the README says more; the Training and Dummy features below are its aids).
- **Attract mode**: the game's own show on a loop with no coin in, even with free play on: the
  notices, the introduction, the title, two fights and the rankings. Start or a coin ends it.
- **Stage viewer**: the stage chosen with nobody on it and no panel. The stage's own pieces move
  as in a fight. Left and right scroll it, or it scrolls by itself from end to end and back.
- **Model viewer**: one character by itself, on a stage or on plain white or black, with no panel. It
  stands as in a fight, and player 1's controls move it; or it goes through one of its animations over
  and over (they are named by the game's three lists of them and a number: A34. Left and right go to the one
  before and the one after, and the name is shown for two seconds when it changes). Drawn from a quarter of its size to 25 times, at full
  speed or in slow motion, an animation starting again at once or after up to ten seconds. "Reset
  options" puts the page's choices back.
- **Colour editor**: the model viewer's character with a panel at the right, worked with player 1's
  controls (up and down choose a row, left and right change it): the character, background and size,
  what it does (its own Animation choice: standing in the game's idle unless changed, or any of its
  animations, as in the viewer; the viewer's choice is left as it was),
  the set of colours (LP to HK, PP, KK, AUTO1, AUTO2), its line (the character's own colours or its
  effects'; choosing one shows the animation that uses it most, for the characters whose four lines
  are always on show, found by running each animation on a copy of the machine in the first seconds), the colour (one of 15; it blinks on the character when chosen), and its red, green and
  blue (the board's 16 levels each). Changes are seen at once. LP copies the colour, MP pastes it, HP
  puts back the game's, the panel's Random colours row, pressed twice (LK, left or right), gives every colour of the set a random one (a row of
  the page too), and HK puts back the colours from before it, until the next change; the page has rows to put back the game's colours for the whole set or the
  whole character. What is changed is saved to the character's file in the `palettes` folder (only
  the sets and lines that are not the game's) a moment after the last change and when the editor
  ends. "Save set" keeps the set in the character's library (`palettes/library/<Character>/`,
  named `Lilith 1`, `Lilith 2`... one file a set, its four lines of colours), and "Load" puts a saved
  one into the set being edited: left and right try each on the character, LP loads it, MK twice
  deletes the file, and leaving the row without loading puts the set's colours back. Files put in
  that folder by hand are listed too, and Custom colours is switched on, so that it is used in play. The hidden characters are not
  in it (the viewer has not got them).

- **Survival**: choose a character, then one opponent after another, each beaten by one
  knock-down. Your health is what the last fight left you, and a little back. Over the first time
  you go down, with the count of opponents beaten.
- **Endless battle**: two fighters played by the game, with no bars and no clock; the one knocked
  down is replaced. Pressing anything on a player's controls takes that fighter over, and ten
  seconds without a press gives it back.
- **Ring out**: choose a character. Nobody loses health: the ends of a stage are open, and a
  fighter knocked over one (while reeling from a hit, guarding one or thrown) falls. Put your
  opponent out and you jump down to the next stage and the next opponent; over when you fall, with
  the count of ring outs.

| Feature | Name in `settings.ini` | What it does |
| --- | --- | --- |
| Survival: Best | `survival_best` | The most opponents beaten in one survival, kept here. Choosing a preset in the launcher puts it back to 0. |
| Ring out: Best | `ringout_best` | The most ring outs in one go, kept the same way. |
| Endless battle: Fighter 1, Fighter 2 | `endless_one`, `endless_two` | Who is on each side: Random (someone new each time), or one of the fifteen characters. |
| Endless battle: Stage | `endless_stage` | Shuffle: a stage that fades into another, at random, every so often. Scroll: the stages go by one after another at a steady pace and the fighters are carried along with the floor, the picture's left edge pushing them on. Or one of the twelve stages. |
| Endless battle: Shuffle every | `endless_shuffle` | How long the fight stays on a stage before Shuffle fades it into another: 1, 2, 3, 5 or 10 minutes. |
| Endless battle: Scroll speed | `endless_scroll` | How fast Scroll's stages go by: Slow, Medium or Fast. |
| Attract mode: Shows | `attract_show` | What attract mode plays. Choices: Everything, The intro only (the introduction to the title, over and over), The fights only (one of the game's eight demonstration fights after another). |
| Stage viewer: Stage | `viewer_stage` | The stage the viewer shows: any of the twelve, or Infinite: every stage in turn, each scrolled from end to end and then out of the picture as the next comes in, its tune fading into the next one's. Changing it while viewing goes to the new one. |
| Stage viewer: Scrolling | `viewer_scroll` | How the viewer moves across the stage. Choices: With the stick, Slow (a pixel every four frames), Medium, Fast (a pixel a frame). |
| Stage viewer: Music | `viewer_music` | The tune that plays in the viewer. Choices: The stage's own, any stage's, Character select. |
| Model viewer: Character | `model_character` | Who is shown. Changing it while viewing goes to the new one. |
| Model viewer: Background | `model_background` | Any of the twelve stages, White or Black. |
| Model viewer: Stage in front | `model_foreground` | In front (what the stage has in front of the fighters stays in front of the character, as in a fight: only at the character's own size, and not in the colour editor) or Behind. |
| Model viewer: Size | `model_size` | Choices: 1x, 1.5x, 2x, 3x, 4x, 6x, 8x, 12x, 16x, 25x, 0.25x, 0.5x, 0.75x. |
| Model viewer: Animation | `model_animation` | Standing (you control it), or one of the character's animations, named by the game's list and a number: A0 to A255, B0 to B255, C0 to C255 (a character has fewer; the ones it has not got are passed over). |
| Model viewer: Speed | `model_speed` | Full, 1/2, 1/4 or 1/8: slow motion for the viewer (the menu's own speed slows it further). |
| Model viewer: Repeat after | `model_repeat` | How long an animation's last frame is held before it starts again: at once, or 1 to 10 seconds. |
| Model viewer: Play all | `model_all` | Every animation the character has, one after another: each shown once (its last frame held for "Repeat after"), then the next, round from the last to the first. Left and right still step through them. |

## Game

| Feature | Name in `settings.ini` | What it does |
| --- | --- | --- |
| Free play | `free_play` | Start works without a coin. |
| Hidden characters | `hidden_characters` | Choosing Gallon gives Dark Gallon and choosing Bishamon gives Oboro Bishamon (the last boss, whom no code chooses on this set); a punch chooses their first colours, a kick their second. The "?" box gives Shadow, who becomes each opponent beaten in a one-player game. For player 1, player 2 or both; the alternative modes' fighters too. Off in every preset. Choices: Off, Player 1, Player 2, Both. |
| Straight to the title | `skip_intro` | When the game starts, and when it is reset, the board's memory test, the version and the warning, the notices and the introduction are run unseen, and the picture comes back with the title (with a coin in, or free play, the title that asks for Start) a second or so later. The alternative modes start as they did. |

## Board

The board's own operator settings, as its test menu has them (設定: System and Game). Each is "As the
board is set" unless chosen, and a choice is taken when the game starts or is reset; the board's
settings memory is not changed. The in-game menu's "Test menu" row (or F2) opens the game's own
test menu instead.

| Feature | Name in `settings.ini` | What it does |
| --- | --- | --- |
| Difficulty | `board_difficulty` | How well the computer fights, 1 (easiest) to 8 (hardest); a new board has 2. |
| Attack power | `board_attack` | How much damage a hit does, 1 to 4 (2). |
| Timer speed | `board_timer` | How fast the round's clock runs, 1 to 4 (2). |
| Game speed | `board_speed` | Normal, Turbo 1 to 3, or Free select 1 to 3 (the players choose; Free select 2). |
| Damage gauges | `board_gauges` | How many health bars each fighter has, 1 to 4 (2). |
| Event | `board_event` | Normal, or one match only (and no continue). |
| Automatic | `board_automatic` | Whether players may choose the automatic guard: None, or Free select. |
| Continue | `board_continue` | Whether a game over can be continued. |
| Demo sound | `board_demo_sound` | Whether the attract mode plays sound. |
| Self-censorship | `board_censorship` | The board's own restraint setting (自主規制). |

## Display

| Feature | Name in `settings.ini` | What it does |
| --- | --- | --- |
| Widescreen | `widescreen` | Its switch in the launcher is on the first page, under "Full screen" (not on the Features page), and that switch decides at every start; the in-game menu's Display page has it too. Fights are shown and played in 16:9 (512 columns where the board shows 384), with more of the stage on each side. Off unless switched on, in every preset, because it changes how the game plays: the players can go to the wide picture's edges (464 apart at most, where the board allows 336), and the camera stops 64 short of where it did, so that the picture ends where the stage does. The bars stay in the middle. Everything that is not a fight keeps the board's 4:3. Known: some pieces of a stage come in late at the wide picture's edge, and at a stage's very ends a few columns of the planes behind are not painted. |
| Invisible walls | `widescreen_walls` | On the Display page, in the launcher's Features and the in-game menu. With widescreen on, fights are still shown in 16:9 but played exactly as on the board: the players are stopped where the board's picture ends, by walls that are not seen, the camera goes as far as the board's, and what the game keeps or draws is what it does on the board (the game's memory is the board's, frame for frame). Near a stage's end the view stays on the stage, so the play area is then to one side of the picture. Nothing without widescreen. Known: a piece of a stage that the board draws only once it is in its own picture comes in at the play area's edge. |
| Custom colours | `custom_colours` | On in the Enhanced preset. Characters in colours of your own: a text file a character in the folder `palettes` beside `settings.ini` (Windows `%APPDATA%\Vampire Savior\palettes`, macOS `~/Library/Application Support/Vampire Savior/palettes/`, Linux `~/.local/share/Vampire Savior/palettes/`), named after the character as this set of the game calls them or as the game abroad does (`Morrigan.txt`, `Gallon.txt` or `Talbain.txt`, `Dark Gallon.txt`). A file has a section for each set it changes, named by how the set is chosen (`[LP]` to `[HK]`, `[PP]` two punches at once, `[KK]` two kicks, `[AUTO1]` and `[AUTO2]` for auto guard), and under it up to four lines of 16 colours (`#RRGGBB` or `#RGB`; `-` keeps the game's). The first line is the character's own colours, the others its effects; the 16th of a line is never seen. The board has 16 levels of red, green and blue, so a colour is rounded to the nearest. Colours made with PalMod (the community's palette editor) work too: edit a copy of your set's `vm3j.10b` with it and put the copy in `palettes`; the characters' colours it changed are used (their sets and their effects': poisoned, a shock, burned and the like), anything else it changed is left as the game's, and the text files go over it. With no files, or switched off, the game's own colours. New colours are taken when a player is next coloured (at the next round). The colour editor's menu page has "Write the game's colours", which puts every character's own colours in `palettes/original` to start from; "Load colour files again" reads files changed while the game runs. |
| Scaling | `display_scaling` | How the picture is enlarged to the window. Sharp (as always): as large as fits, each pixel a crisp block, only the edges between blocks blended. Whole pixels: every pixel the same whole number of the screen's pixels tall, with a black border round the picture (on the Steam Deck's 1280 x 800, 3 times: 672 lines). Smooth: as large as fits, blended all over. |
| Picture shape | `display_shape` | 4:3 (as always): as the arcade's monitor showed the board's 384 x 224, its pixels a little narrower than tall. Square pixels: every pixel as wide as tall (12:7; with Whole pixels, every pixel a perfect square block). Fill: the whole window or screen, stretched. Widescreen's extra columns are counted in each. |
| Window size | `window_size` | How large the window opens: 3x (as always) the board's 224 lines, or 2x, 4x, 5x, 6x. Changing it resizes the window at once. Full screen is on the launcher's first page and F11. |

## Cheats

| Feature | Name in `settings.ini` | What it does |
| --- | --- | --- |
| No damage | `no_damage` | Player 1's health does not go down. |
| No damage, player 2 | `no_damage_2` | Player 2's health does not go down (the opponent's, in a one-player game). |
| One-hit knockouts | `one_hit` | Player 2 (the opponent, in a one-player game) goes down with the first hit. |
| Full gauge | `full_gauge` | Player 1 always has special gauges to spend. |
| Full gauge, player 2 | `full_gauge_2` | Player 2 always has special gauges to spend. |
| Infinite time | `infinite_time` | The round's clock stands still. |

## Training mode

| Feature | Name in `settings.ini` | What it does |
| --- | --- | --- |
| Start over at | `restart_at` | Where "Start over" puts the players: the middle or a corner, player 1 on the left or (swapped) on the right. Choices: Middle, Left corner, Right corner, Middle, swapped, Left corner, swapped, Right corner, swapped. |
| Stage | `training_stage` | Where a training session's fight is: the game's own choice (at random, from those that go with player 2's character) or a stage named here. Takes effect when the characters are next chosen. Choices: The game's choice, Feast of the Damned, Concrete Cave, Tower of Arrogance, Red Thirst, Deserted Chateau, Abaraya, Vanity Paradise, War Agony, Forever Torment, Green Scream, Iron Horse, Iron Terror, Fetus of God. |
| Combo readout | `damage_readout` | Under the bar of the player hit: what the hit took off; for a combo (hits landing before the player is free again) the hits and the damage; how much of it is lost for good and how much comes back; the round's best. |
| Input display | `input_display` | Shows what each player presses and for how many frames: the stick as a keypad number (5 is the middle, 6 right, 2 down), then the buttons. |
| Hit boxes | `hit_boxes` | Draws the boxes the game tests: where a player can be hit (blue), attacks (red; orange for things thrown), and the boxes that keep the players apart (green). |
| Frame data | `frame_data` | After a move: the step it can first hit on (startup), how many it can hit for (active) and how many follow before the player can act (recovery). After a hit or a guarded one: who can act first, by how many steps (+: the attacker). |
| Frame meter | `frame_meter` | A row of cells for each player, one for every step of the game: green before a move can hit, red while it can, blue after; yellow reeling from a hit, light blue from a guarded one; grey in the air. |
| Health refills | `health_refill` | A player's health comes back when they have not been hit for a moment. |

## Dummy

| Feature | Name in `settings.ini` | What it does |
| --- | --- | --- |
| Stands | `dummy_stand` | Player 2 stands still, whatever its controls do. |
| Crouches | `dummy_crouch` | Player 2 crouches. |
| Jumps | `dummy_jump` | Player 2 jumps on the spot, again and again. |
| Record | `dummy_record` | Player 1's controls work player 2, and what they do is recorded in the slot chosen (20 seconds at most). Switch to Replay when done. |
| Replay | `dummy_replay` | Player 2 does what was recorded in the slot chosen, over and over. |
| Replay any slot | `dummy_replay_any` | Player 2 does what was recorded, over and over, each time from a slot picked at random. |
| Recording slot | `dummy_slot` | Which of three recordings Record makes and Replay plays. Choices: 1, 2, 3. |
| Guards | `dummy_guard` | Player 2 guards player 1's attacks, standing or crouching as each needs: always; only for a moment after being hit (to see whether a combo really is one); or one attack in two, at random. Choices: Off, Always, After a hit, At random. |
| While guarding | `dummy_guard_action` | What player 2 does while guarding a hit: presses buttons to push the attacker away, or does a guard cancel (towards, down, down and towards with a punch or a kick: most characters have one or the other). Choices: Nothing, Pushes away, Guard cancel, punch, Guard cancel, kick. |
| Answers with the recording | `dummy_reversal` | Player 2 plays the recording once as soon as it can act after guarding, after being hit (or knocked down, or thrown), or both. Choices: Off, After guarding, After being hit, After either. |
| Softens throws | `dummy_tech` | Player 2 does a throw's own command while being thrown (a "tech hit"). |
| Gets up | `dummy_getup` | Where player 2 gets up after being knocked down: where it fell, or rolled towards player 1 or away. Choices: Where it fell, Towards, Away. |

## Always there

Not features, because they change nothing in the game: the in-game menu (with its Test menu row, the
board's test switch), save states (nine slots),
stopping the game and going on a frame at a time, slow motion, a coin button, Reset, and choosing
keys and gamepad buttons in the launcher. The menu's "Start over" row and the rows that start a
mode are commands, not features.
