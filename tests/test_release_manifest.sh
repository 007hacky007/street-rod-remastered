#!/bin/sh
# tools/release-manifest.sh: the update manifest a release carries, read by the launcher (launcher/update.cpp).
#   test_release_manifest.sh <repo root>
set -u
root=$1 fail=0
dir=$(mktemp -d)
trap 'rm -rf "$dir"' EXIT
printf abc > "$dir/Street-Rod-Enhanced.dmg"
printf hello > "$dir/Street-Rod-Enhanced-windows.zip"
if sh "$root/tools/release-manifest.sh" 0.6.0 "$dir"; then
    want='version=0.6.0
format=1
mac.sha256=ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad
mac.size=3
windows.sha256=2cf24dba5fb0a30e26e83b2ac5b9e29e1b161e5c1fa7425e73043362938b9824
windows.size=5'
    if [ "$(cat "$dir/Street-Rod-Enhanced.txt")" = "$want" ]; then echo "ok   manifest"; else
        echo "FAIL manifest:"; cat "$dir/Street-Rod-Enhanced.txt"; fail=1; fi
else
    echo "FAIL the script failed"; fail=1
fi
if sh "$root/tools/release-manifest.sh" 0.6 "$dir" 2>/dev/null; then echo "FAIL took version 0.6"; fail=1; else
    echo "ok   refuses 0.6"; fi
rm "$dir/Street-Rod-Enhanced-windows.zip"
if sh "$root/tools/release-manifest.sh" 0.6.0 "$dir" 2>/dev/null; then echo "FAIL took a missing zip"; fail=1; else
    echo "ok   refuses a missing zip"; fi
exit $fail
