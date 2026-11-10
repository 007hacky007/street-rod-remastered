#!/bin/sh
# With an identity art pack (the original pictures at 4x) the remastered output must equal the original:
# the demo runs with SR_REMASTER_SELFTEST, which counts replaced pixels whose colour differs.
#   test_identity.sh <streetrod-enhanced> <game dir> <repo> [seconds]
# Opens a window for the duration.
set -eu
game=$1 dir=$2 repo=$3 secs=${4:-60}
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
"$game" --game-dir "$dir" --dump-pictures "$tmp" >/dev/null
python3 "$repo/tools/remaster/rm.py" identity-pack "$tmp/pack" "$tmp" >/dev/null
SR_REMASTER_SELFTEST=1 "$game" --game-dir "$dir" --remaster-dir "$tmp/pack" demo >/dev/null 2>"$tmp/err" &
pid=$!
sleep "$secs"
kill $pid 2>/dev/null || true
wait $pid 2>/dev/null || true
line=$(grep 'remaster selftest' "$tmp/err" | tail -1)
echo "$line"
frames=$(echo "$line" | sed 's/.*frames \([0-9]*\).*/\1/')
replaced=$(echo "$line" | sed 's/.*replaced \([0-9]*\).*/\1/')
bad=$(echo "$line" | sed 's/.*mismatched \([0-9]*\).*/\1/')
[ -n "$line" ] && [ "$frames" -gt 100 ] && [ "$replaced" -gt 0 ] && [ "$bad" = 0 ] && echo "ok   identity" && exit 0
echo "FAIL identity"
exit 1
