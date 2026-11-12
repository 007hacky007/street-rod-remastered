#!/bin/sh
# Street Rod 2's enhanced race view against the original frame (SR_ENH_COMPARE, src2/enhanced/enh_compare.h):
# headless drives of each race kind with the demo autopilot (SR_SEED 2), judged mode by mode from the compare
# log's per-mode lines. An enhanced mode must match (road edges within 1 px in every frame, the median frame
# within 10 permille of mismatched pixels, the game's row tables, the objects re-projected within 1 px), be on
# the screen (composed at least once per game frame) and leave the mirror the game's (orig) or draw its glass
# (enh); a mode not enhanced yet must not be logged at all.
#   test_sr2_enh_compare.sh <streetrod2-enhanced> <SR2 game dir>
set -u
exe=$1 dir=$2
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
cp -R "$dir" "$tmp/game"
fail=0

run() {  # run <kind> <seconds> [race] [VAR=value ...]: in the background, the log in $tmp/<kind>.cmp; the race
         # SR_DEBUG_RACE drives is <kind> unless given
    k=$1 s=$2 race=${3:-$1}
    shift 2; [ $# -gt 0 ] && shift
    env SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy SR_SEED=2 SR_DEBUG_RACE="$race" SR_KEYS=1:03 "$@" \
        SR_ENH_COMPARE="$tmp/$k.cmp" "$exe" --game-dir "$tmp/game" --run-ms "$s"000 demo v >"$tmp/$k.log" 2>&1 &
}

field() { echo "$1" | awk -v k="$2" '{ for (i = 1; i < NF; i++) if ($i == k) { print $(i + 1); exit } }'; }
mode_line() { grep "^mode $2 " "$tmp/$1.cmp" 2>/dev/null | head -n 1; }
# frames_with <kind> <mode> <field>: frame lines of that mode whose field is > 0 (obj_err: >= 0)
frames_with() {
    awk -v m="$2" -v k="$3" '/^frame / && $4 == m {
        for (i = 1; i < NF; i++) if ($i == k && ($(i + 1) > 0 || (k == "obj_err" && $(i + 1) >= 0))) { c++; break } }
        END { print c + 0 }' "$tmp/$1.cmp"
}

check() {  # check <kind> <mode> <min frames> <orig|enh> [noobj]
    k=$1 m=$2 min=$3 mir=$4 noobj=${5:-}
    s=$(mode_line "$k" "$m")
    if [ -z "$s" ]; then
        if [ "$min" -gt 0 ]; then echo "FAIL $k mode $m (no frames)"; fail=1; else echo "ok   $k mode $m (no frames)"; fi
        return
    fi
    n=$(field "$s" frames) e=$(field "$s" worst_edge) p=$(field "$s" mismatch_p50) g=$(field "$s" geo_max)
    o=$(field "$s" obj_max) sh=$(field "$s" shown) mc=$(field "$s" mirror_cov_min)
    me=$(field "$s" mirror_worst_edge) mp=$(field "$s" mirror_mismatch_p50)
    ok=1
    [ "$n" -ge "$min" ] && [ "$e" -le 1 ] && [ "$p" -le 10 ] && [ "$g" = 0 ] && [ "$sh" -ge "$n" ] || ok=0
    [ -n "$noobj" ] || [ "$o" -le 1 ] || ok=0
    if [ "$mir" = orig ]; then [ "$mc" = 0 ] || ok=0
    else [ "$mc" = 1596 ] && [ "$me" -le 1 ] && [ "$mp" -le 10 ] || ok=0; fi
    if [ "$min" -gt 0 ]; then    # the numbers measured something
        [ "$(frames_with "$k" "$m" edge_rows)" -gt 0 ] || ok=0
        [ "$mir" = orig ] || [ "$(frames_with "$k" "$m" mirror_edge_rows)" -gt 0 ] || ok=0
        [ -n "$noobj" ] || [ "$(frames_with "$k" "$m" obj_err)" -gt 0 ] || ok=0
    fi
    if [ $ok = 1 ]; then echo "ok   $k mode $m ($s)"; else echo "FAIL $k mode $m ($s)"; fail=1; fi
}

after() {  # after <kind> <mode> <last mode> <min>: frames of <mode> after the last frame (or race start, a mode not
           # enhanced logs no frames) of <last mode>
    n=$(awk -v m="$2" -v l="$3" '/^race / && $2 == l { c = 0 }
        /^frame / { if ($4 == l) c = 0; else if ($4 == m) c++ } END { print c + 0 }' "$tmp/$1.cmp" 2>/dev/null)
    if [ -n "$n" ] && [ "$n" -ge "$4" ]; then echo "ok   $1 mode $2 after mode $3 ($n frames)"
    else echo "FAIL $1 mode $2 after mode $3 (${n:-0} frames, want $4)"; fail=1; fi
}

police_in_mirror() {  # police_in_mirror <kind>: a frame with the police out (police >= 2) behind (opp_sprite >= 9)
    n=$(awk '/^frame / { p = -1; o = -1; for (i = 1; i < NF; i++) { if ($i == "police") p = $(i + 1);
        if ($i == "opp_sprite") o = $(i + 1) } if (p >= 2 && o >= 9) c++ } END { print c + 0 }' "$tmp/$1.cmp" 2>/dev/null)
    if [ -n "$n" ] && [ "$n" -gt 0 ]; then echo "ok   police in the mirror ($n frames)"
    else echo "FAIL police in the mirror (${n:-0} frames)"; fail=1; fi
}

none() {  # none <kind> <mode>
    s=$(mode_line "$1" "$2")
    if [ ! -f "$tmp/$1.cmp" ]; then echo "FAIL $1 mode $2 (no log)"; fail=1
    elif [ -z "$s" ]; then echo "ok   $1 mode $2 not enhanced"
    else echo "FAIL $1 mode $2 ($s)"; fail=1; fi
}

run cruise 90
run drag 90
run grudge 200
run mulholland 110
run police 110 mulholland SR_DEBUG_POLICE=480
run aqueduct 80
wait

check cruise 0 20 enh
check drag 1 20 enh
check drag 0 0 enh
check grudge 0 20 enh        # the location-4 routes, there and back
after grudge 0 4 20
check grudge 2 20 enh
check grudge 4 20 enh
check mulholland 0 0 enh
check mulholland 16 20 enh
check police 16 20 enh
police_in_mirror police
none aqueduct 8
check aqueduct 0 0 enh
exit $fail
