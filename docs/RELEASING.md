# Releasing

Versions are `MAJOR.MINOR.PATCH`, with `-beta.N` or `-rc.N` while pre-release. They start at
`0.1.0-beta.1` and stay pre-release until someone has played every platform's build by hand.

## What a release is

Three archives and their checksums, as a GitHub release of a tag:

| File | For |
| --- | --- |
| `VampireSavior-macOS.zip` | macOS 13 or later, Apple silicon and Intel (one app, "Vampire Savior Recomp"; signed ad hoc, not notarized) |
| `VampireSavior-linux-x86_64.tar.gz` | Linux x86_64, Steam Deck, Steam Machine |
| `VampireSavior-windows-x64.zip` | Windows 10 and 11, 64-bit |
| `SHA256SUMS.txt` | |

Each holds the launcher, the game program and the licences. No ROM, graphics, sound or key is in
them: the player supplies their own ROM set. **The game program does hold the game's 68000 and Z80
programs translated to C++ and compiled**, as a build from source does and as most published
recompilations' downloads do. That is a form of the game's code, and giving it out is a risk the
maintainer has accepted (2026-10-12). It also means an archive can only be built by someone with the
ROM set, on their own machine: CI never builds or holds one. `tools/release/check_archive.sh`
checks each archive (no game files, no home path, the licences there, the game program really the
translated one and not CI's stand-in).

The tag's source on GitHub is the source the GPL asks for; each archive's `SOURCE.txt` names it.
The translated code is not part of that source: it is made from the player's own ROM set by the
tools that are.

## The steps

1. In a pull request: set `VERSION`, move the "Unreleased" lines of `CHANGELOG.md` under a heading
   for the version. `make audit` passes; with a ROM set, so do `make check` over the ladder movie,
   `make check-blocks`, `make check-rewrites` and `make check-release FRAMES=40000`. Merge it.
2. On `main`: `git tag -a vX.Y.Z -m "X.Y.Z"` and `git push origin vX.Y.Z`.
3. `.github/workflows/release.yml` runs on tags `v*`: the audit (`--public`), and the builds of
   `ci.yml` on GitHub's macOS, Linux and Windows machines (the game program with no translated
   code and the launcher, each started with no display: the proof the tag compiles everywhere).
   It then makes a **draft** release whose notes are the version's section of the changelog, with
   no files. A tag with `-beta` or `-rc` is marked pre-release.
4. On a Mac with Docker and the ROM set, at the tag: `make`, the checks of step 1 again, then
   `tools/release/all.sh`, which writes the three archives and `SHA256SUMS.txt` in `build/release/`.
5. A person plays each archive on its system (picture, sound, a gamepad, the launcher's pages, a
   save state, training mode).
6. `gh release upload vX.Y.Z build/release/VampireSavior-* build/release/SHA256SUMS.txt`, then
   publish the draft.

## Before the repository is public for the first time

`tools/audit.sh --public` must pass (it adds: no port that is still private is named, every source
file has a licence line, the project files are there). Then: a new repository from one commit of
the tree as it is, the private history kept in a private archive repository, the audit run on the
new tree, and only then the first tag.
