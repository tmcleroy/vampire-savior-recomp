#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Tommy McLeroy
#
# Release build for Linux x86_64 (Steam Deck and Steam Machine included):
# build/release/VampireSavior-linux-x86_64.tar.gz (launcher, game, licences). Run on x86_64 Linux
# with the packages of tools/release/Dockerfile (docs/BUILDING.md), and the translated code of a build
# with your ROM set (lib.sh). SDL 3 is built in, so the programs need only the system's usual X11 or
# Wayland, sound and zlib libraries.
set -eu
. "$(dirname "$0")/lib.sh"
BUILD="$ROOT/build/release-linux"
sdl_static linux
build_programs "$BUILD" -DCMAKE_PREFIX_PATH="$DEPS/sdl-linux" -DCMAKE_EXE_LINKER_FLAGS="-static-libstdc++ -static-libgcc"

STAGE="$BUILD/stage/VampireSavior"
rm -rf "$BUILD/stage" && mkdir -p "$STAGE"
cp "$BUILD/launcher/$GAME-launcher" "$BUILD/game/$GAME" "$STAGE/"
strip "$STAGE/$GAME-launcher" "$STAGE/$GAME"
add_notices "$STAGE" "$BUILD/launcher"
rm -f "$OUT/VampireSavior-linux-x86_64.tar.gz"
tar -czf "$OUT/VampireSavior-linux-x86_64.tar.gz" -C "$BUILD/stage" VampireSavior
check_archive "$OUT/VampireSavior-linux-x86_64.tar.gz"
echo "wrote $OUT/VampireSavior-linux-x86_64.tar.gz"
