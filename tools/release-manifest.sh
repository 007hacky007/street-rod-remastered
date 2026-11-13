#!/bin/sh
# Writes DIST/Street-Rod-Enhanced.txt, the update manifest the launcher reads from the latest release
# (launcher/update.cpp): the version and the size and SHA-256 of the disk image and the Windows zip.
#   tools/release-manifest.sh 0.6.0 dist
set -eu
version=$1 dist=$2
case $version in
    *[!0-9.]* | .* | *. | *..*) echo "release-manifest: not a version: $version" >&2; exit 2 ;;
esac
[ "$(printf %s "$version" | tr -cd . | wc -c | tr -d ' ')" = 2 ] ||
    { echo "release-manifest: not a version: $version" >&2; exit 2; }
sha() {
    if command -v sha256sum >/dev/null 2>&1; then sha256sum "$1"; else shasum -a 256 "$1"; fi | cut -d' ' -f1
}
size() { wc -c < "$1" | tr -d ' '; }
dmg=$dist/Street-Rod-Enhanced.dmg zip=$dist/Street-Rod-Enhanced-windows.zip
for f in "$dmg" "$zip"; do
    [ -f "$f" ] || { echo "release-manifest: missing $f" >&2; exit 1; }
done
{
    echo "version=$version"
    echo "format=1"
    echo "mac.sha256=$(sha "$dmg")"
    echo "mac.size=$(size "$dmg")"
    echo "windows.sha256=$(sha "$zip")"
    echo "windows.size=$(size "$zip")"
} > "$dist/Street-Rod-Enhanced.txt"
