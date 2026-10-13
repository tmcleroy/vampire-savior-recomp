#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Tommy McLeroy
#
# Release build for macOS (Apple silicon and Intel in one program, macOS 13 and later):
# build/release/VampireSavior-macOS.zip holding "Vampire Savior Recomp.app" (the launcher, with the game
# beside it). Needs the translated code of a build with your ROM set (lib.sh). SDL 3 is built in. The app is
# signed ad hoc and not notarized, so Gatekeeper warns the first time it is opened.
set -eu
. "$(dirname "$0")/lib.sh"
BUILD="$ROOT/build/release-macos"
ARCH="-DCMAKE_OSX_DEPLOYMENT_TARGET=13.0 -DCMAKE_OSX_ARCHITECTURES=arm64;x86_64"
sdl_static macos $ARCH
build_programs "$BUILD" $ARCH -DCMAKE_PREFIX_PATH="$DEPS/sdl-macos"

NAME="$(cfg title)"
APP="$OUT/$NAME.app"
rm -rf "$APP" && mkdir -p "$APP/Contents/MacOS" "$APP/Contents/Resources"
cp "$BUILD/launcher/$GAME-launcher" "$BUILD/game/$GAME" "$APP/Contents/MacOS/"
strip -x "$APP/Contents/MacOS/$GAME-launcher" "$APP/Contents/MacOS/$GAME"
sed -e "s/@VERSION@/$VERSION/g" -e "s/@NAME@/$NAME/g" -e "s/@APP_ID@/$(cfg app_id)/g" -e "s/@EXE@/$GAME-launcher/g" \
    "$ROOT/tools/release/Info.plist" > "$APP/Contents/Info.plist"
add_notices "$APP/Contents/Resources" "$BUILD/launcher"
codesign --force --deep -s - "$APP"

for binary in "$APP/Contents/MacOS/"*; do
    if otool -L "$binary" | grep -q '/opt/homebrew\|/usr/local'; then
        echo "error: $binary links a library outside macOS" >&2
        exit 1
    fi
done
rm -f "$OUT/VampireSavior-macOS.zip"
(cd "$OUT" && ditto -c -k --norsrc --noextattr --keepParent "$NAME.app" VampireSavior-macOS.zip)
check_archive "$OUT/VampireSavior-macOS.zip"
echo "wrote $OUT/VampireSavior-macOS.zip"
