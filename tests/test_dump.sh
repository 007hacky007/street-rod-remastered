#!/bin/sh
# --dump-pictures: every picture of LIB1/LIB2 as an indexed BMP plus pictures.jsonl, and the race-view
# shapes (shapes.jsonl).
#   test_dump.sh <streetrod-enhanced> <game dir>
set -eu
game=$1 dir=$2
out=$(mktemp -d)
trap 'rm -rf "$out"' EXIT
"$game" --game-dir "$dir" --dump-pictures "$out" >/dev/null
fail=0
n=$(wc -l < "$out/pictures.jsonl" | tr -d ' ')
[ "$n" = 277 ] || { echo "FAIL records: $n"; fail=1; }
grep -q '"id":2,"w":24,"h":64,"masked":1' "$out/pictures.jsonl" || { echo "FAIL id 2 record"; fail=1; }
for s in $(sed 's/.*"sha1":"\([0-9a-f]*\)".*/\1/' "$out/pictures.jsonl"); do
    [ -f "$out/$s.bmp" ] || { echo "FAIL missing $s.bmp"; fail=1; break; }
done
ns=$(grep -c '"kind":"shape"' "$out/shapes.jsonl" 2>/dev/null || true)
[ "${ns:-0}" -ge 10 ] || { echo "FAIL shapes: ${ns:-0}"; fail=1; }
for s in $(sed 's/.*"sha1":"\([0-9a-f]*\)".*/\1/' "$out/shapes.jsonl" 2>/dev/null); do
    [ -f "$out/$s.bmp" ] && [ -f "$out/$s.mask.bmp" ] || { echo "FAIL missing shape $s"; fail=1; break; }
done
[ $fail = 0 ] && echo "ok   dump"
exit $fail
