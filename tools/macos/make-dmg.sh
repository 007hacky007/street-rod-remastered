#!/bin/sh
# Makes dist/Street-Rod-Enhanced.dmg (and its .sha256) from dist/Street Rod Enhanced.app (package.sh), with
# dmgbuild in a private virtual environment under build-deps/venv.
#
#   tools/macos/make-dmg.sh --version 0.3.0
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
app="$root/dist/Street Rod Enhanced.app"
[ -d "$app" ] || { echo "run tools/macos/package.sh first" >&2; exit 1; }

venv="$root/build-deps/venv"
if [ ! -x "$venv/bin/dmgbuild" ]; then
    python3 -m venv "$venv"
    "$venv/bin/pip" install --quiet dmgbuild==1.6.7
fi

dmg="$root/dist/Street-Rod-Enhanced.dmg"
rm -f "$dmg" "$dmg.sha256"
cd "$root"
"$venv/bin/dmgbuild" -s tools/macos/dmg/settings.py -D app="$app" -D license="$root/LICENSE" \
    -D assets="$root/tools/macos/dmg" "Street Rod Enhanced $version" "$dmg"
(cd "$root/dist" && shasum -a 256 Street-Rod-Enhanced.dmg > Street-Rod-Enhanced.dmg.sha256)
echo "built $dmg"
