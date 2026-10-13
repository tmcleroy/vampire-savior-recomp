#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Tommy McLeroy. Part of cps2recomp.
#
# A contact sheet of screenshots, to look over a run at a glance: sheet.sh OUT.png COLUMNS shot1.png shot2.png ...
# (the shots in order, left to right, top to bottom; needs ffmpeg)
out=$1; cols=$2; shift 2
n=$#
rows=$(( (n + cols - 1) / cols ))
dir=$(mktemp -d)
i=0
for f in "$@"; do cp "$f" "$dir/$(printf '%05d' $i).png"; i=$((i + 1)); done
ffmpeg -loglevel error -y -framerate 1 -i "$dir/%05d.png" -vf "scale=192:112,tile=${cols}x${rows}" -frames:v 1 "$out"
rm -rf "$dir"
