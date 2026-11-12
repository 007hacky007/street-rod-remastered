#!/usr/bin/env python3
"""Street Rod Enhanced remaster pipeline. See tools/remaster/README.md."""
import argparse, collections, json, os, shutil, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import numpy as np  # noqa: E402
from PIL import Image, ImageDraw  # noqa: E402

from rmlib import catalog, jobs, pack  # noqa: E402
from rmlib import palettes as palettes_lib  # noqa: E402
from rmlib.check import check  # noqa: E402
from rmlib.cleanup import assign_index, cleanup  # noqa: E402
from rmlib.images import unique_pictures  # noqa: E402
from rmlib.progress import Progress  # noqa: E402
from rmlib.register import register  # noqa: E402
from rmlib.repair import find_offset, hue_lock, splice, tiles_of  # noqa: E402

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(HERE))
ART = os.environ.get('SRE_REMASTER_ART') or os.path.join(REPO, 'remaster-art')   # a worktree has none of its own
CATALOG = os.path.join(HERE, 'catalog.json')
DUMPS = [os.path.join(ART, 'dump', g) for g in ('main', 'datadisk', 'sr2')]
MIN_SCORE = 0.3


def load_catalog():
    return json.load(open(CATALOG))


def palettes():
    p = os.path.join(ART, 'palettes.json')
    if not os.path.exists(p):
        return {}
    return {k: [int(c, 16) for c in v] for k, v in json.load(open(p)).items()}


def progress():
    return Progress(os.path.join(ART, 'progress.json'))


def cmd_palettes(logs):
    """Merge SR_REMASTER_PALETTE_LOG files: per picture the first palette it was shown with that is not dark (a
    fade); 'default' = the most common one (for pictures never seen)."""
    path = os.path.join(ART, 'palettes.json')
    known = json.load(open(path)) if os.path.exists(path) else {}
    known.pop('default', None)
    records = [json.loads(line) for log in logs for line in open(log) if line.strip()]
    out = palettes_lib.merge(known, records)
    palettes_lib.fill_like(out, load_catalog())  # pictures never seen: the palette of a sibling (pal_like)
    common = collections.Counter(tuple(v) for v in out.values()).most_common(1)
    if common:
        out['default'] = list(common[0][0])
    json.dump(out, open(path, 'w'), indent=1)
    print(len(out) - (1 if common else 0), 'pictures with a palette')


def cmd_jobs(batch):
    """Jobs for every catalogue entry of the batch: one per sheet name, else one per picture."""
    pr = progress()
    sheets = jobs.group(load_catalog(), batch)
    for name, shas in sheets.items():
        pr.add(name, shas)
    print(len(sheets), 'jobs for batch', batch)


def cmd_job(name):
    pr = progress()
    d = jobs.build(name, pr.jobs[name]['sha1s'], load_catalog(), DUMPS, palettes(), os.path.join(ART, 'jobs'))
    print(d)


def cmd_ingest(name, result, again=False):
    cat, pal, pr = load_catalog(), palettes(), progress()
    meta = json.load(open(os.path.join(ART, 'jobs', name, 'job.json')))
    raw = os.path.join(ART, 'raw')
    os.makedirs(raw, exist_ok=True)
    tries = pr.jobs[name]['tries']
    kept = result if again else os.path.join(raw, '%s.%d.png' % (name, tries + 1))
    if not again:
        shutil.copyfile(result, kept)
    res = np.asarray(Image.open(kept).convert('RGBA'))
    sx, sy = res.shape[1] / meta['canvas'][0], res.shape[0] / meta['canvas'][1]
    pics = unique_pictures(DUMPS)
    os.makedirs(os.path.join(ART, 'art'), exist_ok=True)
    all_ok, reasons, rows = True, [], []
    for cell in meta['cells']:
        s = cell['sha1']
        p = pics[s]
        ref = jobs.palette_of(s, pal)
        orig = jobs.picture_rgb(p, ref)
        x, y, w, h = cell['rect']
        rect = register(jobs.target_rgb(p, ref), res[..., :3], (x * sx, y * sy, w * sx, h * sy))
        ok, why = rect[4] >= MIN_SCORE, [] if rect[4] >= MIN_SCORE else ['not found (score %.2f)' % rect[4]]
        rgba, ix = cleanup(p.idx, p.transparent, res, rect[:4], ref)
        if ok:
            ok, why = check(orig, ~p.transparent, rgba)
        pack.write_entry(os.path.join(ART, 'art'), s, rgba, ix, ref)
        json.dump({'ok': ok, 'reasons': why, 'rect': rect[:4], 'score': rect[4], 'job': name, 'raw': kept,
                   'review': None, 'ref': ['%06X' % c for c in ref]},
                  open(os.path.join(ART, 'art', 'pics', s + '.json'), 'w'), indent=1)
        all_ok &= ok
        reasons += ['%s: %s' % (s[:8], r) for r in why]
        rows.append((orig, rgba, why))
    review_sheet(name, rows)
    if again:                                    # an earlier result again: not a new try
        j = pr.jobs[name]
        j['status'], j['reasons'] = ('done' if all_ok else j['status']), reasons
        pr.save()
    else:
        pr.result(name, all_ok, reasons)
    print(name, 'ok' if all_ok else 'FAILED', pr.jobs[name]['status'])
    for r in reasons[:12]:
        print('  ', r)


