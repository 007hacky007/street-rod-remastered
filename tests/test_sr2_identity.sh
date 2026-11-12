#!/bin/sh
# Street Rod 2 with an identity art pack (its own pictures at 4x) must look as without one: headless runs
# with SR_REMASTER_SELFTEST count the replaced pixels whose colour differs (0 wanted). Also a pack with art
# for only some pictures (every player's case) and a missing pack folder (the game runs as without it), and
# the palette log, whose keys must all be the dump's (a key taken after the in-place unpack would not be).
#   test_sr2_identity.sh <streetrod2-enhanced> <SR2 game dir> <repo>
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
python3 "$repo/tools/remaster/rm.py" identity-pack "$tmp/pack" "$tmp/dump" >/dev/null || { echo "FAIL identity-pack"; exit 1; }
cp -R "$tmp/pack" "$tmp/half"
python3 -I -c '
import json, sys
p = sys.argv[1] + "/pack.json"
m = json.load(open(p))
m["pictures"] = {k: v for k, v in m["pictures"].items() if k[0] in "01234567"}
json.dump(m, open(p, "w"))
' "$tmp/half"

run() {   # run NAME PACK MS [VAR=VALUE ...] -- [ARGS ...]: in the background, stderr in $tmp/NAME.err
    name=$1 pack=$2 ms=$3
    shift 3
    cp -R "$dir" "$tmp/game_$name"
    vars=
    while [ "$1" != -- ]; do vars="$vars $1"; shift; done
    shift
    (env SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy SR_REMASTER_SELFTEST=1 $vars \
        "$exe" --game-dir "$tmp/game_$name" --remaster-dir "$pack" --run-ms "$ms" "$@" >/dev/null 2>"$tmp/$name.err"
     echo $? >"$tmp/$name.rc") &
}

run demo "$tmp/pack" 150000 SR_SEED=11 "SR_REMASTER_PALETTE_LOG=$tmp/pal.jsonl" -- demo v adlib
run drag_classic "$tmp/pack" 90000 SR_DEBUG_RACE=drag -- --classic demo v adlib
run aqueduct_classic "$tmp/pack" 90000 SR_DEBUG_RACE=aqueduct -- --classic demo v adlib
run half "$tmp/half" 90000 SR_SEED=3 -- demo v adlib
run nopack "$tmp/none" 5000 -- v adlib
wait

for name in demo drag_classic aqueduct_classic half; do
    line=$(grep 'remaster selftest:' "$tmp/$name.err" | tail -1)
    frames=$(echo "$line" | sed -n 's/.*frames \([0-9]*\).*/\1/p')
    replaced=$(echo "$line" | sed -n 's/.*replaced \([0-9]*\).*/\1/p')
    bad=$(echo "$line" | sed -n 's/.*mismatched \([0-9]*\).*/\1/p')
    [ -n "$line" ] && [ "$frames" -gt 100 ] && [ "$replaced" -gt 0 ] && [ "$bad" = 0 ] &&
        ! grep -q 'art for' "$tmp/$name.err"
    check "$name" $? "${line:-no selftest line}"
done

rc=$(cat "$tmp/nopack.rc" 2>/dev/null)
[ "$rc" = 0 ] && grep -q 'remaster: cannot read' "$tmp/nopack.err"
check nopack $? "exit ${rc:-none}"

python3 -I -c '
import json, sys
log, dump = sys.argv[1], sys.argv[2]
known = {json.loads(l)["sha1"] for l in open(dump + "/pictures.jsonl")}
try:
    seen = {json.loads(l)["sha1"] for l in open(log)}
except OSError:
    seen = set()
unknown = seen - known
print("  palette log: %d pictures, %d not in the dump" % (len(seen), len(unknown)))
sys.exit(0 if len(seen) >= 40 and not unknown else 1)
' "$tmp/pal.jsonl" "$tmp/dump"
check palette_log $?
exit $fail
