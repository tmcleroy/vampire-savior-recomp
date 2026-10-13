#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Tommy McLeroy
#
# All release builds on a Mac with Docker: macOS here, Linux and Windows in the x86_64 build image
# (tools/release/Dockerfile). Writes build/release/ with the three downloads and SHA256SUMS.txt.
# Needs the translated code of a build with your ROM set (`make`, `make explore`).
set -eu
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"
tools/audit.sh
docker build --platform linux/amd64 -q -t vampiresavior-build tools/release >/dev/null
tools/release/macos.sh
docker run --rm --platform linux/amd64 -v "$ROOT":/src -w /src vampiresavior-build tools/release/linux.sh
docker run --rm --platform linux/amd64 -v "$ROOT":/src -w /src vampiresavior-build tools/release/windows.sh
cd build/release
shasum -a 256 VampireSavior-macOS.zip VampireSavior-linux-x86_64.tar.gz VampireSavior-windows-x64.zip > SHA256SUMS.txt
cat SHA256SUMS.txt
echo "version $(cat "$ROOT/VERSION"): release files in $ROOT/build/release"
