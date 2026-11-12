"""The palette each picture is shown with, from the engine's palette logs (SR_REMASTER_PALETTE_LOG)."""

DARK = 0x40      # a palette whose brightest channel stays below this is a fade, not the picture's colours


def is_dark(pal):
    return max(max(int(c[i:i + 2], 16) for i in (0, 2, 4)) for c in pal) < DARK


def merge(known, records):
    """known: sha1 -> palette (16 x 'RRGGBB'); records: logged {'sha1', 'pal'} in order. The first palette that
    is not dark wins; a dark one already known is replaced."""
    out = {k: v for k, v in known.items() if not is_dark(v)}
    for r in records:
        if not is_dark(r['pal']):
            out.setdefault(r['sha1'], r['pal'])
    return out


def fill_like(out, cat):
    """Pictures never seen: the palette of a sibling (catalogue pal_like = the sibling's id in the same game,
    'main' when the entry has main ids, else its first game)."""
    by_id = {}
    for k, e in cat.items():
        for g, ids in e.get('ids', {}).items():
            for i in ids:
                by_id[(g, i)] = k
    for k, e in cat.items():
        like = e.get('pal_like')
        if not like or k in out:
            continue
        games = e.get('games') or list(e.get('ids', {}))
        if not games:
            continue
        g = 'main' if 'main' in e.get('ids', {}) else games[0]
        src = by_id.get((g, like))
        if src in out:
            out[k] = out[src]
