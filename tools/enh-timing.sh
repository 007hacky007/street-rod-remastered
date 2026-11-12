#!/bin/sh
# Where the enhanced race view's frame time goes, in every race kind of both games, at 1280 x 800 (res scale 4,
# aa 2), without art and with an identity art pack (built here from the game's own pictures). One run at a time:
# Street Rod 2's drag, town drive, Aqueduct and Mulholland (SR_SEED 2, Mulholland SR_SEED 4; 30 s each) and Street
# Rod 1's demo race (SR_SEED 3, stopped after 70 s). Per run, from SR_ENH_TRACE: the composed frames (n), the median
# and 90th percentile of the enhanced view's time (drawing and resolve, ms), the median slowest and mean band, and
# from SR_DEBUG_FPS the median of the host's per-second compose time (the art over the whole picture, the EGA
# decode and the enhanced view; seconds with enhanced frames only). Timings mean little on a busy machine: the
# load is printed first.
#   tools/enh-timing.sh <streetrod-enhanced> <SR1 game dir> <streetrod2-enhanced> <SR2 game dir> [SR_ENH_THREADS]
set -u
[ $# -ge 4 ] || { echo "usage: $0 <streetrod-enhanced> <SR1 game dir> <streetrod2-enhanced> <SR2 game dir> [bands]" >&2; exit 2; }
exe1=$1 dir1=$2 exe2=$3 dir2=$4
[ $# -ge 5 ] && export SR_ENH_THREADS="$5"
repo=$(cd "$(dirname "$0")/.." && pwd)
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
export SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy SR_DEBUG_FPS=1
S4="--res-scale 4 --aa 2"

cores=$(sysctl -n hw.logicalcpu 2>/dev/null || nproc 2>/dev/null || echo 1)
load=$(uptime | sed 's/.*load averages*: *//; s/,/ /g' | awk '{ print $1 }')
echo "load $load on $cores cores"
awk -v l="$load" -v c="$cores" 'BEGIN { exit !(l > c / 2) }' &&
    echo "warning: load $load on $cores cores, timings unreliable"

pack() {    # pack EXE DIR OUT
    mkdir -p "$3.dump"
    "$1" --game-dir "$2" --dump-pictures "$3.dump" >/dev/null 2>&1 &&
        python3 "$repo/tools/remaster/rm.py" identity-pack "$3" "$3.dump" >/dev/null
}
# the host's lines of seconds in which the trace grew (enhanced frames were composed)
keep_fps() {    # keep_fps TRACE OUT < stderr
    prev=0
    while IFS= read -r l; do
        case $l in *'[HOST FPS]'*)
            sz=$(wc -c < "$1" 2>/dev/null || echo 0)
            [ "$sz" -gt "$prev" ] && echo "$l" >> "$2"
            prev=$sz ;;
        esac
    done
}
stat_of() {     # stat_of FIELD FILE: median p90 of the number after FIELD on each line
    sed -n "s/.* $1:* \([0-9.][0-9.]*\).*/\1/p" "$2" | sort -n |
        awk '{ v[NR] = $1 } END { if (!NR) { print "- -"; exit }
             m = int((NR + 1) / 2); p = int(NR * 0.9 + 0.999); if (p < 1) p = 1
             print v[m], v[p] }'
}
report() {      # report GAME RACE ART: the run's line
    t=$tmp/run.trace
    n=$(grep -c . "$t" 2>/dev/null || echo 0)
    set -- "$1" "$2" "$3" $(stat_of ms "$t") $(stat_of bmax "$t") $(stat_of bmean "$t") $(stat_of compose "$tmp/run.fps")
    echo "$1 $2 $3 n $n median $4 p90 $5 bmax $6 bmean $8 compose ${10}"
}
run2() {        # run2 RACE ART [VAR=value ...]
    race=$1 art=$2
    shift 2
    rm -rf "$tmp/run.trace" "$tmp/run.fps" "$tmp/g" && cp -R "$dir2" "$tmp/g"
    a=
    [ "$art" = art ] && a="--remaster-dir $tmp/pack_2"
    env SR_SEED=2 SR_KEYS=1:03 SR_DEBUG_RACE="$race" SR_ENH_TRACE="$tmp/run.trace" "$@" "$exe2" --game-dir "$tmp/g" \
        $S4 $a --run-ms 30000 demo v 2>&1 >/dev/null | keep_fps "$tmp/run.trace" "$tmp/run.fps"
    report sr2 "$race" "$art"
}
run1() {        # run1 ART
    art=$1
    rm -rf "$tmp/run.trace" "$tmp/run.fps" "$tmp/g" && cp -R "$dir1" "$tmp/g"
    a=
    [ "$art" = art ] && a="--remaster-dir $tmp/pack_1"
    mkfifo "$tmp/err"
    keep_fps "$tmp/run.trace" "$tmp/run.fps" < "$tmp/err" &
    r=$!
    SR_SEED=3 SR_ENH_TRACE="$tmp/run.trace" "$exe1" --game-dir "$tmp/g" $S4 $a demo >/dev/null 2>"$tmp/err" &
    p=$!
    sleep 70
    kill $p 2>/dev/null
    wait $p $r 2>/dev/null
    rm -f "$tmp/err"
    report sr1 demo "$art"
}

pack "$exe2" "$dir2" "$tmp/pack_2" || { echo "SR2 identity pack failed" >&2; exit 1; }
pack "$exe1" "$dir1" "$tmp/pack_1" || { echo "SR1 identity pack failed" >&2; exit 1; }
for art in none art; do
    run2 drag $art
    run2 cruise $art
    run2 aqueduct $art
    run2 mulholland $art SR_SEED=4
done
run1 none
run1 art
