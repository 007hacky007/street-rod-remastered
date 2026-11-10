"""Repairs for pictures that keep failing: the hue lock recolours texels to the hue of the original colour
they belong to, keeping their shading (the engine's palette-relative rule, src/remaster/rm_color.c)."""
import re
import numpy as np

SCALE = 4


def _lum(c):
    return c[..., 0] * 0.299 + c[..., 1] * 0.587 + c[..., 2] * 0.114


def shade(texel, ref):
    """Texel brightness against ref, applied to ref (both float N x 3)."""
    k = (_lum(texel) + 1.0) / (_lum(ref) + 1.0)
    k = k[..., None]
    return np.where(k <= 1, ref * k, ref + (255 - ref) * (1 - 1 / k)).clip(0, 255)


def tiles_of(reasons, kind='colour'):
    """(x0, x1, y0, y1) in original pixels of the check's failing tiles of one kind."""
    out = []
    for r in reasons:
        m = re.search(r'pixels (\d+)-(\d+) x (\d+)-(\d+): .*%s' % kind, r)
        if m:
            out.append(tuple(int(v) for v in m.groups()))
    return out


def hue_lock(rgba, idx, ref, tiles, orig_idx=None):
    """rgba / idx: the art; ref: 16 x 0xRRGGBB; tiles: regions in original pixels; orig_idx: the original
    picture's indices (each texel then takes the hue of the original pixel it covers, and idx is set to it:
    the art's own idx follows the drifted colour). Returns (rgba, idx) repaired copies."""
    out, oix = rgba.copy(), idx.copy()
    pal = np.array([[c >> 16, c >> 8 & 255, c & 255] for c in ref], np.float32)
    up = np.repeat(np.repeat(orig_idx, SCALE, 0), SCALE, 1) if orig_idx is not None else idx
    for x0, x1, y0, y1 in tiles:
        ys, xs = slice(y0 * SCALE, (y1 + 1) * SCALE), slice(x0 * SCALE, (x1 + 1) * SCALE)
        t = out[ys, xs, :3].astype(np.float32)
        oix[ys, xs] = up[ys, xs]
        out[ys, xs, :3] = shade(t, pal[oix[ys, xs]]).astype(np.uint8)
    return out, oix
