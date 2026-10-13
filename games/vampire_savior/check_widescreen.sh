#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Tommy McLeroy
#
# Checks the widescreen feature (src/widescreen.cpp), with the ladder movie's controls (the fights do not go
# as the movie's did: the feature changes how the game plays). Where it reads the stage's plan for the
# picture's sides, the plan has to give what the game itself drew into the map: the run counts the tiles
# compared and those that differ. And no instruction may be left to the interpreter (the places the feature
# takes the game's comparisons over have translated code after them). That the game with the feature off is
# what it was is `make check`'s and the movies' business. Run from the repository's top.
GAME=vampire_savior
port=${B:-build/$GAME}/$GAME
movie=games/$GAME/movies/ladder.txt
status=0
out=$(VSAV_WIDE_CHECK=1 $port --rom "${ROM:-rom}" --headless --frames "${1:-39500}" --movie $movie --features free_play,no_damage,one_hit,widescreen --stats 2>&1) || status=1
compared=$(echo "$out" | grep "^widescreen:" | awk '{n += $4} END {print n + 0}')
differ=$(echo "$out" | grep "^widescreen:" | awk '{n += $(NF - 1)} END {print n + 0}')
echo "the stage's plan against the map: $compared tiles, $differ differ"
[ "$compared" -gt 0 ] && [ "$differ" -eq 0 ] || status=1
echo "$out" | grep "ran in the interpreter"
echo "$out" | grep -q "^0 instructions had no translation and ran in the interpreter" || status=1
[ $status -eq 0 ] && echo "ok: widescreen" || echo "FAILED"
exit $status