def review_sheet(name, rows):
    """remaster-art/review/<job>.png: each original (4x, hard pixels) beside its cleaned-up art."""
    tiles = []
    for orig, rgba, why in rows:
        h, w = orig.shape[:2]
        a = Image.fromarray(orig).resize((w * 4, h * 4), Image.NEAREST)
        bg = Image.new('RGB', (w * 4, h * 4), (40, 40, 40))
        bg.paste(Image.fromarray(rgba[..., :3]), (0, 0), Image.fromarray(rgba[..., 3]))
        t = Image.new('RGB', (w * 8 + 8, h * 4), (255, 0, 0) if why else (0, 160, 0))
        t.paste(a, (0, 0))
        t.paste(bg, (w * 4 + 8, 0))
        tiles.append(t)
    W = max(t.width for t in tiles)
    sheet = Image.new('RGB', (W, sum(t.height + 8 for t in tiles)), (20, 20, 20))
    y = 0
    for t in tiles:
        sheet.paste(t, (0, y))
        y += t.height + 8
    os.makedirs(os.path.join(ART, 'review'), exist_ok=True)
    sheet.save(os.path.join(ART, 'review', name + '.png'))


def cmd_repair(name, only=None, whole=False):
    """The hue lock on the tiles that failed for colour (or only those containing a point of `only`, a list
    of (x, y) original pixels), then the checks again."""
    pal, pr = palettes(), progress()
    pics = unique_pictures(DUMPS)
    src = os.path.join(ART, 'art', 'pics')
    all_ok, reasons, rows = True, [], []
    for s in pr.jobs[name]['sha1s']:
        m = json.load(open(os.path.join(src, s + '.json')))
        p, ref = pics[s], jobs.palette_of(s, pal)
        orig = jobs.picture_rgb(p, ref)
        res = np.asarray(Image.open(m['raw']).convert('RGBA'))     # from the generated image again
        rgba, ix = cleanup(p.idx, p.transparent, res, m['rect'], ref)
        tiles = tiles_of(check(orig, ~p.transparent, rgba)[1])
        if whole and tiles:                      # the whole picture: no seams between locked and unlocked tiles
            tiles = [(0, p.w - 1, 0, p.h - 1)]
        if only:
            tiles = [t for t in tiles if any(t[0] <= x <= t[1] and t[2] <= y <= t[3] for x, y in only)]
        if tiles:
            rgba, ix = hue_lock(rgba, ix, ref, tiles, p.idx)
            ok, why = check(orig, ~p.transparent, rgba)
            pack.write_entry(os.path.join(ART, 'art'), s, rgba, ix, ref)
            m.update(ok=ok, reasons=why, repaired='hue lock: %d tiles' % len(tiles))
            json.dump(m, open(os.path.join(src, s + '.json'), 'w'), indent=1)
        all_ok &= m['ok']
        reasons += ['%s: %s' % (s[:8], r) for r in m['reasons']]
        rows.append((jobs.picture_rgb(p, ref), rgba, m['reasons']))
    review_sheet(name, rows)
    j = pr.jobs[name]
    j['reasons'] = reasons
    j['status'] = 'done' if all_ok else 'attention'
    pr.save()
    print(name, 'repaired' if all_ok else 'still failing', j['status'])
    for r in reasons[:12]:
        print('  ', r)


def erode(mask, n):
    """mask without the pixels within n (Chebyshev distance) of a False one."""
    out = mask.copy()
    for _ in range(n):
        m = out.copy()
        m[1:] &= out[:-1]; m[:-1] &= out[1:]
        m[:, 1:] &= out[:, :-1]; m[:, :-1] &= out[:, 1:]
        m[1:, 1:] &= out[:-1, :-1]; m[:-1, :-1] &= out[1:, 1:]
        m[1:, :-1] &= out[:-1, 1:]; m[:-1, 1:] &= out[1:, :-1]
        out = m
    return out


