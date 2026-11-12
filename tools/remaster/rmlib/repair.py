"""Repairs for pictures that keep failing: the hue lock recolours texels to the hue of the original colour
they belong to, keeping their shading (the engine's palette-relative rule, common/remaster/rm_color.c)."""
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


def find_offset(frame_idx, bg_idx):
    """(x, y) where the frame's original pixels match the background picture best."""
    h, w = frame_idx.shape
    best = (-1.0, 0, 0)
    for y in range(bg_idx.shape[0] - h + 1):
        for x in range(bg_idx.shape[1] - w + 1):
            m = float((bg_idx[y:y + h, x:x + w] == frame_idx).mean())
            if m > best[0]:
                best = (m, x, y)
    return best[1], best[2]


def splice(rgba, idx, frame_idx, bg_rgba, bg_ix, bg_idx, at, allowed=None, max_diff=None):
    """An opaque frame that carries its own copy of the scene around a figure (the attendant at Gus'): the
    frame's pixels that equal the scene's at `at` and connect to the frame's edge through such pixels take
    the scene's remastered texels, so the frame meets the scene without a seam. Returns (rgba, idx, n)."""
    h, w = frame_idx.shape
    x0, y0 = at
    same = bg_idx[y0:y0 + h, x0:x0 + w] == frame_idx
    if allowed is not None:
        same &= allowed
    ys, xs = slice(y0 * SCALE, (y0 + h) * SCALE), slice(x0 * SCALE, (x0 + w) * SCALE)
    if max_diff is not None:
        d = np.abs(rgba[..., :3].astype(np.int32) - bg_rgba[ys, xs, :3].astype(np.int32)).sum(2)
        d = d.reshape(h, SCALE, w, SCALE).mean((1, 3)) / 3
        same &= d <= max_diff
    keep = np.zeros_like(same)
    stack = [(y, x) for y in range(h) for x in range(w) if (y in (0, h - 1) or x in (0, w - 1)) and same[y, x]]
    for y, x in stack:
        keep[y, x] = True
    while stack:
        y, x = stack.pop()
        for v, u in ((y - 1, x), (y + 1, x), (y, x - 1), (y, x + 1)):
            if 0 <= v < h and 0 <= u < w and same[v, u] and not keep[v, u]:
                keep[v, u] = True
                stack.append((v, u))
    k = np.repeat(np.repeat(keep, SCALE, 0), SCALE, 1)
    out, oix = rgba.copy(), idx.copy()
    out[k] = bg_rgba[ys, xs][k]
    oix[k] = bg_ix[ys, xs][k]
    return out, oix, int(keep.sum())
