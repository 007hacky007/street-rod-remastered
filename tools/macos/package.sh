#!/bin/sh
# Builds Street Rod Enhanced for macOS (arm64, macOS 12+) against build-deps/prefix (tools/macos/build-deps.sh),
# runs the unit tests and assembles dist/Street Rod Enhanced.app, ad-hoc signed. With --pack, the art pack in
# DIR goes inside the app (Contents/Resources/Remaster).
#
#   tools/macos/package.sh --version 0.4.0 [--pack DIR]
set -eu

version= pack=
while [ $# -gt 0 ]; do
    case "$1" in
        --version) version=$2; shift 2 ;;
        --pack) pack=$2; shift 2 ;;
        *) echo "usage: $0 --version X.Y.Z [--pack DIR]" >&2; exit 2 ;;
    esac
done
[ -n "$version" ] || { echo "usage: $0 --version X.Y.Z [--pack DIR]" >&2; exit 2; }
[ -z "$pack" ] || [ -f "$pack/pack.json" ] || { echo "no art pack in $pack" >&2; exit 1; }

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
cp "$build/streetrod-enhanced" "$app/Contents/MacOS/"
cp "$root/launcher/macos/AppIcon.icns" "$root/LICENSE" "$app/Contents/Resources/"
[ -z "$pack" ] || { mkdir -p "$app/Contents/Resources/Remaster"; cp "$pack/pack.json" "$app/Contents/Resources/Remaster/"; cp -R "$pack/pics" "$app/Contents/Resources/Remaster/"; }
sed "s/@VERSION@/$version/g" "$root/launcher/macos/Info.plist.in" > "$app/Contents/Info.plist"
plutil -lint "$app/Contents/Info.plist" >/dev/null
strip -x "$app/Contents/MacOS/Street Rod Enhanced" "$app/Contents/MacOS/streetrod-enhanced"

codesign --force -s - "$app/Contents/MacOS/streetrod-enhanced"
codesign --force -s - "$app"
codesign --verify --strict "$app"
echo "built $app"
