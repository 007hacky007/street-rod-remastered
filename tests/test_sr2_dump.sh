#!/bin/sh
# Street Rod 2's --dump-pictures: every picture of LIB1/LIB2/LIB3 as an indexed BMP (plus a mask BMP for the
# masked ones) and pictures.jsonl, keyed by the SHA-1 of the packed bytes; checked against an independent
# reader of the library files, for repeatability and, given Street Rod 1, for the pictures both games share
# (the same key must mean the same pixels).
#   test_sr2_dump.sh <streetrod2-enhanced> <SR2 game dir> [<streetrod-enhanced> <SR1 game dir>]
set -u
sr2=$1 dir2=$2 sr1=${3:-} dir1=${4:-}
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
out=$tmp/a again=$tmp/b one=$tmp/sr1
mkdir -p "$out" "$again" "$one"
fail=0
check() {   # check NAME CONDITION-EXIT-CODE [detail]
    if [ "$2" = 0 ]; then echo "ok   $1"; else echo "FAIL $1${3:+: $3}"; fail=1; fi
}
sha1s() { sed 's/.*"sha1":"\([0-9a-f]*\)".*/\1/' "$1/pictures.jsonl"; }

msg=$(SDL_VIDEO_DRIVER=dummy "$sr2" --game-dir "$dir2" --dump-pictures "$out" 2>&1); rc=$?
[ $rc = 0 ] && [ "$msg" = "299 pictures written to $out" ]; check dump_runs $? "exit $rc: $msg"
[ -f "$out/pictures.jsonl" ] || { echo "FAIL no pictures.jsonl"; exit 1; }

n=$(wc -l < "$out/pictures.jsonl" | tr -d ' ')
[ "$n" = 299 ]; check records $? "$n"
nd=$(sha1s "$out" | sort -u | wc -l | tr -d ' ')
[ "$nd" = 297 ]; check distinct_sha1s $? "$nd"
c1=$(grep -c '"lib":1,' "$out/pictures.jsonl") c2=$(grep -c '"lib":2,' "$out/pictures.jsonl")
c3=$(grep -c '"lib":3,' "$out/pictures.jsonl")
[ "$c1 $c2 $c3" = "4 212 83" ]; check per_lib $? "$c1 $c2 $c3"
grep -q '"lib":3,"index":0,"id":2000,' "$out/pictures.jsonl"; check lib3_id $?
grep -q '"lib":2,"index":0,"id":1000,' "$out/pictures.jsonl" && grep -q '"lib":1,"index":0,"id":1,' "$out/pictures.jsonl"
check lib12_ids $?

missing=0
for s in $(sha1s "$out"); do [ -f "$out/$s.bmp" ] || { missing=1; echo "  missing $s.bmp"; break; }; done
for s in $(grep -v '"masked":0}' "$out/pictures.jsonl" | sed 's/.*"sha1":"\([0-9a-f]*\)".*/\1/'); do
    [ -f "$out/$s.mask.bmp" ] || { missing=1; echo "  missing $s.mask.bmp"; break; }
done
[ $missing = 0 ]; check files $?
[ ! -e "$out/shapes.jsonl" ]; check no_shapes $?

# An independent reader of SR2's libraries: u16 count, 28-byte records (w +0, h +2, masked +5, u32 offset +16h
# from 2 + count * 28, u16 size +1Ah); the key is the SHA-1 of the `size` bytes at the offset (first one BCh).
python3 -I - "$dir2" "$out" <<'EOF'; check independent_reader $?
import hashlib, json, os, struct, sys
from PIL import Image
game, out = sys.argv[1], sys.argv[2]
names = {n.upper(): n for n in os.listdir(game)}
recs = {}
for line in open(os.path.join(out, "pictures.jsonl")):
    r = json.loads(line)
    recs[(r["lib"], r["index"])] = r
bad = 0
total = 0
for lib in (1, 2, 3):
    d = open(os.path.join(game, names["LIB%d" % lib]), "rb").read()
    count = struct.unpack_from("<H", d, 0)[0]
    base = 2 + count * 28
    for i in range(count):
        w, h = struct.unpack_from("<HH", d, 2 + i * 28)
        masked = d[2 + i * 28 + 5]
        off, size = struct.unpack_from("<IH", d, 2 + i * 28 + 0x16)
        off += base
        total += 1
        r = recs.get((lib, i))
        if d[off] != 0xBC or r is None:
            print("  lib %d index %d: %s" % (lib, i, "no BCh" if r else "no record")); bad += 1; continue
        sha = hashlib.sha1(d[off:off + size]).hexdigest()
        if (r["sha1"], r["w"], r["h"], r["masked"]) != (sha, w, h, masked):
            print("  lib %d index %d: %s, want %s %d x %d masked %d" % (lib, i, r, sha, w, h, masked)); bad += 1
            continue
        names_to_check = [sha + ".bmp"] + ([sha + ".mask.bmp"] if masked else [])
        for f in names_to_check:
            im = Image.open(os.path.join(out, f))
            if im.size != (w, h) or im.mode not in ("P", "L"):   # 8-bit; a grey palette opens as L
                print("  %s: %s %s, want %d x %d 8-bit" % (f, im.size, im.mode, w, h)); bad += 1
if total != len(recs):
    print("  %d library records, %d dumped" % (total, len(recs))); bad += 1
sys.exit(1 if bad else 0)
EOF

SDL_VIDEO_DRIVER=dummy "$sr2" --game-dir "$dir2" --dump-pictures "$again" >/dev/null 2>&1
d=$(diff -r "$out" "$again" 2>&1); [ -z "$d" ]; check repeatable $? "$d"

if [ -n "$sr1" ]; then
    SDL_VIDEO_DRIVER=dummy "$sr1" --game-dir "$dir1" --dump-pictures "$one" >/dev/null 2>&1
    shared=$(sha1s "$out" | sort -u > "$tmp/s2"; sha1s "$one" | sort -u > "$tmp/s1"; comm -12 "$tmp/s1" "$tmp/s2")
    ns=$(printf '%s\n' "$shared" | grep -c .)
    differ=0
    for s in $shared; do
        cmp -s "$one/$s.bmp" "$out/$s.bmp" || { differ=1; echo "  $s.bmp differs"; }
        if [ -f "$one/$s.mask.bmp" ] || [ -f "$out/$s.mask.bmp" ]; then
            cmp -s "$one/$s.mask.bmp" "$out/$s.mask.bmp" || { differ=1; echo "  $s.mask.bmp differs"; }
        fi
    done
    [ "$ns" = 48 ] && [ $differ = 0 ]; check shared $? "$ns shared"
else
    echo "skip shared"
fi
exit $fail
