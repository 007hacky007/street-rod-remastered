#!/bin/sh
# Checks that a package carries the licence notices its programs need: this project's MIT licence and,
# because the host links Nuked-OPL3 (LGPL 2.1+) into both games, its licence with a pointer to the source.
#   tools/check-notices.sh <folder with the licence files>
set -u
dir=$1
fail=0
if [ ! -f "$dir/LICENSE" ] && [ ! -f "$dir/LICENSE.txt" ]; then echo "missing: LICENSE in $dir"; fail=1; fi
n="$dir/LICENSE-Nuked-OPL3.txt"
if [ ! -f "$n" ]; then echo "missing: LICENSE-Nuked-OPL3.txt in $dir"; fail=1
else
    grep -q 'GNU LESSER GENERAL PUBLIC LICENSE' "$n" || { echo "no LGPL text in $n"; fail=1; }
    grep -q 'https://github.com/nukeykt/Nuked-OPL3' "$n" || { echo "no source pointer in $n"; fail=1; }
fi
[ $fail -eq 0 ] && echo "notices ok: $dir"
exit $fail
