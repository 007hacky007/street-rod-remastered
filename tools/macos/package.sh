#!/bin/sh
# Builds Street Rod Enhanced for macOS (arm64, macOS 12+) against build-deps/prefix (tools/macos/build-deps.sh),
# runs the unit tests and assembles dist/Street Rod Enhanced.app, ad-hoc signed: the launcher and both games. The remastered pictures are
# not in it: the launcher downloads them.
#
#   tools/macos/package.sh --version 0.5.0
set -eu

version=
while [ $# -gt 0 ]; do
    case "$1" in
        --version) version=$2; shift 2 ;;
        *) echo "usage: $0 --version X.Y.Z" >&2; exit 2 ;;
    esac
done
[ -n "$version" ] || { echo "usage: $0 --version X.Y.Z" >&2; exit 2; }

root=$(cd "$(dirname "$0")/../.." && pwd)
build="$root/build-mac"
prefix="$root/build-deps/prefix"
[ -d "$prefix/lib" ] || { echo "run tools/macos/build-deps.sh first" >&2; exit 1; }

cmake -S "$root" -B "$build" -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=arm64 \
    -DCMAKE_OSX_DEPLOYMENT_TARGET=12.0 -DCMAKE_PREFIX_PATH="$prefix" \
    "-DCMAKE_IGNORE_PREFIX_PATH=/opt/homebrew;/usr/local" -DSRE_LAUNCHER=ON -DSRE_TESTS=ON
cmake --build "$build"
ctest --test-dir "$build" --output-on-failure

app="$root/dist/Street Rod Enhanced.app"
rm -rf "$root/dist"
mkdir -p "$app/Contents/MacOS" "$app/Contents/Resources"
cp "$build/Street Rod Enhanced" "$app/Contents/MacOS/"
cp "$build/streetrod-enhanced" "$build/streetrod2-enhanced" "$app/Contents/MacOS/"
cp "$root/launcher/macos/AppIcon.icns" "$root/LICENSE" "$app/Contents/Resources/"
cat "$root/third_party/nuked-opl3/NOTICE" "$root/third_party/nuked-opl3/LICENSE" \
    > "$app/Contents/Resources/LICENSE-Nuked-OPL3.txt"
sh "$root/tools/check-notices.sh" "$app/Contents/Resources"
sed "s/@VERSION@/$version/g" "$root/launcher/macos/Info.plist.in" > "$app/Contents/Info.plist"
plutil -lint "$app/Contents/Info.plist" >/dev/null
strip -x "$app/Contents/MacOS/Street Rod Enhanced" "$app/Contents/MacOS/streetrod-enhanced" \
    "$app/Contents/MacOS/streetrod2-enhanced"
sh "$root/tools/check-package.sh" "$app/Contents/MacOS"

codesign --force -s - "$app/Contents/MacOS/streetrod-enhanced"
codesign --force -s - "$app/Contents/MacOS/streetrod2-enhanced"
codesign --force -s - "$app"
codesign --verify --strict "$app"
echo "built $app"
