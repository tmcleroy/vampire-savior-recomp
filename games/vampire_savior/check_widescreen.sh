#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Tommy McLeroy
#
# Checks the widescreen feature (src/widescreen.cpp). It must change the picture and nothing else: the ladder
# movie is run with it off and on, and the game's memory (work RAM, video and object memory, the video
# registers) written out at three frames has to be the same. And where it reads the stage's plan for the
# picture's sides, the plan has to give what the game itself drew into the map (the same run counts the
# tiles compared, and those that differ). Run from the repository's top.
GAME=vampire_savior
port=${B:-build/$GAME}/$GAME
movie=games/$GAME/movies/ladder.txt
features=free_play,no_damage,one_hit
frames=${1:-39500}
out=$(mktemp -d)
status=0
for at in 5000 20000 39000; do
    [ "$at" -lt "$frames" ] || continue
    $port --rom "${ROM:-rom}" --headless --frames $((at + 1)) --movie $movie --features $features --dump-ram $at:$out/off$at > /dev/null 2>&1 &
    $port --rom "${ROM:-rom}" --headless --frames $((at + 1)) --movie $movie --features $features,widescreen --dump-ram $at:$out/on$at > /dev/null 2>&1 &
done
wait
for at in 5000 20000 39000; do
    [ "$at" -lt "$frames" ] || continue
    for part in "" .video .objects .regs; do
        if ! cmp -s "$out/off$at$part" "$out/on$at$part"; then
            echo "frame $at: the game's memory$part differs with widescreen on"
            status=1
        fi
    done
done
[ $status -eq 0 ] && echo "ok: the game's memory is the same with widescreen on and off"
plan=$(VSAV_WIDE_CHECK=1 $port --rom "${ROM:-rom}" --headless --frames "$frames" --movie $movie --features $features,widescreen 2>&1 | grep "^widescreen:")
compared=$(echo "$plan" | awk '{n += $4} END {print n + 0}')
differ=$(echo "$plan" | awk '{n += $(NF - 1)} END {print n + 0}')
echo "the stage's plan against the map: $compared tiles, $differ differ"
[ "$compared" -gt 0 ] && [ "$differ" -eq 0 ] || status=1
rm -rf "$out"
[ $status -eq 0 ] && echo "ok: widescreen" || echo "FAILED"
exit $status
