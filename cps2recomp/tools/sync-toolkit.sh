#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Tommy McLeroy. Part of cps2recomp.
#
# Keeps a game repository's copy of the cps2recomp toolkit and checks that nobody changed it there.
#
#   cps2recomp/tools/sync-toolkit.sh           copy the toolkit as committed in its own repository (HEAD)
#                                              into cps2recomp/ and record the commit
#   cps2recomp/tools/sync-toolkit.sh --check   fail if cps2recomp/ is not what was recorded (make check-toolkit)
#
# Run from the game repository's top. A clone of the toolkit is looked for beside the game's repository
# (../cps2recomp), or where CPS2RECOMP says. The copy is committed in the game's repository, which so builds
# from a fresh clone on its own. Never change the copy by hand: change the toolkit in its repository (build
# and check a game against it with make CR=../cps2recomp), commit there, and run this in each game.
set -eu

main() {
    dest=cps2recomp
    [ -d games ] || { echo "sync-toolkit: run from a game repository's top (no games/ here)" >&2; exit 1; }
    src=${CPS2RECOMP:-$(pwd)/../cps2recomp}
    record="$dest/COMMIT"
    if command -v shasum > /dev/null; then sum="shasum -a 256"; else sum="sha256sum"; fi
    # the copy's files and their hashes, one a line
    hashes() {
        (cd "$dest" && find . -type f ! -path ./COMMIT | sed 's|^\./||' | LC_ALL=C sort | while read -r f; do
            printf '%s  %s\n' "$($sum < "$f" | cut -d' ' -f1)" "$f"
        done)
    }

    if [ "${1:-}" = "--check" ]; then
        [ -f "$record" ] || { echo "sync-toolkit: no $record; run cps2recomp/tools/sync-toolkit.sh" >&2; exit 1; }
        commit=$(sed -n 's/^commit //p' "$record")
        if [ "$(hashes)" != "$(grep -v '^#' "$record" | grep -v '^commit ')" ]; then
            echo "sync-toolkit: cps2recomp/ is not what cps2recomp $commit gave (changed by hand?)." >&2
            echo "  Change the toolkit in its own repository and run cps2recomp/tools/sync-toolkit.sh." >&2
            exit 1
        fi
        echo "cps2recomp/: commit $commit"
        exit 0
    fi

    git -C "$src" rev-parse HEAD > /dev/null 2>&1 || { echo "sync-toolkit: no cps2recomp at $src (set CPS2RECOMP)" >&2; exit 1; }
    commit=$(git -C "$src" rev-parse HEAD)
    if [ -n "$(git -C "$src" status --porcelain)" ]; then
        echo "sync-toolkit: note: $src has changes not committed; copying it as committed ($commit)" >&2
    fi
    # (only ever a copy this script made is replaced)
    if [ -e "$dest" ] && [ ! -f "$record" ] && [ "${2:-}" != "--first" ] && [ "${1:-}" != "--first" ]; then
        echo "sync-toolkit: $dest is there and is not a copy made by this script (--first to replace it)" >&2
        exit 1
    fi
    tmp=$(mktemp -d)
    git -C "$src" archive "$commit" | tar -x -C "$tmp"
    if [ -d "$dest" ]; then
        find "$dest" -type f -delete
        find "$dest" -depth -type d -empty -delete
    fi
    mkdir -p "$dest"
    (cd "$tmp" && tar -c .) | tar -x -C "$dest"
    rm -rf "$tmp"
    {
        echo "# A copy of the cps2recomp toolkit, made by its tools/sync-toolkit.sh. Do not change it here."
        echo "commit $commit"
        hashes
    } > "$record"
    echo "cps2recomp/: commit $commit"
}
# (the whole script is read before any of it runs: it replaces itself)
main "$@"; exit
