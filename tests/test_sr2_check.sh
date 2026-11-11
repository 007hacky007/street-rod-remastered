#!/bin/sh
# Street Rod 2's executable recognises its own SR.EXE and refuses Street Rod 1's, naming the game, and
# Street Rod 1's refuses Street Rod 2's; each with exit code 1.
#   test_sr2_check.sh <streetrod2-enhanced> <SR2 game dir> [<streetrod-enhanced> <SR1 game dir>]
set -u
sr2=$1 dir2=$2 sr1=${3:-} dir1=${4:-}
fail=0
out=$("$sr2" --game-dir "$dir2" --check 2>&1); rc=$?
if [ $rc -eq 0 ] && echo "$out" | grep -q 'SR.EXE ok: image 288912 bytes'; then echo "ok   sr2_check"
else echo "FAIL sr2_check (exit $rc): $out"; fail=1; fi
if [ -n "$sr1" ]; then
    out=$(SDL_VIDEO_DRIVER=dummy "$sr2" --game-dir "$dir1" --check 2>&1); rc=$?
    if [ $rc -eq 1 ] && echo "$out" | grep -q 'Street Rod 2: The Next Generation'; then echo "ok   sr2_check_wrong_game"
    else echo "FAIL sr2_check_wrong_game (exit $rc): $out"; fail=1; fi
    out=$(SDL_VIDEO_DRIVER=dummy "$sr1" --game-dir "$dir2" --check 2>&1); rc=$?
    # Street Rod 1's message is left as it was: its size check comes first.
    if [ $rc -eq 1 ] && echo "$out" | grep -q 'unexpected load image size 288912'; then echo "ok   sr1_check_wrong_game"
    else echo "FAIL sr1_check_wrong_game (exit $rc): $out"; fail=1; fi
fi
exit $fail
