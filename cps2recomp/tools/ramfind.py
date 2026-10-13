#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Tommy McLeroy. Part of cps2recomp.
"""Finds where the game keeps a value, from work RAM dumps (--dump-ram) taken when the value was known.

  ramfind.py [-w] DUMP=VALUE DUMP=VALUE ...

Prints the addresses whose byte (or, with -w, big-endian word) had each VALUE (decimal or 0x hex; BCD
values as hex) in its DUMP. A value may be 'a..b' for a range, or '*' for anything.
"""
import sys


def main():
    args = sys.argv[1:]
    word = "-w" in args
    args = [a for a in args if a != "-w"]
    if not args:
        sys.exit(__doc__)
    dumps = []
    for a in args:
        path, value = a.rsplit("=", 1)
        if value == "*":
            lo, hi = 0, 0xFFFF
        elif ".." in value:
            lo, hi = (int(v, 0) for v in value.split(".."))
        else:
            lo = hi = int(value, 0)
        dumps.append((open(path, "rb").read(), lo, hi))
    step = 2 if word else 1
    for at in range(0, 0x10000, step):
        values = []
        for data, lo, hi in dumps:
            v = (data[at] << 8) | data[at + 1] if word else data[at]
            if not lo <= v <= hi:
                break
            values.append(v)
        else:
            print("%06X: %s" % (0xFF0000 + at, " ".join(("%04X" if word else "%02X") % v for v in values)))


if __name__ == "__main__":
    main()
