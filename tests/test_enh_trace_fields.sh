#!/bin/sh
# Where a frame's time goes: both games' SR_ENH_TRACE lines carry the drawing (recording) and resolve times, the
# frame's commands, flushes and bands and the slowest and mean band, and SR_DEBUG_FPS's host line the mean time of
# the whole frame's compose (the art over the picture and the enhanced view). Street Rod 2's drag (15 s) and Street
# Rod 1's demo race (SR_SEED 3, stopped after 80 s: its first race), res scale 4, aa 2.
#   test_enh_trace_fields.sh <streetrod-enhanced|-> <SR1 game dir|-> <streetrod2-enhanced|-> <SR2 game dir|->
set -u
exe1=$1 dir1=$2 exe2=$3 dir2=$4
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
fail=0
fields=' draw [0-9.][0-9.]* resolve [0-9.][0-9.]* cmds [0-9][0-9]* flushes [0-9][0-9]* bands [0-9][0-9]* bmax [0-9.][0-9.]* bmean [0-9.][0-9.]*'
check() {   # check NAME: the trace has lines, every one with the fields; stderr has the compose time
    name=$1 why=
    n=$(grep -c . "$tmp/$name.trace" 2>/dev/null)
    [ "${n:-0}" -gt 20 ] || why="too few trace lines (${n:-0})"
    [ -z "$why" ] && [ "$(grep -vc -- "$fields" "$tmp/$name.trace")" != 0 ] &&
        why="a trace line without the fields: $(grep -v -- "$fields" "$tmp/$name.trace" | head -1)"
    [ -z "$why" ] && ! grep -aq 'compose: [0-9.][0-9.]* ms' "$tmp/$name.err" &&
        why="no compose time: $(grep -a 'HOST FPS' "$tmp/$name.err" | tail -1)"
    if [ -z "$why" ]; then echo "ok   $name ($n lines: $(tail -1 "$tmp/$name.trace"))"
    else echo "FAIL $name: $why"; fail=1; fi
}
export SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy SR_DEBUG_FPS=1
if [ "$exe2" != - ]; then
    cp -R "$dir2" "$tmp/g2"
    SR_SEED=2 SR_KEYS=1:03 SR_DEBUG_RACE=drag SR_ENH_TRACE="$tmp/sr2.trace" "$exe2" --game-dir "$tmp/g2" \
        --res-scale 4 --aa 2 --run-ms 15000 demo v >/dev/null 2>"$tmp/sr2.err"
    check sr2
fi
if [ "$exe1" != - ]; then
    cp -R "$dir1" "$tmp/g1"
    SR_SEED=3 SR_ENH_TRACE="$tmp/sr1.trace" "$exe1" --game-dir "$tmp/g1" --res-scale 4 --aa 2 demo \
        >/dev/null 2>"$tmp/sr1.err" &
    p=$!
    sleep 80
    kill $p 2>/dev/null
    wait $p
    check sr1
fi
exit $fail
