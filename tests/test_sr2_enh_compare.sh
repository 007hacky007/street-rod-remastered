#!/bin/sh
# Street Rod 2's enhanced race view against the original frame (SR_ENH_COMPARE, src2/enhanced/enh_compare.h):
# headless drives of each kind with the demo autopilot; the enhanced kinds must match (road edges within
# 1 px in every frame, the median frame within 10 permille of mismatched pixels), be on the screen (composed at
# least once per game frame) and leave the mirror the game's; the others must not be drawn enhanced.
#   test_sr2_enh_compare.sh <streetrod2-enhanced> <SR2 game dir>
set -u
exe=$1 dir=$2
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
cp -R "$dir" "$tmp/game"
fail=0

run() {  # run <kind> <seconds>: in the background, the log in $tmp/<kind>.cmp
    SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy SR_DEBUG_RACE=$1 SR_KEYS=1:03 SR_ENH_COMPARE="$tmp/$1.cmp" \
        "$exe" --game-dir "$tmp/game" --run-ms "$2"000 demo v >"$tmp/$1.log" 2>&1 &
}

summary() { tail -n 1 "$tmp/$1.cmp" 2>/dev/null; }
field() { echo "$1" | awk -v k="$2" '{ for (i = 1; i < NF; i++) if ($i == k) print $(i + 1) }'; }

run drag 90
run cruise 90
run grudge 60
run mulholland 60
wait

for k in drag cruise; do
    s=$(summary $k)
    n=$(field "$s" frames) e=$(field "$s" worst_edge) m=$(field "$s" mismatch_p50)
    mc=$(field "$s" mirror_cov_max) sh=$(field "$s" shown)
    if [ -n "$n" ] && [ "$n" -ge 20 ] && [ "$e" -le 1 ] && [ "$m" -le 10 ] && [ "$mc" = 0 ] && [ "$sh" -ge "$n" ]; then
        echo "ok   $k ($s)"
    else echo "FAIL $k (${s:-no log})"; fail=1; fi
done
# Grudge (modes 2, 4) and Mulholland (10h = 16) are not enhanced yet; their runs also drive through town (mode 0).
for k in grudge:2:4 mulholland:16:16; do
    name=${k%%:*} m=${k#*:}; m1=${m%%:*} m2=${m#*:}
    if [ ! -f "$tmp/$name.cmp" ]; then echo "FAIL $name (no log)"; fail=1; continue; fi
    n=$(awk -v a="$m1" -v b="$m2" '/^frame / && ($4 == a || $4 == b) { c++ } END { print c + 0 }' "$tmp/$name.cmp")
    if [ "$n" = 0 ]; then echo "ok   $name not enhanced"
    else echo "FAIL $name ($n frames enhanced, want 0)"; fail=1; fi
done
exit $fail
