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
inPicture="DrawnIfInWide,DrawnIfInPicture,ThrownGone,PieceByCamera,KeepIn,KeepBothIn,AllPieces"
scripts="PiecePlain,PieceFlickering,PieceDancers,PieceLady,PieceThrower"
for skip in "" "AllHitTests,CameraAcross,CameraUp,$inPicture" "$inPicture,Drawn,$scripts" "AllHitTests,PlayersAgainstPlayers,ThrownAgainstPlayers,GuardRange" \
    "AllHitTests,PlayersAgainstPlayers,ThrownAgainstPlayers,HitTest,AttackAgainst"; do
    run versus_normals 3000 free_play "$skip"
    run versus_attacks 2600 free_play "$skip"
    run versus 6000 "" "$skip"
    run ladder 12000 free_play "$skip"
done
# The stage's code (camera.cpp, planes.cpp, rows.cpp, picture.cpp, edges.cpp, pieces.cpp): each stage has its own choice of those routines, so every stage
# is visited: a training session on it, the players moved by made-up controls (no movie with them: a movie's
# controls would be the only ones, and the camera has to go from end to end). The outer rewrites, then
# with them left out the ones they use.
stage() { # stage (1-12), rewrites to leave out
    out=$(CPS2RECOMP_SKIP_REWRITES="$2" $port --rom "${ROM:-rom}" --headless --frames 6000 --auto 7:1500 \
        --keys "5:ftraining_stage=$1,10:ctraining_start" --check-rewrites 2>&1)
    echo "== stage $1${2:+ (without the outer ones)}"
    # (training's own changes to the round's rules differ from the game's, as they are meant to: only the
    # stage's rewrites are looked at)
    out=$(echo "$out" | grep -E "^rewrite|did not" | grep -E "Camera|Players(Across|Up)|Back8|Far32|Rows|Drawn|Piece|Script[0-9]|Plane(Of|AndRow)|PlaceOf|Animat|GiveBack|ThrownGone|KeepIn|KeepBothIn|did not" | grep -v " 0 calls")
    echo "$out"
    echo "$out" | grep -q "CameraPlaced" || status=1
    echo "$out" | grep -E "differs|did not|calls, [1-9][0-9]* differed" >/dev/null && status=1
}
outer=$(grep -oE '"(Back8|Far32)(Across|Up)[A-Za-z0-9]*"' games/$GAME/src/planes.cpp | tr -d '"' | paste -sd, -)
for n in 1 2 3 4 5 6 7 8 9 10 11 12; do
    stage $n ""
    stage $n "CameraAcross,CameraUp,$outer,$inPicture"
    stage $n "CameraAcross,CameraUp,$outer,$inPicture,$scripts,Drawn,SetStageAnimation"
done
[ $status -eq 0 ] && echo "ok: every rewrite does what the game's code does" || echo "FAILED"
exit $status
