#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Tommy McLeroy
#
# Checks the repository's tracked files for things that must never be committed (CONTRIBUTING.md):
# game files, keys, code generated from the game, personal paths and addresses, large files, and code
# of another project's that THIRD_PARTY_NOTICES.md does not list. Run it before pushing (make audit);
# CI runs it on every pull request. Exits non-zero on a finding.
#
#   tools/audit.sh            the checks every push must pass
#   tools/audit.sh --public   also what must hold before the repository is public (docs/RELEASING.md)
set -eu
cd "$(git rev-parse --show-toplevel)"
LIST="$(mktemp)"
trap 'rm -f "$LIST"' EXIT
git ls-files > "$LIST"

fail=0
check() { if [ -n "$2" ]; then echo "AUDIT FAIL: $1"; echo "$2" | head -10; fail=1; else echo "ok: $1"; fi; }
# (this script and the archive check name the things they look for, so they are not searched)
grep_files() { grep -vE '^tools/(audit|release/check_archive)\.sh$' "$LIST" | tr '\n' '\0' | xargs -0 grep -nIE "$@" 2>/dev/null || true; }

check "no game or archive file types" "$(grep -iE '\.(zip|7z|rar|key|bin|rom|chd|state|sav|eep|nv|wav|lst)$|(^|/)[a-z0-9]{2,5}\.[0-9]{2}[a-z]?m?$' "$LIST" || true)"
check "no code or lists generated from a game" "$(grep -E '(^|/)(work|build)/|(^|/)(native|z80)_[0-9]+\.cpp$|(^|/)(entries|guessed)\.txt|(^|/)game_info\.h$' "$LIST" || true)"
check "no pictures but the README's" "$(grep -iE '\.(png|jpe?g|gif|bmp|ppm|webp)$' "$LIST" | grep -vE '^docs/screenshots/[a-z0-9-]+\.(png|jpg|gif)$' || true)"
# (the published checksums of the libraries a build downloads, and the hashes of the copied toolkit's
# own files, are not keys)
check "no key-like hex strings (32 digits or more)" "$(grep_files '\b[0-9a-fA-F]{32,}\b' | grep -vE 'URL_HASH SHA256=[0-9a-f]{64}$|(^|/)COMMIT:|_SHA256=[0-9a-f]{64}$' || true)"
# (a CPS-2 board's key is two 32-bit numbers; a tool that wrote key files from them was taken out, and neither
# it nor its numbers may come back)
check "no board key written out (makekey.py with its numbers)" "$(grep_files 'makekey\.py[^|]*[ =](0x)?[0-9a-fA-F]{8}[ ,]+(0x)?[0-9a-fA-F]{8}\b' || true)"
check "no tool that writes key files" "$(grep -iE '(^|/)makekey' "$LIST" || true)"
check "no home folder paths" "$(grep_files -e '/Users/' -e '/home/[a-z]' -e 'C:\\\\Users' || true)"
check "no private session links or private network addresses" "$(grep_files -e 'claude\.ai/code/session' -e '\b192\.168\.[0-9]+\.[0-9]+' -e '\b10\.[0-9]+\.[0-9]+\.[0-9]+\b' || true)"
check "nothing that says where game files or keys can be found" "$(grep_files -i -e 'archive\.org' -e 'rom ?sites?\b' -e 'emuparadise' -e 'cps2crpt\.c' -e 'torrent' || true)"
# (the README's pictures may be larger: the model viewer's moving picture is a few megabytes)
check "no file over 256 KB (the README's pictures: 6 MB)" "$( (grep -v '^docs/screenshots/' "$LIST" | tr '\n' '\0' | xargs -0 find 2>/dev/null -prune -type f -size +256k; \
    grep '^docs/screenshots/' "$LIST" | tr '\n' '\0' | xargs -0 find 2>/dev/null -prune -type f -size +6144k) || true)"
# Files of another project's say so in their first lines; each must be one THIRD_PARTY_NOTICES.md lists
check "files of another project's are only the ones the notices list" "$(grep_files -l -e '^(//|#) *license: *BSD' -e 'SPDX-License-Identifier: (GPL-3\.0-or-later|BSD|MIT|Zlib)' \
    | grep -vE '^cps2recomp/machine/cps2crypt\.cpp$|^cps2recomp/recomp-common/launcher/src/(ui\.cpp|ui\.h|embedded\.h)$' || true)"

if [ "${1:-}" = "--public" ]; then
    echo "-- before the repository is public"
    # (ports that are not public yet are not named in one that is)
    check "no names of ports that are still private" "$(grep_files -i -e 'snesrecomp' -e 'fighting.?vipers' -e 'bonkers' -e 'puzzle.?fighter' || true)"
    check "every source file has a license line" "$(grep -E '\.(h|c|cpp|py|sh|cmake|mk)$|CMakeLists\.txt$' "$LIST" | tr '\n' '\0' | xargs -0 grep -LiE 'SPDX-License-Identifier:|^// license:' 2>/dev/null || true)"
    check "the README and the notices are there" "$(for f in README.md LICENSE THIRD_PARTY_NOTICES.md CONTRIBUTING.md CODE_OF_CONDUCT.md SECURITY.md CHANGELOG.md VERSION; do [ -f "$f" ] || echo "missing: $f"; done)"
fi
[ "$fail" = 0 ] || { echo "audit failed"; exit 1; }
echo "audit passed ($(wc -l < "$LIST" | tr -d ' ') files)"
