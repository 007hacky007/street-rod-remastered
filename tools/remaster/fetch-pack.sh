#!/bin/sh
# Downloads the published art pack, checks it and unpacks it as DIR/Remaster, for the packaging scripts'
# --pack. A new pack: attach its zip to a release and change the URL and the checksum here.
#
#   tools/remaster/fetch-pack.sh DIR
set -eu

url=https://github.com/007hacky007/streetrod-enhanced/releases/download/v0.4.0/Street-Rod-Enhanced-remaster-pack.zip
sha256=40de8200f22a588cd468ba22b6868224835295a7b152cb0208777c78c525587e

dir=$1
mkdir -p "$dir"
curl -fsSL --retry 3 -o "$dir/pack.zip" "$url"
if command -v sha256sum >/dev/null; then sum=$(sha256sum "$dir/pack.zip"); else sum=$(shasum -a 256 "$dir/pack.zip"); fi
[ "${sum%% *}" = "$sha256" ] || { echo "the art pack's checksum doesn't match" >&2; exit 1; }
rm -rf "$dir/Remaster"
unzip -q "$dir/pack.zip" -d "$dir"
rm "$dir/pack.zip"
[ -f "$dir/Remaster/pack.json" ] || { echo "no pack.json in the art pack" >&2; exit 1; }
echo "art pack in $dir/Remaster"
