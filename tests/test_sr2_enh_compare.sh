#!/bin/sh
# Street Rod 2's enhanced race view against the original frame (SR_ENH_COMPARE, src2/enhanced/enh_compare.h):
# headless drives of each kind with the demo autopilot; the enhanced kinds must match (road edges within
# 1 px in every frame, the median frame within 10 permille of mismatched pixels), be on the screen (composed at
# least once per game frame) and leave the mirror the game's; the others must not be drawn enhanced. The drag
# again with an identity art pack (drag_art): the same thresholds, and the compare tool must see palette indices
# only (its dump paints a sample that is not one magenta, FF00FFh, which the EGA palette does not have).
#   test_sr2_enh_compare.sh <streetrod2-enhanced> <SR2 game dir> <repo>
set -u
exe=$1 dir=$2 repo=$3
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
cp -R "$dir" "$tmp/game"
fail=0

mkdir -p "$tmp/dump" "$tmp/art_dump"
SDL_VIDEO_DRIVER=dummy "$exe" --game-dir "$dir" --dump-pictures "$tmp/dump" >/dev/null || { echo "FAIL dump"; exit 1; }
python3 "$repo/tools/remaster/rm.py" identity-pack "$tmp/pack" "$tmp/dump" >/dev/null || { echo "FAIL identity-pack"; exit 1; }

run() {  # run <name> <kind> <seconds> [ARGS ...]: in the background, the log in $tmp/<name>.cmp
    name=$1 kind=$2 s=$3 cmp_dump=
    shift 3
    [ "$name" = drag_art ] && cmp_dump="$tmp/art_dump"   # empty: no compare dump
    SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy SR_DEBUG_RACE=$kind SR_KEYS=1:03 SR_ENH_COMPARE="$tmp/$name.cmp" \
        SR_ENH_COMPARE_DUMP="$cmp_dump" "$exe" --game-dir "$tmp/game" "$@" --run-ms "$s"000 demo v \
        >"$tmp/$name.log" 2>&1 &
}

summary() { tail -n 1 "$tmp/$1.cmp" 2>/dev/null; }
field() { echo "$1" | awk -v k="$2" '{ for (i = 1; i < NF; i++) if ($i == k) print $(i + 1) }'; }

run drag drag 90
run cruise cruise 90
run grudge grudge 60
run mulholland mulholland 60
run drag_art drag 90 --remaster-dir "$tmp/pack"
wait

for k in drag cruise drag_art; do
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
python3 -I -c '
import glob, sys
from PIL import Image
files = sorted(glob.glob(sys.argv[1] + "/cmp*.bmp"))
bad = 0
for f in files:
    im = Image.open(f).convert("RGB")
    w, h = im.size
    bad += sum(1 for p in im.crop((w // 2, 0, w, h)).getdata() if p == (255, 0, 255))
print("  %d compare dumps, %d magenta pixels" % (len(files), bad))
sys.exit(0 if files and not bad else 1)
' "$tmp/art_dump"
if [ $? = 0 ]; then echo "ok   drag_art indices"; else echo "FAIL drag_art indices"; fail=1; fi
exit $fail
