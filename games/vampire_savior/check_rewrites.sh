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
. cps2recomp/tools/at-once.sh # (the runs at once, a few at a time; what each wrote printed in order)
check() { # movie, frames, features, rewrites to leave out
    out=$(CPS2RECOMP_SKIP_REWRITES="$4" $port --rom "${ROM:-rom}" --headless --frames "$2" --movie "$movies/$1.txt" --features "$3" --check-rewrites 2>&1)
    failed=$?
    echo "== $1${4:+ (without $4)}"
    echo "$out" | grep -E "^rewrite|did not" | grep -v " 0 calls"
    return $failed
}
run() {
    at_once check "$@"
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
check_stage() { # stage (1-12), rewrites to leave out, more of the port's options
    out=$(CPS2RECOMP_SKIP_REWRITES="$2" $port --rom "${ROM:-rom}" --headless --frames 6000 --auto 7:1500 \
        --keys "5:ftraining_stage=$1,10:ctraining_start" $3 --check-rewrites 2>&1)
    echo "== stage $1${2:+ (without the outer ones)}${3:+ ($3)}"
    # (training's own changes to the round's rules differ from the game's, as they are meant to: only the
    # stage's rewrites are looked at)
    out=$(echo "$out" | grep -E "^rewrite|did not" | grep -E "Camera|Players(Across|Up)|Back8|Far32|Rows|Drawn|Piece|Script[0-9]|Plane(Of|AndRow)|PlaceOf|Animat|GiveBack|ThrownGone|KeepIn|KeepBothIn|did not" | grep -v " 0 calls")
    echo "$out"
    echo "$out" | grep -q "CameraPlaced" || return 1
    echo "$out" | grep -E "differs|did not|calls, [1-9][0-9]* differed" >/dev/null && return 1
    return 0
}
stage() {
    at_once check_stage "$@"
}
outer=$(grep -oE '"(Back8|Far32)(Across|Up)[A-Za-z0-9]*"' games/$GAME/src/planes.cpp | tr -d '"' | paste -sd, -)
for n in 1 2 3 4 5 6 7 8 9 10 11 12; do
    stage $n ""
    stage $n "CameraAcross,CameraUp,$outer,$inPicture"
    stage $n "CameraAcross,CameraUp,$outer,$inPicture,$scripts,Drawn,SetStageAnimation"
    # (many pieces' scripts do something else once a round is decided, which a training session's never
    # is: the game's flag for it held on, for the game's code and the rewrites alike)
    stage $n "CameraAcross,CameraUp,$outer,$inPicture" "--poke FF8107=01"
done
# Moves that go by the picture's edges (moves.cpp): an endless battle on one stage taken over by player 1, as a
# character that has them (Felicia 8, Bishamon 9, Q-Bee 12: endless_one's choices), with a movie of its inputs; nobody
# loses health, or the one knocked down would be someone else
check_moves() { # movie, frames, character, rewrites that are to be called
    out=$($port --rom "${ROM:-rom}" --headless --frames "$2" --movie "$movies/$1.txt" --keys 10:cendless_start \
        --features "endless_one=$3,endless_two=1,endless_stage=2,full_gauge,no_damage,no_damage_2" --check-rewrites 2>&1)
    echo "== $1 (character $3)"
    out=$(echo "$out" | grep -E "^rewrite ($4) ")
    echo "$out"
    echo "$out" | grep -E " 0 calls|calls, [1-9][0-9]* differed" >/dev/null && return 1
    return 0
}
at_once check_moves wall_cling 3300 8 "OnTheWall"
at_once check_moves moves 27000 12 "QBeeToTheWall|QBeeFlies|QBeeCarries"
at_once check_moves moves 27000 9 "BishamonCaught"
at_once_wait || status=1
[ $status -eq 0 ] && echo "ok: every rewrite does what the game's code does" || echo "FAILED"
exit $status
