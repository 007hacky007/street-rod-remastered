#!/bin/sh
# The enhanced view drawn in bands on worker threads must be the frame one thread draws: SR_ENH_THREADS_CHECK draws
# every composed frame both ways (odd frames in bands first) and compares the sample buffers and resolved frames byte
# for byte. A run passes only when it compared frames, the commands really ran in the bands it asked for
# (SR_ENH_THREADS, set here: never the machine's core count), art samples were drawn with an art pack and none
# without, a full command list was met where SR_ENH_CMDS makes it small, and the compare tool's scale-1 frames were
# checked where SR_ENH_COMPARE is on. Runs (res scale 4, aa 2, 8 bands, each without art and with an identity art
# pack unless named otherwise): Street Rod 2's drag, town drive, Grudge Night, Aqueduct, Aqueduct against the King
# (his rotated sprites) and Mulholland (SR_SEED 4: the bridges, the mirror passes); with art only: drag_b3 (3 bands,
# res scale 3, aa 3), aqueduct_b7 (7 bands, res scale 1, aa 1), mulholland_cmds (a 50-command list),
# mulholland_cmp and aqking_cmp (SR_ENH_COMPARE); Street Rod 1's demo race (SR_SEED 3), with art also demo_b3 and
# demo_cmds; with the remaster's self-test on (it draws in bands too), SR2 drag_selftest and SR1 demo_selftest,
# whose self-test line must show no mismatched sample and race samples counted. Every run must show unloaded 0 (no
# command body met a picture whose art was never looked up). 23 runs, at most 6 games at once.
#   test_enh_threads.sh <streetrod-enhanced|-> <SR1 game dir|-> <streetrod2-enhanced|-> <SR2 game dir|-> <repo>
set -u
exe1=$1 dir1=$2 exe2=$3 dir2=$4 repo=$5
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
fail=0
started=0
names=

