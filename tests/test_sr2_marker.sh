#!/bin/sh
# Street Rod 2 with a marker art pack (each picture tinted): the tag maps of a headless demo run must show
# a full-screen picture tagged and many pictures over the run, and the tint must show (selftest mismatches).
# Floors: below them look at frame*.bmp beside tags*.bmp (SR_REMASTER_TAGMAP) for pictures shown untinted.
# A drag with the enhanced race view must draw tinted art samples too (race > 0, mismatched > 0).
#   test_sr2_marker.sh <streetrod2-enhanced> <SR2 game dir> <repo>
set -u
exe=$1 dir=$2 repo=$3
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
fail=0
check() {   # check NAME CONDITION-EXIT-CODE [detail]
    if [ "$2" = 0 ]; then echo "ok   $1${3:+ ($3)}"; else echo "FAIL $1${3:+: $3}"; fail=1; fi
}

mkdir -p "$tmp/dump"
SDL_VIDEO_DRIVER=dummy "$exe" --game-dir "$dir" --dump-pictures "$tmp/dump" >/dev/null || { echo "FAIL dump"; exit 1; }
python3 "$repo/tools/remaster/rm.py" marker-pack "$tmp/pack" "$tmp/dump" >/dev/null || { echo "FAIL marker-pack"; exit 1; }
cp -R "$dir" "$tmp/game"
cp -R "$dir" "$tmp/game_drag"
mkdir -p "$tmp/map"
SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy SR_SEED=11 SR_REMASTER_SELFTEST=1 SR_REMASTER_TAGMAP="$tmp/map" \
    SR_REMASTER_TAGMAP_N=100 "$exe" --game-dir "$tmp/game" --remaster-dir "$tmp/pack" --res-scale 1 --classic \
    --run-ms 180000 demo v adlib >/dev/null 2>"$tmp/err" &
SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy SR_DEBUG_RACE=drag SR_REMASTER_SELFTEST=1 "$exe" \
    --game-dir "$tmp/game_drag" --remaster-dir "$tmp/pack" --run-ms 90000 demo v adlib >/dev/null 2>"$tmp/drag.err" &
wait

python3 -I -c '
import glob, sys
from PIL import Image
best, slots, n = 0.0, set(), 0
for f in sorted(glob.glob(sys.argv[1] + "/tags*.bmp")):
    px = Image.open(f).convert("RGB").getdata()
    tagged = 0
    for r, g, b in px:
        s = r + 256 * g
        if s:
            tagged += 1
            slots.add(s)
    best = max(best, tagged / len(px))
    n += 1
print("  %d tag maps, largest tagged share %.3f, %d slots" % (n, best, len(slots)))
sys.exit(0 if best >= 0.90 and len(slots) >= 20 else 1)
' "$tmp/map"
check coverage $?

line=$(grep 'remaster selftest:' "$tmp/err" | tail -1)
bad=$(echo "$line" | sed -n 's/.*mismatched \([0-9]*\).*/\1/p')
[ -n "$bad" ] && [ "$bad" -gt 0 ]
check selftest $? "${line:-no selftest line}"

line=$(grep 'remaster selftest:' "$tmp/drag.err" | tail -1)
bad=$(echo "$line" | sed -n 's/.*mismatched \([0-9]*\).*/\1/p')
race=$(echo "$line" | sed -n 's/.*race \([0-9]*\).*/\1/p')
[ -n "$line" ] && [ "$race" -gt 0 ] && [ "$bad" -gt 0 ]
check drag_marker $? "${line:-no selftest line}"
exit $fail