def cmd_splice(sha1, bg, cell=0, common=False, max_diff=None, margin=0):
    """Opaque frames that carry a copy of the scene (sha1) drawn over the scene picture bg: each frame (cells
    of `cell` pixels across, 0 = the whole picture) takes the scene's art outside its figure; with common,
    not near the scene's own figure (where at least two frames differ from the scene). Run after the
    ingest of sha1; a new ingest undoes it. Takes the art from the last ingest (the .json raw), not a spliced one."""
    pics, src = unique_pictures(DUMPS), os.path.join(ART, 'art', 'pics')
    p, b = pics[sha1], pics[bg]
    m = json.load(open(os.path.join(src, sha1 + '.json')))
    ref = [int(c, 16) for c in m['ref']] if m.get('ref') else jobs.palette_of(sha1, palettes())
    if m.get('repaired'):
        sys.exit('%s was repaired (%s); splice works on the cleaned-up result only' % (sha1[:8], m['repaired']))
    res = np.asarray(Image.open(m['raw']).convert('RGBA'))     # from the generated image again
    rgba, ix = cleanup(p.idx, p.transparent, res, m['rect'], ref)
    brgba = np.asarray(Image.open(os.path.join(src, bg + '.png')).convert('RGBA'))
    bix = np.asarray(Image.open(os.path.join(src, bg + '.idx.png')))
    cell = cell or p.w
    done, cells = [], []
    for c0 in range(0, p.w, cell):
        fr = p.idx[:, c0:c0 + cell]
        cells.append((c0, fr, find_offset(fr, b.idx)))
    allowed = None
    if common:                  # the scene's own figure: where at least two frames differ from the scene
        votes = sum((b.idx[y0:y0 + fr.shape[0], x0:x0 + fr.shape[1]] != fr).astype(int) for _, fr, (x0, y0) in cells)
        allowed = erode(votes < min(2, len(cells)), margin)
    for c0, fr, at in cells:
        xs = slice(c0 * 4, (c0 + fr.shape[1]) * 4)
        r, x, n = splice(rgba[:, xs], ix[:, xs], fr, brgba, bix, b.idx, at, allowed, max_diff)
        rgba, ix = rgba.copy(), ix.copy()
        rgba[:, xs], ix[:, xs] = r, x
        done.append({'cell': c0, 'at': list(at), 'pixels': n})
        print('cell %d at %d,%d: %d pixels from the scene' % (c0, at[0], at[1], n))
    pack.write_entry(os.path.join(ART, 'art'), sha1, rgba, ix, ref)
    m['spliced'] = {'bg': bg, 'common': common, 'margin': margin, 'max_diff': max_diff, 'cells': done}
    json.dump(m, open(os.path.join(src, sha1 + '.json'), 'w'), indent=1)


def set_review(sha1, verdict, note):
    path = os.path.join(ART, 'art', 'pics', sha1 + '.json')
    m = json.load(open(path))
    m['review'] = {'verdict': verdict, 'note': note}
    json.dump(m, open(path, 'w'), indent=1)
    pr = progress()
    job = pr.jobs[m['job']]
    if verdict in ('accept', 'original'):
        settled = []
        for s in job['sha1s']:
            mm = json.load(open(os.path.join(ART, 'art', 'pics', s + '.json')))
            settled.append(mm['ok'] or (mm.get('review') or {}).get('verdict') in ('accept', 'original'))
        if all(settled):
            job['status'] = 'done'
            pr.save()
    if verdict == 'reject' and job['status'] == 'done':
        job['status'] = 'failed'
        job['reasons'] = ['%s: rejected on review: %s' % (sha1[:8], note)]
        pr.save()


def cmd_reindex():
    """Every picture's texel indices again from its art as it is now (after repairs), by assign_index."""
    pics, src, n, changed = unique_pictures(DUMPS), os.path.join(ART, 'art', 'pics'), 0, 0
    for f in sorted(os.listdir(src)):
        if not f.endswith('.json'):
            continue
        s, m = f[:-5], json.load(open(os.path.join(src, f)))
        if not os.path.exists(os.path.join(src, s + '.png')):
            continue
        ref = [int(c, 16) for c in m['ref']] if m.get('ref') else jobs.palette_of(s, palettes())
        rgba = np.asarray(Image.open(os.path.join(src, s + '.png')).convert('RGBA'))
        path = os.path.join(src, s + '.idx.png')
        old = np.asarray(Image.open(path))
        ix = assign_index(rgba[..., :3], rgba[..., 3], pics[s].idx, ref)
        Image.fromarray(ix, 'L').save(path)
        n += 1
        changed += int((ix != old).sum())
    print(n, 'pictures,', changed, 'texels moved to another index')


