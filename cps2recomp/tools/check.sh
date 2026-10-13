#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Tommy McLeroy. Part of cps2recomp.
#
# Checks of the game's translated code, by the picture and state it makes frame by frame:
#   1. translated code against the interpreter run the same way (--cpu blocks);
#   2. two runs of the translated code alike.
#
#   check.sh FRAMES [MOVIE] [extra port arguments]
#
# Without a movie the input is made up (--auto 7:400). Run from the repository's top. The sound program runs
# on the Z80 here whether or not there is a driver for it (tools/check_sound.sh checks that).
GAME=${GAME:-vampire_savior}
ROM=${ROM:-rom}
frames=${1:-3000}; movie=$2
[ $# -ge 2 ] && shift 2 || shift $#
extra="$*"
work=work/$GAME/check
port=${B:-build/$GAME}/$GAME
mkdir -p "$work"
if [ -n "$movie" ]; then input="--movie $movie"; else input="--auto 7:400"; fi
run() { # name, cpu mode
    $port --rom "$ROM" --headless --frames "$frames" $input --cpu "$2" --sound z80 --hash-log "$work/$1.txt" $extra 2>"$work/$1.err" || {
        echo "FAILED: the $1 run stopped"; cat "$work/$1.err"; exit 1; }
}
run blocks blocks & run native native & run again native & wait
[ "$(wc -l < "$work/native.txt")" -eq "$frames" ] || { echo "FAILED: the run stopped early"; exit 1; }
status=0
compare() {
    if cmp -s "$work/$1.txt" "$work/$2.txt"; then
        echo "ok: $3 ($frames frames)"
    else
        echo "FAILED: $3: first difference at frame $(cmp "$work/$1.txt" "$work/$2.txt" | sed 's/.*line //')"
        status=1
    fi
}
compare blocks native "translated code does what the interpreter does"
compare native again "two runs alike"
exit $status
