#!/bin/sh
# Street Rod 2's --music-volume and --sfx-volume: the title runs headless for 12 s with each sound device
# and the RMS of the output from 3 s on says whether the music sounded.
#   test_sr2_volumes.sh <streetrod2-enhanced> <SR2 game dir>
set -u
exe=$1 dir=$2
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
cp -R "$dir" "$tmp/game"
fail=0

rms() {  # rms <wav>: of the output from 3 s on
    python3 -I -c '
import sys, wave, struct, math
w = wave.open(sys.argv[1]); rate, ch = w.getframerate(), w.getnchannels()
w.readframes(3 * rate); d = w.readframes(w.getnframes())
s = struct.unpack("<%dh" % (len(d) // 2), d)
print("%.1f" % (math.sqrt(sum(x * x for x in s) / len(s)) if s else 0))' "$1"
}

run() {  # run <name> <want: loud|quiet> <args...>
    name=$1 want=$2; shift 2
    SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy SR_AUDIO_DUMP="$tmp/$name.wav" \
        "$exe" --game-dir "$tmp/game" --run-ms 12000 "$@" >"$tmp/$name.log" 2>&1
    r=$(rms "$tmp/$name.wav" 2>/dev/null || echo -1)
    if [ "$want" = loud ]; then python3 -I -c "import sys; sys.exit(0 if $r > 100 else 1)"
    else python3 -I -c "import sys; sys.exit(0 if 0 <= $r < 5 else 1)"; fi
    if [ $? -eq 0 ]; then echo "ok   $name (rms $r)"; else echo "FAIL $name (rms $r, want $want)"; fail=1; fi
}

run adlib_default loud v adlib
run adlib_music_0 quiet --music-volume 0 v adlib
run adlib_sfx_0 loud --sfx-volume 0 v adlib
run speaker_default loud v speaker
run speaker_music_0 quiet --music-volume 0 v speaker
run speaker_both_0 quiet --music-volume 0 --sfx-volume 0 v speaker
exit $fail
