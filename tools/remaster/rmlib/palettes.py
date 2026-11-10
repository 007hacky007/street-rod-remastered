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
