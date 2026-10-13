#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Tommy McLeroy. Part of cps2recomp.
#
# Plays the game with made-up input, several runs side by side, and adds every place the code reached by a
# computed jump to the entries file (the next 'make' translates from those too).
#
#   explore.sh FIRST_SEED RUNS FRAMES [extra port arguments]
#
# Run from the repository's top; GAME and ROM as for make. AUTO: what follows the seed in --auto
# (default ":400": from frame 400, both players; ":400:1": one player, so that it goes up the arcade ladder).
# Cheats help it get far: --features free_play,no_damage,one_hit
GAME=${GAME:-vampire_savior}
ROM=${ROM:-rom}
first=$1; runs=$2; frames=$3; shift 3
work=work/$GAME
port=${B:-build/$GAME}/$GAME
mkdir -p "$work/explore"
seq "$first" $((first + runs - 1)) | xargs -P 6 -I{} sh -c \
    "$port --rom '$ROM' --headless --frames $frames --auto {}${AUTO:-:400} --record-entries $work/explore/entries_{}.txt $* 2>/dev/null"
# (one file per run, so that runs do not write over one another; merged here: the 68000's, then the sound
# driver's)
merge() { # the entries file, the runs' files
    main=$1; shift
    before=$(grep -vc '^#' "$main" 2>/dev/null || echo 0)
    cat "$main" "$@" 2>/dev/null | grep -v '^#' | sort -u > "$main.new"
    { echo "# Places the game's code was seen to reach in ways the translator cannot follow"; cat "$main.new"; } > "$main"
    rm -f "$main.new"
    echo "$(basename "$main"): $before -> $(grep -vc '^#' "$main")"
}
merge "$work/entries.txt.z80" "$work"/explore/entries_*.txt.z80
rm -f "$work"/explore/entries_*.txt.z80
merge "$work/entries.txt" "$work"/explore/entries_*.txt
rm -f "$work"/explore/entries_*.txt
