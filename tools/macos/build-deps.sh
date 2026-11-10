#!/bin/sh
# Builds the libraries the macOS app links statically: SDL3 and wxWidgets (core, base, net), arm64, for
# macOS 12 and newer, into build-deps/prefix. A library already installed there is skipped, so CI can cache
# the prefix.
#
#   tools/macos/build-deps.sh
set -eu

SDL_TAG=release-3.4.20
WX_TAG=v3.2.12
TARGET=12.0

root=$(cd "$(dirname "$0")/../.." && pwd)
deps="$root/build-deps"
src="$deps/src"
prefix="$deps/prefix"
mkdir -p "$src" "$prefix"

common="-G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=arm64
        -DCMAKE_OSX_DEPLOYMENT_TARGET=$TARGET -DCMAKE_INSTALL_PREFIX=$prefix
        -DCMAKE_PREFIX_PATH=$prefix -DCMAKE_IGNORE_PREFIX_PATH=/opt/homebrew;/usr/local"

fetch() {  # name tag url [extra git args]
    if [ ! -d "$src/$1" ]; then
        git -c advice.detachedHead=false clone --depth 1 --branch "$2" $4 "$3" "$src/$1"
    fi
}

if [ ! -f "$prefix/lib/libSDL3.a" ]; then
    fetch SDL "$SDL_TAG" https://github.com/libsdl-org/SDL.git ""
    # shellcheck disable=SC2086
    cmake -S "$src/SDL" -B "$deps/build-sdl" $common \
        -DSDL_SHARED=OFF -DSDL_STATIC=ON -DSDL_TEST_LIBRARY=OFF -DSDL_TESTS=OFF -DSDL_EXAMPLES=OFF
    cmake --build "$deps/build-sdl"
    cmake --install "$deps/build-sdl"
fi

if ! ls "$prefix"/lib/libwx_osx_cocoau_core-3.2*.a >/dev/null 2>&1; then
    fetch wxWidgets "$WX_TAG" https://github.com/wxWidgets/wxWidgets.git "--recurse-submodules --shallow-submodules"
    # shellcheck disable=SC2086
    cmake -S "$src/wxWidgets" -B "$deps/build-wx" $common \
        -DwxBUILD_SHARED=OFF -DwxBUILD_SAMPLES=OFF -DwxBUILD_TESTS=OFF -DwxBUILD_DEMOS=OFF \
        -DwxBUILD_BENCHMARKS=OFF -DwxUSE_WEBREQUEST=ON \
        -DwxUSE_ZLIB=builtin -DwxUSE_LIBPNG=builtin -DwxUSE_LIBJPEG=builtin -DwxUSE_LIBTIFF=builtin \
        -DwxUSE_EXPAT=builtin -DwxUSE_REGEX=builtin -DwxUSE_LIBWEBP=builtin -DwxUSE_NANOSVG=builtin
    cmake --build "$deps/build-wx"
    cmake --install "$deps/build-wx"
fi

echo "dependencies in $prefix"
