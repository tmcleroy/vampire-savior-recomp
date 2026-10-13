# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Tommy McLeroy
#
# Shared by the release scripts (sourced, not run). A release's game program holds the game's program
# translated to C++, as a build from source does: the generated code in work/<game>/gen, made by `make`
# and `make explore` from the builder's own ROM set. So releases are built on the machine of someone who
# has the game, never by CI. With NO_GAME_CODE=1 the program is built with no translated code instead (it
# runs a ROM set in the port's interpreters): that is what CI builds, to prove everything compiles and
# starts on each system, and it is never what a release holds.
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
GAME="${GAME:-vampire_savior}"
VERSION="${VERSION:-$(cat "$ROOT/VERSION")}"
OUT="$ROOT/build/release"
DEPS="$ROOT/build/deps"
SDL_VERSION=3.4.16
SDL_SHA256=7322236cd12090c3eb40b9728be4d49c76f66ad17d04369584d4ecad5cf77c68
SDL_MINGW_SHA256=c7ef65bd72eabac6e5b535411dbd8d5824d0aab24fd62ff8812666b336f18a9c
cfg() { (cd "$ROOT" && python3 cps2recomp/tools/gamecfg.py "$1" "$GAME"); }
sha256() { if command -v sha256sum >/dev/null; then sha256sum "$@"; else shasum -a 256 "$@"; fi; }
mkdir -p "$OUT" "$DEPS"

# SDL 3's source, checked against its published checksum: $DEPS/SDL3-$SDL_VERSION
sdl_source() {
    if [ ! -d "$DEPS/SDL3-$SDL_VERSION/src" ]; then
        curl -fsSL -o "$DEPS/SDL3-$SDL_VERSION.tar.gz" "https://github.com/libsdl-org/SDL/releases/download/release-$SDL_VERSION/SDL3-$SDL_VERSION.tar.gz"
        echo "$SDL_SHA256  $DEPS/SDL3-$SDL_VERSION.tar.gz" | sha256 -c - >/dev/null
        tar -xzf "$DEPS/SDL3-$SDL_VERSION.tar.gz" -C "$DEPS"
    fi
}
# SDL 3 as a static library in $DEPS/sdl-$1 (the rest: more arguments for CMake)
sdl_static() {
    name=$1; shift
    if [ ! -f "$DEPS/sdl-$name/lib/cmake/SDL3/SDL3Config.cmake" ]; then
        sdl_source
        cmake -S "$DEPS/SDL3-$SDL_VERSION" -B "$DEPS/sdl-$name-build" -G Ninja -DCMAKE_BUILD_TYPE=Release -DSDL_SHARED=OFF \
            -DSDL_STATIC=ON -DSDL_TEST_LIBRARY=OFF -DCMAKE_INSTALL_PREFIX="$DEPS/sdl-$name" "$@" >/dev/null
        cmake --build "$DEPS/sdl-$name-build" --target install >/dev/null
    fi
}
# The game and the launcher, in $1 (the rest: more arguments for CMake)
build_programs() {
    build=$1; shift
    generated="$ROOT/work/$GAME"
    if [ "${NO_GAME_CODE:-}" = 1 ]; then
        generated="$build/no-game-code"
        mkdir -p "$generated/gen"
        (cd "$ROOT" && python3 cps2recomp/tools/gamecfg.py --header "$generated/gen/game_info.h" "$GAME")
    elif ! ls "$generated"/gen/native_*.cpp >/dev/null 2>&1; then
        echo "error: no translated code in $generated/gen: run 'make' and 'make explore' with your ROM set first" >&2
        exit 1
    fi
    cmake -S "$ROOT/cps2recomp" -B "$build/game" -G Ninja -DCMAKE_BUILD_TYPE=Release -DGAME="$GAME" \
        -DGAME_DIR="$ROOT/games/$GAME" -DWORK_DIR="$generated" "$@" | grep "cps2recomp:"
    cmake --build "$build/game" --target "$GAME"
    cmake -S "$ROOT/cps2recomp/launcher" -B "$build/launcher" -G Ninja -DCMAKE_BUILD_TYPE=Release \
        -DGAME_NAME="$(cfg name)" -DGAME_EXE="$GAME" -DAPP_ID="$(cfg app_id)" \
        -DHOME_ENV="$(echo "$GAME" | tr a-z A-Z)_HOME" -DDISCLAIMER="$(cfg disclaimer)" \
        -DROM_SET="$(cfg rom_set)" -DROM_PARENT="$(cfg rom_parent)" -DROM_NAME="$(cfg rom_name)" \
        -DROM_PROGRAM="$(cfg program)" -DROM_GRAPHICS="$(cfg graphics)" -DROM_SOUND_PROGRAM="$(cfg sound_program)" \
        -DROM_SAMPLES="$(cfg samples)" -DROM_KEY="$(cfg key)" -DDISPLAY_SWITCHES="$(cfg display)" "$@" >/dev/null
    cmake --build "$build/launcher"
}
# What every archive carries beside the programs: $1 the folder, $2 the launcher's build folder
check_archive() { # the archive
    if [ "${NO_GAME_CODE:-}" = 1 ]; then "$ROOT/tools/release/check_archive.sh" --no-game-code "$1"; else "$ROOT/tools/release/check_archive.sh" "$1"; fi
}
add_notices() {
    mkdir -p "$1/licenses"
    cp "$ROOT/LICENSE" "$1/LICENSE.txt"
    cp "$ROOT/THIRD_PARTY_NOTICES.md" "$ROOT/README.md" "$ROOT/CHANGELOG.md" "$1/"
    cp "$DEPS/SDL3-$SDL_VERSION/LICENSE.txt" "$1/licenses/SDL3.txt"
    cp "$2/_deps/imgui-src/LICENSE.txt" "$1/licenses/DearImGui.txt"
    cp "$(find "$2/_deps/inter-src" -iname 'LICENSE*' | head -1)" "$1/licenses/Inter-OFL.txt"
    printf 'Source for this version (GPL-3.0): https://github.com/tmcleroy/vampire-savior-recomp/tree/v%s\n' "$VERSION" > "$1/SOURCE.txt"
}