def cmd_pack(out):
    """The art that passed the checks and was not rejected on review (or was accepted on review)."""
    src = os.path.join(ART, 'art', 'pics')
    refs = {}
    os.makedirs(os.path.join(out, 'pics'), exist_ok=True)
    for f in sorted(os.listdir(src)):
        if not f.endswith('.json'):
            continue
        s = f[:-5]
        m = json.load(open(os.path.join(src, f)))
        rv = (m.get('review') or {}).get('verdict')
        if rv in ('reject', 'original') or (not m['ok'] and rv != 'accept'):
            continue
        for ext in ('.png', '.idx.png'):
            shutil.copyfile(os.path.join(src, s + ext), os.path.join(out, 'pics', s + ext))
        # the palette the art was made in (kept at ingest), not a later one
        refs[s] = [int(c, 16) for c in m['ref']] if m.get('ref') else jobs.palette_of(s, palettes())
    pack.write_meta(out, refs)
    print(len(refs), 'pictures in', out)


def cmd_status():
    pr, cat = progress(), load_catalog()
    c = collections.Counter(j['status'] for j in pr.jobs.values())
    print(dict(c), 'jobs;', sum(1 for e in cat.values() if e.get('batch')), 'of', len(cat), 'pictures in a batch')
    for n, j in pr.jobs.items():
        if j['status'] == 'attention':
            print('  attention', n, '; '.join(j['reasons'][:3]))


def main():
    ap = argparse.ArgumentParser(prog='rm.py')
    sub = ap.add_subparsers(dest='cmd', required=True)
    c = sub.add_parser('catalog', help='merge dumps (GAME=DIR ...) into catalog.json')
    c.add_argument('dumps', nargs='+')
    for name in ('identity-pack', 'marker-pack'):
        s = sub.add_parser(name)
        s.add_argument('out')
        s.add_argument('dumps', nargs='+')
    i = sub.add_parser('install')
    i.add_argument('pack')
    i.add_argument('dest')
    sub.add_parser('palettes').add_argument('logs', nargs='+')
    sub.add_parser('jobs').add_argument('batch', type=int)
    sub.add_parser('job').add_argument('name')
    sub.add_parser('next')
    g = sub.add_parser('ingest')
    g.add_argument('name')
    g.add_argument('result')
    g.add_argument('--again', action='store_true', help='an earlier raw result again, not a new try')
    for v in ('accept', 'reject', 'original'):
        r = sub.add_parser(v)
        r.add_argument('sha1')
        r.add_argument('note', nargs='?', default='')
    rp = sub.add_parser('repair')
    rp.add_argument('name')
    rp.add_argument('--at', nargs='*', default=None, help='only the tiles containing these X,Y original pixels')
    rp.add_argument('--all', action='store_true', help='lock the whole picture (no seams)')
    sp = sub.add_parser('splice', help='opaque frames: the scene art outside the figure')
    sp.add_argument('sha1')
    sp.add_argument('bg')
    sp.add_argument('--cell', type=int, default=0)
    sp.add_argument('--common', action='store_true', help="not near the scene's own figure (two frames differ from the scene there)")
    sp.add_argument('--margin', type=int, default=0, help="with --common: keep this far from the scene's own figure")
    sp.add_argument('--max-diff', type=float, default=None, help='only where the two drawings agree (mean RGB difference)')
    sub.add_parser('reindex', help='texel indices again from the art as it is (after repairs)')
    sub.add_parser('pack').add_argument('out')
    sub.add_parser('status')
    a = ap.parse_args()
    if a.cmd == 'catalog':
        dumps = [tuple(d.split('=', 1)) for d in a.dumps]
        cat = catalog.merge(CATALOG, dumps)
        print(len(cat), 'pictures')
        for game, _ in dumps:
            t, s, o = catalog.overlap(cat, game)
            print('%s: %d pictures, %d also in other games, %d only in %s' % (game, t, s, o, game))
    elif a.cmd == 'identity-pack':
        print(pack.identity_pack(a.dumps, a.out), 'pictures')
    elif a.cmd == 'marker-pack':
        print(pack.marker_pack(a.dumps, a.out), 'pictures')
    elif a.cmd == 'install':
        pack.install(a.pack, a.dest)
    elif a.cmd == 'palettes':
        cmd_palettes(a.logs)
    elif a.cmd == 'jobs':
        cmd_jobs(a.batch)
    elif a.cmd == 'job':
        cmd_job(a.name)
    elif a.cmd == 'next':
        print(progress().next() or '')
    elif a.cmd == 'reindex':
        cmd_reindex()
    elif a.cmd == 'ingest':
        cmd_ingest(a.name, a.result, a.again)
    elif a.cmd in ('accept', 'reject', 'original'):
        set_review(a.sha1, a.cmd, a.note)
    elif a.cmd == 'repair':
        cmd_repair(a.name, [tuple(int(v) for v in p.split(',')) for p in a.at] if a.at else None, a.all)
    elif a.cmd == 'splice':
        cmd_splice(a.sha1, a.bg, a.cell, a.common, a.max_diff, a.margin)
    elif a.cmd == 'pack':
        cmd_pack(a.out)
    elif a.cmd == 'status':
        cmd_status()


if __name__ == '__main__':
    main()
