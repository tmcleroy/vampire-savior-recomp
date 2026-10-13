#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Tommy McLeroy. Part of cps2recomp.
# Checks the sound driver (sound/driver.cpp: the game's sound program as ordinary code) against the program
# itself on the Z80:
#   1. both run, the driver doing each piece of work as the Z80 starts on it, and every write to the QSound
#      chip compared (--sound check): over made-up play, and over the sound test (every song in the sound
#      ROM and made-up ones of the kinds it lacks, placed every way, with the other commands in between;
#      every fourth seed in mono);
#   2. the game's side of a run (its processor, memory and picture, frame by frame) the same with the driver
#      as with the Z80.
#
#   check_sound.sh [RUNS [FRAMES]]     from the repository's top; a few minutes
GAME=${GAME:-vampire_savior}
ROM=${ROM:-rom}
runs=${1:-8}; frames=${2:-150000}
work=work/$GAME/check
port=${B:-build/$GAME}/$GAME
mkdir -p "$work"
if ! $port --rom "$ROM" --headless --frames 1 --sound check > /dev/null 2>&1; then
    echo "skipped: there is no driver for this game's sound program (it runs on the Z80)"
    exit 0
fi
status=0
every="30 7 90 3 12 240 1 45"
for seed in $(seq 1 "$runs"); do
    n=$(echo $every | cut -d' ' -f$(( (seed - 1) % 8 + 1 )))
    $port --rom "$ROM" --headless --frames "$frames" --sound-test "$seed:$n" --sound check --stats > "$work/sound_$seed.txt" 2>&1 &
done
$port --rom "$ROM" --headless --frames 24000 --auto 7:400 --sound check --stats > "$work/sound_play.txt" 2>&1 &
$port --rom "$ROM" --headless --frames 24000 --auto 7:400 --sound z80 --hash-game --hash-log "$work/game_z80.txt" > /dev/null 2>&1 &
$port --rom "$ROM" --headless --frames 24000 --auto 7:400 --sound driver --hash-game --hash-log "$work/game_driver.txt" > /dev/null 2>&1 &
wait
writes=0
for f in "$work"/sound_play.txt $(seq 1 "$runs" | sed "s|.*|$work/sound_&.txt|"); do
    n=$(grep "writes to the sound chip the same" "$f" | cut -d' ' -f1)
    if grep -q "differs" "$f" || [ -z "$n" ]; then
        echo "FAILED: $(basename "$f" .txt)"; grep -A3 "differs" "$f" | cut -c1-300
        status=1
    else
        writes=$((writes + n))
    fi
done
[ $status -eq 0 ] && echo "ok: the driver writes the sound chip what the Z80 does ($writes writes)"
if cmp -s "$work/game_z80.txt" "$work/game_driver.txt"; then
    echo "ok: the game runs the same with the driver as with the Z80 (24000 frames)"
else
    echo "FAILED: the game with the driver differs from the game with the Z80 at frame $(cmp "$work/game_z80.txt" "$work/game_driver.txt" | sed 's/.*line //')"
    status=1
fi
exit $status
