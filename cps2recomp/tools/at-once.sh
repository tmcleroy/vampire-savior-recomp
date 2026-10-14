# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Tommy McLeroy. Part of cps2recomp.
#
# For a game's check scripts: run checks at once, a few at a time (JOBS, 8 unless said), and print what each wrote
# in the order they were started. Sourced, not run:
#
#   . cps2recomp/tools/at-once.sh
#   at_once some_check its arguments...   (a function or a program: what it writes and whether it failed are kept)
#   at_once_wait || status=1              (prints them all, in order; fails if any did)
AT_ONCE_DIR=$(mktemp -d)
AT_ONCE_N=0
at_once() {
    AT_ONCE_N=$((AT_ONCE_N + 1))
    while [ "$(jobs -r | wc -l)" -ge "${JOBS:-8}" ]; do sleep 1; done
    (
        "$@" > "$AT_ONCE_DIR/$AT_ONCE_N.out" 2>&1 || touch "$AT_ONCE_DIR/$AT_ONCE_N.failed"
    ) &
}
at_once_wait() {
    wait
    at_once_i=1
    at_once_bad=0
    while [ $at_once_i -le $AT_ONCE_N ]; do
        cat "$AT_ONCE_DIR/$at_once_i.out"
        [ -e "$AT_ONCE_DIR/$at_once_i.failed" ] && at_once_bad=1
        at_once_i=$((at_once_i + 1))
    done
    rm -rf "$AT_ONCE_DIR"
    AT_ONCE_DIR=$(mktemp -d)
    AT_ONCE_N=0
    return $at_once_bad
}
