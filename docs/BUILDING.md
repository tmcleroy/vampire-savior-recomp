# Building from source

Everything here runs from a checkout with your own ROM set in `rom/` (ignored by Git): the set's
files in that folder, or `vsavj.zip` (and `vsav.zip`, if your collection keeps the shared files
there), with the set's key file. Nothing built from it is ever committed: see
[CONTRIBUTING.md](../CONTRIBUTING.md). `games/vampire_savior/game.cfg` lists the files and their
checksums; the build tells you which one is missing or different.

## macOS

Apple clang (Command Line Tools), then `brew install cmake ninja sdl3 python`.

```sh
make                    # build/vampire_savior/vampire_savior (twice the first time: the port itself
                        # writes the deciphered program the translator works from)
make explore            # once, about ten minutes: finds the game's code by made-up play and translates it
make run                # play (windowed)
make launcher           # build/vampire_savior/vampire_savior-launcher
make app                # build/vampire_savior/Vampire Savior.app (launcher + game), a quick bundle for
                        # your own machine; a release's app is made by tools/release/macos.sh
```

## Linux (and SteamOS)

GCC 13 or later or Clang 16 or later (C++20), CMake 3.28 or later, Ninja, Python 3, zlib's and
SDL 3's development files, and what SDL needs to open a window (the list is in
`tools/release/Dockerfile`). Where the distribution has no SDL 3 package, build it once and point
CMake at it:

```sh
cmake -S SDL3-3.4.16 -B sdl-build -DCMAKE_INSTALL_PREFIX=$HOME/sdl3 && cmake --build sdl-build --target install
CMAKE_PREFIX_PATH=$HOME/sdl3 make
```

Then the same `make` targets as on macOS (not `make app`).

## Windows

Built by cross-compiling with MinGW-w64 on Linux (`tools/release/windows.sh`), or in an MSYS2
UCRT64 shell with `mingw-w64-ucrt-x86_64-{gcc,cmake,ninja,python,sdl3,zlib}` and the same `make`
targets. Microsoft's compiler has not been tried.

## The checks

```sh
make audit              # nothing from the game, no keys, no personal paths in the tracked files
make check FRAMES=40000 MOVIE=games/vampire_savior/movies/ladder.txt ARGS="--features free_play,no_damage,one_hit"
                        # translated code equals the interpreter run block by block; two runs alike
make check-blocks       # every translated block beside the interpreter: registers, flags, cycles, writes
make check-rewrites     # every readable rewrite beside the routine it replaces
make check-release FRAMES=40000
                        # the build with no translated code (the interpreters; what CI builds) against the translated one
```

[cps2recomp/docs/VERIFYING.md](../cps2recomp/docs/VERIFYING.md) says what each proves. A run with no
window and no sound device, for tests on any system:

```sh
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy build/vampire_savior/vampire_savior --rom rom --headless \
    --frames 40000 --movie games/vampire_savior/movies/ladder.txt --features free_play,no_damage,one_hit \
    --hash-picture --hash-log pictures.txt --stats
```

`--hash-picture` hashes each frame's picture, which is how builds for different systems are
compared: the same program built for macOS, Linux and Windows must write the same file.

That holds for builds made from the same translated code. Which code gets translated, and where its
blocks start, comes from the entries your own tree has found (`make explore`, and every `make run`
adds what play reaches), and the port looks at time and interrupts only where a block starts. So two
trees with different entries are both right and still differ in a few pictures: a tree built from
nothing by the steps above against the tree the releases are built from gave 21 different pictures
in the 40,000 of the ladder movie, and the same last picture. Compare hash files, and replay input
movies, between builds of one tree.

## Release builds

A release's game program holds the translated code, so it is built where the ROM set is, after
`make` and `make explore` (see [PORT.md](PORT.md) and [RELEASING.md](RELEASING.md)):

```sh
tools/release/macos.sh      # build/release/VampireSavior-macOS.zip (Apple silicon and Intel)
docker build --platform linux/amd64 -t vampiresavior-build tools/release
docker run --rm --platform linux/amd64 -v "$PWD":/src -w /src vampiresavior-build tools/release/linux.sh
docker run --rm --platform linux/amd64 -v "$PWD":/src -w /src vampiresavior-build tools/release/windows.sh
tools/release/all.sh        # all three and SHA256SUMS.txt
```

Each script ends with `tools/release/check_archive.sh`, which fails if an archive holds a game
file or a builder's home path, or lacks the licences. With `NO_GAME_CODE=1` the scripts build the
game program with no translated code (it runs a ROM set in the port's interpreters, more slowly):
that needs no ROM set, and is what CI builds to prove everything compiles and starts.
[RELEASING.md](RELEASING.md) is the whole process.
