#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Tommy McLeroy. Part of cps2recomp.
"""Prints a --trace file (so many bytes of work RAM after every frame) where it changes.

  tracechanges.py FILE BYTES [FIRST_FRAME [IGNORE,IGNORE...]]

BYTES: how many bytes a frame (HIGH - LOW + 1 of --trace). IGNORE: offsets of bytes whose changes alone do
not make a line (counters).
"""
import sys


def main():
    if len(sys.argv) < 3:
        sys.exit(__doc__)
    data = open(sys.argv[1], "rb").read()
    size = int(sys.argv[2], 0)
    first = int(sys.argv[3]) if len(sys.argv) > 3 else 1
    ignore = [int(x, 0) for x in sys.argv[4].split(",")] if len(sys.argv) > 4 else []
    before = None
    for n in range(len(data) // size):
        row = data[n * size:(n + 1) * size]
        key = bytes(0 if i in ignore else b for i, b in enumerate(row))
        if key != before:
            print("%6d  %s" % (first + n, " ".join(row[i:i + 2].hex() for i in range(0, size, 2))))
            before = key


if __name__ == "__main__":
    main()
