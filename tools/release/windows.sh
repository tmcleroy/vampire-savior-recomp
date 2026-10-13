#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Tommy McLeroy
#
# Release build for Windows x64, cross-compiled with MinGW-w64 on Linux:
# build/release/VampireSavior-windows-x64.zip (launcher, game, SDL3.dll, zlib1.dll, licences). Needs the packages
# of tools/release/Dockerfile, and the translated code of a build with your ROM set (lib.sh).
set -eu
. "$(dirname "$0")/lib.sh"
BUILD="$ROOT/build/release-windows"
SDL="$DEPS/SDL3-mingw-$SDL_VERSION"
if [ ! -d "$SDL/x86_64-w64-mingw32" ]; then
    curl -fsSL -o "$DEPS/sdl3-mingw.tar.gz" "https://github.com/libsdl-org/SDL/releases/download/release-$SDL_VERSION/SDL3-devel-$SDL_VERSION-mingw.tar.gz"
    echo "$SDL_MINGW_SHA256  $DEPS/sdl3-mingw.tar.gz" | sha256 -c - >/dev/null
    mkdir -p "$SDL" && tar -xzf "$DEPS/sdl3-mingw.tar.gz" -C "$SDL" --strip-components=1
fi
sdl_source # (for its licence text)
build_programs "$BUILD" -DCMAKE_TOOLCHAIN_FILE="$ROOT/tools/release/mingw-w64.cmake" \
    -DCMAKE_PREFIX_PATH="$SDL/x86_64-w64-mingw32" -DCMAKE_EXE_LINKER_FLAGS="-static"

STAGE="$BUILD/stage/VampireSavior"
rm -rf "$BUILD/stage" && mkdir -p "$STAGE"
cp "$BUILD/launcher/$GAME-launcher.exe" "$BUILD/game/$GAME.exe" "$SDL/x86_64-w64-mingw32/bin/SDL3.dll" "$STAGE/"
cp "$(find /usr/x86_64-w64-mingw32 /usr/lib/gcc/x86_64-w64-mingw32 -name zlib1.dll | head -1)" "$STAGE/"
x86_64-w64-mingw32-strip "$STAGE"/*.exe
# (linked statically but for SDL3.dll, zlib1.dll and Windows' own; anything else here is a mistake)
if x86_64-w64-mingw32-objdump -p "$STAGE"/*.exe | grep -i 'DLL Name' | grep -viE 'SDL3|zlib1|KERNEL32|msvcrt|USER32|SHELL32|ADVAPI32|ole32|api-ms-|GDI32|IMM32|WINMM|VERSION|SETUPAPI|OLEAUT32|COMDLG32|WS2_32'; then
    echo "error: a program needs a DLL that is not in the archive" >&2
    exit 1
fi
add_notices "$STAGE" "$BUILD/launcher"
# (zlib1.dll is in the archive and the MinGW-w64 and GCC runtimes are linked into the programs)
cp "$(ls /usr/share/doc/libz-mingw-w64*/copyright | head -1)" "$STAGE/licenses/zlib.txt"
cp /usr/share/doc/mingw-w64-common/copyright "$STAGE/licenses/MinGW-w64-runtime.txt"
cp /usr/share/doc/gcc-mingw-w64-base/copyright "$STAGE/licenses/GCC-runtime.txt"
rm -f "$OUT/VampireSavior-windows-x64.zip"
(cd "$BUILD/stage" && zip -qr "$OUT/VampireSavior-windows-x64.zip" VampireSavior)
check_archive "$OUT/VampireSavior-windows-x64.zip"
echo "wrote $OUT/VampireSavior-windows-x64.zip"
