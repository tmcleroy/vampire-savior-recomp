#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Tommy McLeroy
#
# Checks the game's rewritten routines (src/) against the code they replace: every call of each, over the
# game's movies, run both ways and compared (cps2recomp/docs/READABLE.md). Inner rewrites are only called by
# outer ones, so each movie is run again with the outer ones left out. Run from the repository's top.
GAME=vampire_savior
port=${B:-build/$GAME}/$GAME
movies=games/$GAME/movies
status=0
run() { # movie, frames, features, rewrites to leave out
    out=$(CPS2RECOMP_SKIP_REWRITES="$4" $port --rom "${ROM:-rom}" --headless --frames "$2" --movie "$movies/$1.txt" --features "$3" --check-rewrites 2>&1) || status=1
    echo "== $1${4:+ (without $4)}"
    echo "$out" | grep -E "^rewrite|did not" | grep -v " 0 calls"
}
for skip in "" "AllHitTests,CameraAcross,CameraUp" "AllHitTests,PlayersAgainstPlayers,ThrownAgainstPlayers,GuardRange" \
    "AllHitTests,PlayersAgainstPlayers,ThrownAgainstPlayers,HitTest,AttackAgainst"; do
    run versus_normals 3000 free_play "$skip"
    run versus_attacks 2600 free_play "$skip"
    run versus 6000 "" "$skip"
    run ladder 12000 free_play "$skip"
done
[ $status -eq 0 ] && echo "ok: every rewrite does what the game's code does" || echo "FAILED"
exit $status
