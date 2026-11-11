#!/bin/sh
# Checks that Street Rod 1's compiled code is the same at two commits: builds streetrod-enhanced at each
# (Release, no launcher, no tests) in temporary folders and compares every object file's code, data and
# relocations. Objects are matched by source file name.
#   tools/sr1-codecheck.sh BASE [HEAD] [--allow NAME.o ...]
# HEAD defaults to the working tree's HEAD commit. Exit 0 when every object that differs is allowed.
# SRE_PREFIX: the SDL3 prefix (default build-deps/prefix).
set -eu
[ $# -ge 1 ] || { echo "usage: $0 BASE [HEAD] [--allow NAME.o ...]" >&2; exit 2; }
base=$1; shift
head=HEAD
if [ $# -gt 0 ] && [ "$1" != "--allow" ]; then head=$1; shift; fi
allow=" "
if [ $# -gt 0 ]; then
    [ "$1" = "--allow" ] || { echo "unexpected $1" >&2; exit 2; }
    shift
    for a in "$@"; do allow="$allow$a "; done
fi
repo=$(cd "$(dirname "$0")/.." && pwd)
prefix=${SRE_PREFIX:-$repo/build-deps/prefix}
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT

build() {  # build REV DIR
    mkdir -p "$2/src"
    git -C "$repo" archive "$1" | tar -x -C "$2/src"
    cmake -S "$2/src" -B "$2/b" -G Ninja -DCMAKE_BUILD_TYPE=Release -DSRE_LAUNCHER=OFF -DSRE_TESTS=OFF \
        -DCMAKE_PREFIX_PATH="$prefix" -DCMAKE_IGNORE_PREFIX_PATH="/opt/homebrew;/usr/local" \
        "-DCMAKE_C_FLAGS=-ffile-prefix-map=$2/src=." >"$2/cmake.log" 2>&1 ||
        { cat "$2/cmake.log" >&2; exit 2; }
    cmake --build "$2/b" --target streetrod-enhanced >"$2/build.log" 2>&1 || { tail -30 "$2/build.log" >&2; exit 2; }
    mkdir -p "$2/dis"
    for d in sre_core sre_host streetrod-enhanced; do
        [ -d "$2/b/CMakeFiles/$d.dir" ] || continue
        find "$2/b/CMakeFiles/$d.dir" -name '*.c.o' | while read -r o; do
            objdump -d -r -s --no-show-raw-insn "$o" | grep -v 'file format' >"$2/dis/$(basename "$o")"
        done
    done
}

build "$base" "$tmp/base"
build "$head" "$tmp/head"

total=0 bad=0
for f in $( (ls "$tmp/base/dis"; ls "$tmp/head/dis") | sort -u); do
    total=$((total + 1))
    if [ ! -f "$tmp/head/dis/$f" ]; then st="only in BASE"
    elif [ ! -f "$tmp/base/dis/$f" ]; then st="only in HEAD"
    elif cmp -s "$tmp/base/dis/$f" "$tmp/head/dis/$f"; then st=same
    else st=DIFF
    fi
    if [ "$st" != same ]; then
        case "$allow" in *" $f "*) st="$st (allowed)" ;; *) bad=$((bad + 1)) ;; esac
    fi
    printf '%-24s %s\n' "$f" "$st"
done
echo "$total objects, $bad differ (allowed:$allow)"
[ "$bad" -eq 0 ]
