#!/bin/sh
# --fps and --vsync are options of both games: taken (a missing game folder then exits 1), a bad --fps
# refused with the usage (2). The dummy video driver: no error box for the missing folder.
#   test_frame_options.sh <program>
set -u
exe=$1 fail=0
want() {  # want <exit code> <args...>
    code=$1; shift
    SDL_VIDEO_DRIVER=dummy "$exe" "$@" --game-dir /nonexistent --check >/dev/null 2>&1
    got=$?
    if [ "$got" = "$code" ]; then echo "ok   $* -> $got"; else echo "FAIL $* -> $got, want $code"; fail=1; fi
}
want 1 --fps 60 --vsync off
want 1 --fps auto --vsync on
want 1 --fps 0
want 2 --fps fast
want 2 --fps
exit $fail
