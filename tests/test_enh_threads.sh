#!/bin/sh
# The enhanced view drawn in bands on worker threads must be the frame one thread draws: SR_ENH_THREADS_CHECK draws
# every composed frame both ways and compares the sample buffers and resolved frames byte for byte. Street Rod 1's
# demo race (SR_SEED 3) and Street Rod 2's drag, town drive, Aqueduct and Mulholland, each without art and with an
# identity art pack, at res scale 4, aa 2.
#   test_enh_threads.sh <streetrod-enhanced|-> <SR1 game dir|-> <streetrod2-enhanced|-> <SR2 game dir|-> <repo>
set -u
exe1=$1 dir1=$2 exe2=$3 dir2=$4 repo=$5
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
fail=0
check() {   # check NAME: frames compared, none differing
    line=$(grep -a 'enh threads check:' "$tmp/$1.err" | tail -1)
    frames=$(echo "$line" | sed -n 's/.*frames \([0-9]*\).*/\1/p')
    bad=$(echo "$line" | sed -n 's/.*differing \([0-9]*\).*/\1/p')
    if [ -n "$line" ] && [ "$frames" -gt 20 ] && [ "$bad" = 0 ] && ! grep -aq 'differing [1-9]' "$tmp/$1.err"; then
        echo "ok   $1 ($line)"
    else echo "FAIL $1: ${line:-no check line}"; fail=1; fi
}
pack() {    # pack EXE DIR OUT
    mkdir -p "$3.dump"
    SDL_VIDEO_DRIVER=dummy "$1" --game-dir "$2" --dump-pictures "$3.dump" >/dev/null 2>&1 &&
        python3 "$repo/tools/remaster/rm.py" identity-pack "$3" "$3.dump" >/dev/null
}
export SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy SR_ENH_THREADS_CHECK=1
if [ "$exe2" != - ]; then
    pack "$exe2" "$dir2" "$tmp/pack2" || { echo "FAIL SR2 identity pack"; exit 1; }
    for race in drag cruise aqueduct mulholland; do
        for art in none art; do
            cp -R "$dir2" "$tmp/g2_${race}_$art"
            if [ $art = art ]; then set -- --remaster-dir "$tmp/pack2"; else set --; fi
            seed=2; [ $race = mulholland ] && seed=4
            SR_SEED=$seed SR_DEBUG_RACE=$race SR_KEYS=1:03 "$exe2" --game-dir "$tmp/g2_${race}_$art" "$@" \
                --res-scale 4 --aa 2 --run-ms 40000 demo v >/dev/null 2>"$tmp/sr2_${race}_$art.err" &
        done
    done
    wait
    for race in drag cruise aqueduct mulholland; do for art in none art; do check "sr2_${race}_$art"; done; done
fi
if [ "$exe1" != - ]; then         # no --run-ms: stopped after the demo's first race
    pack "$exe1" "$dir1" "$tmp/pack1" || { echo "FAIL SR1 identity pack"; exit 1; }
    for art in none art; do
        cp -R "$dir1" "$tmp/g1_$art"
        if [ $art = art ]; then set -- --remaster-dir "$tmp/pack1"; else set --; fi
        SR_SEED=3 "$exe1" --game-dir "$tmp/g1_$art" "$@" --res-scale 4 --aa 2 demo >/dev/null 2>"$tmp/sr1_$art.err" &
        echo $! >"$tmp/sr1_$art.pid"
    done
    sleep 80
    for art in none art; do kill "$(cat "$tmp/sr1_$art.pid")" 2>/dev/null; done
    wait
    for art in none art; do check "sr1_$art"; done
fi
exit $fail
