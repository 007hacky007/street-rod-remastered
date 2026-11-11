#!/bin/sh
# Checks that a package carries both games and the launcher: "Street Rod Enhanced", streetrod-enhanced and
# streetrod2-enhanced as programs and, in the Windows zip, the empty Game and Game2 folders.
#   tools/check-package.sh <folder with the programs> [--windows]
set -u
dir=$1
ext=
win=0
[ "${2:-}" = "--windows" ] && { ext=.exe; win=1; }
fail=0
for p in "Street Rod Enhanced" streetrod-enhanced streetrod2-enhanced; do
    if [ ! -f "$dir/$p$ext" ] || [ ! -x "$dir/$p$ext" ]; then echo "missing: $p$ext"; fail=1; fi
done
if [ $win -eq 1 ]; then
    for d in Game Game2; do [ -d "$dir/$d" ] || { echo "missing: $d"; fail=1; }; done
fi
[ $fail -eq 0 ] && echo "package ok: $dir"
exit $fail