field() { echo "$1" | sed -n "s/.* $2 \([0-9]*\).*/\1/p"; }
check() {   # check NAME ART BANDS: frames compared, none differing, in BANDS bands, art samples iff ART
    name=$1 art=$2 want=$3
    line=$(grep -a 'enh threads check:' "$tmp/$name.err" | tail -1)
    frames=$(field "$line" frames) bad=$(field "$line" differing) bands=$(field "$line" bands)
    a=$(field "$line" art) cmp=$(field "$line" cmp) unl=$(field "$line" unloaded)
    why=
    [ -n "$line" ] || why="no check line"
    [ -z "$why" ] && [ "${frames:-0}" -le 20 ] && why="too few frames"
    [ -z "$why" ] && { [ "$bad" != 0 ] || grep -aq 'differing [1-9]' "$tmp/$name.err"; } && why="frames differ"
    [ -z "$why" ] && [ "$bands" != "$want" ] && why="bands ${bands:-missing}, want $want"
    [ -z "$why" ] && [ -z "$a" ] && why="no art field"
    [ -z "$why" ] && [ "$art" = 1 ] && [ "$a" -eq 0 ] && why="no art drawn"
    [ -z "$why" ] && [ "$art" = 0 ] && [ "$a" -ne 0 ] && why="art drawn without a pack"
    [ -z "$why" ] && [ "$unl" != 0 ] && why="unloaded ${unl:-missing}, want 0"
    case $name in *_cmds)
        [ -z "$why" ] && ! grep -aq 'command list is full' "$tmp/$name.err" && why="the command list was never full" ;;
    esac
    case $name in *_selftest)
        st=$(grep -a 'remaster selftest:' "$tmp/$name.err" | tail -1)
        [ -z "$why" ] && [ -z "$st" ] && why="no self-test line"
        [ -z "$why" ] && [ "$(field "$st" mismatched)" != 0 ] && why="self-test: $st"
        [ -z "$why" ] && [ "$(field "$st" race)" -eq 0 ] && why="self-test counted no race sample: $st" ;;
    esac
    case $name in *_cmp)
        [ -z "$why" ] && [ "${cmp:-0}" -eq 0 ] && why="no compare-tool frame checked" ;;
    esac
    if [ -z "$why" ]; then echo "ok   $name ($line)"
    else echo "FAIL $name: $why: ${line:-}"; fail=1; fi
}
pack() {    # pack EXE DIR OUT
    mkdir -p "$3.dump"
    SDL_VIDEO_DRIVER=dummy "$1" --game-dir "$2" --dump-pictures "$3.dump" >/dev/null 2>&1 &&
        python3 "$repo/tools/remaster/rm.py" identity-pack "$3" "$3.dump" >/dev/null
}
slot() {    # waits until fewer than 6 games run
    while [ $((started - $(ls "$tmp" | grep -c '\.done$'))) -ge 6 ]; do sleep 1; done
}
# game NAME ART BANDS EXE DIR [VAR=value ...] -- ARGS: a game in the background, its stderr in $tmp/NAME.err
game() {
    name=$1 art=$2 want=$3 exe=$4 gdir=$5
    shift 5
    envs=
    while [ $# -gt 0 ] && [ "$1" != -- ]; do envs="$envs $1"; shift; done
    [ $# -gt 0 ] && shift
    [ "$art" = 1 ] && set -- "$@" --remaster-dir "$tmp/pack_$exe_kind"
    slot
    started=$((started + 1))
    names="$names $name:$art:$want"
    cp -R "$gdir" "$tmp/g_$name"
    if [ "$exe_kind" = 1 ]; then    # Street Rod 1 has no --run-ms: stopped after 80 s (the demo's first race)
        ( env $envs "$exe" --game-dir "$tmp/g_$name" "$@" demo >/dev/null 2>"$tmp/$name.err" &
          p=$!; sleep 80; kill $p 2>/dev/null; wait $p; touch "$tmp/$name.done" ) &
    else
        ( env $envs "$exe" --game-dir "$tmp/g_$name" "$@" demo v >/dev/null 2>"$tmp/$name.err"
          touch "$tmp/$name.done" ) &
    fi
}
export SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy SR_ENH_THREADS_CHECK=1 SR_ENH_THREADS=8
S4="--res-scale 4 --aa 2"
if [ "$exe2" != - ]; then
    pack "$exe2" "$dir2" "$tmp/pack_2" || { echo "FAIL SR2 identity pack"; exit 1; }
    exe_kind=2
    R="SR_SEED=2 SR_KEYS=1:03"
    for art in 0 1; do
        game "sr2_grudge_$art" $art 8 "$exe2" "$dir2" $R SR_DEBUG_RACE=grudge -- $S4 --run-ms 200000
    done
    for art in 0 1; do
        game "sr2_drag_$art" $art 8 "$exe2" "$dir2" $R SR_DEBUG_RACE=drag -- $S4 --run-ms 40000
        game "sr2_cruise_$art" $art 8 "$exe2" "$dir2" $R SR_DEBUG_RACE=cruise -- $S4 --run-ms 40000
        game "sr2_aqueduct_$art" $art 8 "$exe2" "$dir2" $R SR_DEBUG_RACE=aqueduct -- $S4 --run-ms 40000
        game "sr2_aqking_$art" $art 8 "$exe2" "$dir2" $R SR_DEBUG_RACE=aqueduct SR_DEBUG_OPP=21 -- $S4 --run-ms 40000
        game "sr2_mulholland_$art" $art 8 "$exe2" "$dir2" $R SR_SEED=4 SR_DEBUG_RACE=mulholland -- $S4 --run-ms 40000
    done
    game sr2_drag_b3 1 3 "$exe2" "$dir2" $R SR_DEBUG_RACE=drag SR_ENH_THREADS=3 -- --res-scale 3 --aa 3 --run-ms 40000
    game sr2_aqueduct_b7 1 7 "$exe2" "$dir2" $R SR_DEBUG_RACE=aqueduct SR_ENH_THREADS=7 -- --res-scale 1 --aa 1 \
        --run-ms 40000
    game sr2_mulholland_cmds 1 8 "$exe2" "$dir2" $R SR_SEED=4 SR_DEBUG_RACE=mulholland SR_ENH_CMDS=50 -- $S4 \
        --run-ms 40000
    game sr2_mulholland_cmp 1 8 "$exe2" "$dir2" $R SR_SEED=4 SR_DEBUG_RACE=mulholland \
        SR_ENH_COMPARE="$tmp/mulholland.cmp" -- $S4 --run-ms 40000
    game sr2_aqking_cmp 1 8 "$exe2" "$dir2" $R SR_DEBUG_RACE=aqueduct SR_DEBUG_OPP=21 \
        SR_ENH_COMPARE="$tmp/aqking.cmp" -- $S4 --run-ms 40000
    game sr2_drag_selftest 1 8 "$exe2" "$dir2" $R SR_DEBUG_RACE=drag SR_REMASTER_SELFTEST=1 -- $S4 --run-ms 40000
fi
if [ "$exe1" != - ]; then
    pack "$exe1" "$dir1" "$tmp/pack_1" || { echo "FAIL SR1 identity pack"; exit 1; }
    exe_kind=1
    game sr1_demo_0 0 8 "$exe1" "$dir1" SR_SEED=3 -- $S4
    game sr1_demo_1 1 8 "$exe1" "$dir1" SR_SEED=3 -- $S4
    game sr1_demo_b3 1 3 "$exe1" "$dir1" SR_SEED=3 SR_ENH_THREADS=3 -- --res-scale 3 --aa 3
    game sr1_demo_cmds 1 8 "$exe1" "$dir1" SR_SEED=3 SR_ENH_CMDS=50 -- $S4
    game sr1_demo_selftest 1 8 "$exe1" "$dir1" SR_SEED=3 SR_REMASTER_SELFTEST=1 -- $S4
fi
wait
for n in $names; do
    IFS=: read -r name art want <<EOF
$n
EOF
    check "$name" "$art" "$want"
done
exit $fail
