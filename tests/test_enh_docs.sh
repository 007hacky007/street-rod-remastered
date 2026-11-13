#!/bin/sh
# The docs of the enhanced view's worker threads say what the code does: README's developer aids name every
# variable the bands read, SR_ENH_TRACE's fields common to both games and SR_DEBUG_FPS's compose time; ENHANCED.md
# says the remaster's self-test draws in bands (only the palette log stays on one thread); the timings are the
# measured ones (none of 1fbc94f's first figures) and every frame time quoted for the threads in ENHANCED.md and
# CHANGELOG's Unreleased entry comes with the whole frame's time, not only the enhanced view's.
#   test_enh_docs.sh <repo>
set -u
cd "$1" || exit 2
fail=0
no() { echo "FAIL $*"; fail=1; }

# README: the variables the bands read, SR_ENH_TRACE and SR_DEBUG_FPS
for v in $(grep -o 'getenv("SR_ENH_[A-Z_]*")' common/enhanced/enh_bands.c | sed 's/.*("\(.*\)")/\1/' | sort -u) \
         SR_ENH_TRACE SR_DEBUG_FPS; do
    grep -q "\`$v" README.md || no "README.md does not name $v"
done
# README: SR_ENH_TRACE's fields that both games write, each in backquotes
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
fields() { grep -A2 'fprintf(trace' "$1" | grep -o '[a-z][a-z]* %' | sed 's/ %//' | sort -u; }
fields src/enhanced/enhanced.c > "$tmp/f1"
fields src2/enhanced/enhanced.c > "$tmp/f2"
[ "$(comm -12 "$tmp/f1" "$tmp/f2" | grep -c .)" -ge 8 ] || no "fewer than 8 trace fields common to both games"
for f in $(comm -12 "$tmp/f1" "$tmp/f2"); do
    grep -q "\`$f\`" README.md || no "README.md does not name SR_ENH_TRACE's field $f"
done
# README: SR_DEBUG_FPS's compose time, in the paragraph that names SR_DEBUG_FPS
grep -q 'compose: %' common/host.c || no "host.c's SR_DEBUG_FPS line has no compose time"
awk '/SR_DEBUG_FPS/ { p = 1 } p && /compose/ { ok = 1 } /^$/ || /^\* / && !/SR_DEBUG_FPS/ { p = 0 }
     END { exit !ok }' README.md || no "README.md's SR_DEBUG_FPS does not name the compose time"
grep -n 'One CPU core draws' README.md && no "README.md says one core draws the road"

# ENHANCED.md: the self-test draws in bands, the palette log on one thread
bullet=$(awk '/^\* \*\*Worker threads\*\*/ { p = 1; print; next } p && /^\* / { exit } p' ENHANCED.md)
[ -n "$bullet" ] || no "ENHANCED.md has no Worker threads bullet"
echo "$bullet" | tr '\n' ' ' | grep -q 'self-test and palette log *keep' &&
    no "ENHANCED.md says the self-test keeps one band"
echo "$bullet" | tr '\n' ' ' | grep -q 'self-test[^.]*bands' || no "ENHANCED.md does not say the self-test draws in bands"
echo "$bullet" | tr '\n' ' ' | grep -q 'palette log[^.]*one thread' ||
    no "ENHANCED.md does not say the palette log draws on one thread"
grep -n -i 'not done[^.]*worker threads' ENHANCED.md && no "ENHANCED.md lists worker threads as not done"

# Timings: 1fbc94f's figures (a loaded machine) are gone
for old in '4.2 / 0.9' '19.3 / 4.4' '4.8 / 1.0' '19.0 / 12.7' '4.9 / 1.7' '18.4 / 3.5' '5.1 / 2.1' '6.3 / 1.6' \
           '5.9 / 2.2' '31.9 / 5.6' '32 to 6 ms'; do
    grep -n -F "$old" ENHANCED.md CHANGELOG.md README.md && no "1fbc94f's figure $old"
done
# Every threads timing comes with the whole frame's time
entry=$(awk '/^## Unreleased/ { p = 1; next } /^## / { p = 0 } p' CHANGELOG.md |
        awk '/^- / { if (e ~ /worker threads/) print e; e = "" } { e = e " " $0 } END { if (e ~ /worker threads/) print e }')
[ -n "$entry" ] || no "CHANGELOG's Unreleased has no worker threads entry"
echo "$entry" | grep -q "bsvdoom's change *to kylofon/streetrod-enhanced" || no "CHANGELOG lost bsvdoom's credit"
echo "$entry" | grep -q ' ms' && ! echo "$entry" | grep -q 'whole frame' &&
    no "CHANGELOG's threads entry quotes ms without the whole frame's time"
for sect in "$bullet" "$(awk '/^## 9\. Street Rod 2/ { p = 1 } p && /^\* / { exit } p' ENHANCED.md)"; do
    echo "$sect" | grep -q ' ms' && ! echo "$sect" | tr '\n' ' ' | grep -q 'whole frame' &&
        no "ENHANCED.md quotes the view's ms without the whole frame's: $(echo "$sect" | head -1)"
done

[ $fail = 0 ] && echo "ok enh_docs"
exit $fail
