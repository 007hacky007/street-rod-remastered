#!/bin/sh
# Builds Street Rod Enhanced for Windows in an MSYS2 UCRT64 shell, runs the unit tests and packs
# dist/Street-Rod-Enhanced-windows.zip: the launcher, the game, their DLLs and an empty Game folder.
#
#   tools/windows/package.sh
# Needs: mingw-w64-ucrt-x86_64-{gcc,cmake,ninja,sdl3,wxwidgets3.2-msw} and zip.
set -eu

root=$(cd "$(dirname "$0")/../.." && pwd)
build="$root/build-win"

cmake -S "$root" -B "$build" -G Ninja -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ \
    -DCMAKE_BUILD_TYPE=Release -DSRE_LAUNCHER=ON -DSRE_TESTS=ON
cmake --build "$build"
ctest --test-dir "$build" --output-on-failure

out="$root/dist/Street Rod Enhanced"
rm -rf "$root/dist"
mkdir -p "$out/Game"
cp "$build/Street Rod Enhanced.exe" "$build/streetrod-enhanced.exe" "$build"/*.dll "$out/"
strip "$out"/*.exe
cp "$root/LICENSE" "$out/LICENSE.txt"
cp "$root/tools/windows/Read me.txt" "$out/"
(cd "$root/dist" && zip -qr Street-Rod-Enhanced-windows.zip "Street Rod Enhanced")
(cd "$root/dist" && sha256sum Street-Rod-Enhanced-windows.zip > Street-Rod-Enhanced-windows.zip.sha256)
echo "built $root/dist/Street-Rod-Enhanced-windows.zip"
