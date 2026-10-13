# Build environment for the Linux and Windows release builds (x86_64 Ubuntu 24.04):
#   docker build --platform linux/amd64 -t vampiresavior-build tools/release
#   docker run --rm --platform linux/amd64 -v "$PWD":/src -w /src vampiresavior-build tools/release/linux.sh
#   docker run --rm --platform linux/amd64 -v "$PWD":/src -w /src vampiresavior-build tools/release/windows.sh
FROM ubuntu:24.04
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
    ca-certificates g++ cmake ninja-build python3 curl file zip unzip git zlib1g-dev \
    libx11-dev libxext-dev libxrandr-dev libxcursor-dev libxi-dev libxss-dev libxtst-dev libxkbcommon-dev \
    libwayland-dev wayland-protocols libdecor-0-dev libegl-dev libgl-dev \
    libpulse-dev libasound2-dev libudev-dev libdbus-1-dev libpipewire-0.3-dev \
    g++-mingw-w64-x86-64-posix gcc-mingw-w64-x86-64-posix binutils-mingw-w64-x86-64 libz-mingw-w64-dev \
    && rm -rf /var/lib/apt/lists/*
