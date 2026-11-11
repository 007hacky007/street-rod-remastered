#!/bin/sh
# Builds Street Rod Enhanced for Windows in an MSYS2 UCRT64 shell, runs the unit tests and packs
# dist/Street-Rod-Enhanced-windows.zip: the launcher, both games, their DLLs and empty Game and Game2 folders. The
# remastered pictures are not in it: the launcher downloads them.
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
mkdir -p "$out/Game" "$out/Game2"
cp "$build/Street Rod Enhanced.exe" "$build/streetrod-enhanced.exe" "$build/streetrod2-enhanced.exe" "$build"/*.dll "$out/"
strip "$out"/*.exe
cp "$root/LICENSE" "$out/LICENSE.txt"
cat "$root/third_party/nuked-opl3/NOTICE" "$root/third_party/nuked-opl3/LICENSE" > "$out/LICENSE-Nuked-OPL3.txt"
sh "$root/tools/check-notices.sh" "$out"
sh "$root/tools/check-package.sh" "$out" --windows
cp "$root/tools/windows/Read me.txt" "$out/"
(cd "$root/dist" && zip -qr Street-Rod-Enhanced-windows.zip "Street Rod Enhanced")
(cd "$root/dist" && sha256sum Street-Rod-Enhanced-windows.zip > Street-Rod-Enhanced-windows.zip.sha256)
echo "built $root/dist/Street-Rod-Enhanced-windows.zip"
