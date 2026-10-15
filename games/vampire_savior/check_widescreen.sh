#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Tommy McLeroy
#
# Checks the widescreen feature (src/widescreen.cpp), with the ladder movie's controls (the fights do not go
# as the movie's did: the feature changes how the game plays). Where it reads the stage's plan for the
# picture's sides, the plan has to give what the game itself drew into the map: the run counts the tiles
# compared and those that differ. And no instruction may be left to the interpreter (the places the feature
# takes the game's comparisons over have translated code after them). That the game with the feature off is
# what it was is `make check`'s and the movies' business.
#
# With invisible walls (widescreen_walls) the game must be the board's: the ladder is played with widescreen and the
# walls on and with neither, and the game's memory, video and object memory and registers are compared every 4,000
# frames (the frame hash would not do: it takes the picture in, which is meant to differ). And on every stage a
# training session is walked to both ends with the walls on (movies/corners.txt): there the wide view keeps to the
# stage, and its 128 columns beyond the board's picture are looked at for a plane the game never painted, a run of
# 40 or more black pixels down a column (above the floor); more than kMost such columns at an end fails.
# Run from the repository's top.
GAME=vampire_savior
port=${B:-build/$GAME}/$GAME
movie=games/$GAME/movies/ladder.txt
status=0
dir=$(mktemp -d)
out=$(VAMPIRE_SAVIOR_HOME=$dir/home VSAV_WIDE_CHECK=1 $port --rom "${ROM:-rom}" --headless --frames "${1:-39500}" --movie $movie --features free_play,no_damage,one_hit,widescreen --stats 2>&1) || status=1
compared=$(echo "$out" | grep "^widescreen:" | awk '{n += $4} END {print n + 0}')
differ=$(echo "$out" | grep "^widescreen:" | awk '{n += $(NF - 1)} END {print n + 0}')
echo "the stage's plan against the map: $compared tiles, $differ differ"
[ "$compared" -gt 0 ] && [ "$differ" -eq 0 ] || status=1
echo "$out" | grep "ran in the interpreter"
echo "$out" | grep -q "^0 instructions had no translation and ran in the interpreter" || status=1

# Invisible walls: the stages' ends
for stage in 1 2 3 4 5 6 7 8 9 10 11 12; do
    VAMPIRE_SAVIOR_HOME=$dir/home_w$stage $port --rom "${ROM:-rom}" --headless --frames 3500 --features free_play,widescreen,widescreen_walls \
        --movie games/$GAME/movies/corners.txt --keys "5:ftraining_stage=$stage,10:ctraining_start" --shot-every 500:$dir/w${stage}_ > /dev/null 2>&1 &
    [ $((stage % 6)) -eq 0 ] && wait
done
wait
python3 - "$dir" <<'PY' || status=1
import struct, sys, zlib
kMost = 20  # columns
def png(path):
    d = open(path, 'rb').read(); i = 8; idat = b''
    while i < len(d):
        n = struct.unpack('>I', d[i:i + 4])[0]; t = d[i + 4:i + 8]; c = d[i + 8:i + 8 + n]
        if t == b'IHDR': w, h, _, kind = struct.unpack('>IIBB', c[:10])
        if t == b'IDAT': idat += c
        i += 12 + n
    raw = zlib.decompress(idat); bpp = 3 if kind == 2 else 4; stride = w * bpp; rows = []; prev = bytearray(stride)
    for y in range(h):
        f = raw[y * (stride + 1)]; line = bytearray(raw[y * (stride + 1) + 1:(y + 1) * (stride + 1)])
        for x in range(stride):
            a = line[x - bpp] if x >= bpp else 0; b = prev[x]; c = prev[x - bpp] if x >= bpp else 0
            if f == 1: line[x] = (line[x] + a) & 255
            elif f == 2: line[x] = (line[x] + b) & 255
            elif f == 3: line[x] = (line[x] + (a + b) // 2) & 255
            elif f == 4:
                p = a + b - c; pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
                line[x] = (line[x] + (a if pa <= pb and pa <= pc else b if pb <= pc else c)) & 255
        rows.append(line); prev = line
    return w, bpp, rows
bad = 0
for stage in range(1, 13):
    shares = []
    # (at the left end the wide picture's 128 columns beyond the board's are on the right, and the other way round)
    for frame, x0 in ((2500, 384), (3500, 0)):
        w, bpp, rows = png('%s/w%d_%06d.png' % (sys.argv[1], stage, frame))
        if w != 512:
            print('stage %2d: the picture is %d wide at frame %d, not 512' % (stage, w, frame)); bad += 1; continue
        columns = 0
        for x in range(x0, x0 + 128):
            run = best = 0
            for y in range(24, 180):
                run = run + 1 if sum(rows[y][x * bpp:x * bpp + 3]) < 12 else 0
                best = max(best, run)
            columns += best >= 40
        shares.append(columns)
    worst = max(shares) if shares else 0
    print('stage %2d, invisible walls: unpainted columns at the left end %2d, at the right end %2d%s' % (stage, *(shares + [0, 0])[:2], '  TOO MANY' if worst > kMost else ''))
    bad += worst > kMost
sys.exit(1 if bad else 0)
PY

# Invisible walls: the game is the board's
differ=0
for frame in 4000 8000 12000 16000 20000 24000 28000 32000 36000; do
    for run in off walls; do
        features=free_play,no_damage,one_hit
        [ $run = walls ] && features=$features,widescreen,widescreen_walls
        VAMPIRE_SAVIOR_HOME=$dir/home_$run$frame $port --rom "${ROM:-rom}" --headless --frames $((frame + 2)) --movie $movie \
            --features $features --dump-ram $frame:$dir/$run$frame.bin > /dev/null 2>&1 &
    done
    wait
    for part in "" .video .objects .regs; do
        cmp -s $dir/off$frame.bin$part $dir/walls$frame.bin$part || { echo "invisible walls: frame $frame$part differs from the board's"; differ=1; }
    done
done
[ $differ -eq 0 ] && echo "invisible walls: the game's memory and registers are the board's at 9 places of the ladder" || status=1
rm -rf "$dir"
[ $status -eq 0 ] && echo "ok: widescreen" || echo "FAILED"
exit $status
