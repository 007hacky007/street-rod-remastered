"""Does a cleaned-up picture still show the original? Compared at the original's resolution, tile by tile:
a pixel differs when its colour is far from every blend of the original colours around it (so smooth edges
pass, a moved, recoloured, added or dropped object does not)."""
import numpy as np

SCALE = 4
TILE = 16
FAR_CHROMA = 45.0       # colour (Cb, Cr) distance from the nearest blend of the neighbours' colours
FAR_LUMA = 110.0        # brightness may change more: the remaster shades
TILE_BAD = 0.08         # a tile fails when this share of its pixels differ ...
TILE_MIN = 6            # ... and at least this many
EDGE_MIN = 12           # tiles with at least this many original outline pixels ...
EDGE_RECALL = 0.55      # ... need this share of them on an outline of the remaster


def _ycc(a):
    a = a.astype(np.float32)
    y = a[..., 0] * 0.299 + a[..., 1] * 0.587 + a[..., 2] * 0.114
    cb = (a[..., 2] - y) * 0.564
    cr = (a[..., 0] - y) * 0.713
    return np.stack([y * (FAR_CHROMA / FAR_LUMA), cb, cr], -1)     # luma scaled: one threshold for both


def _down(rgba):
    h, w = rgba.shape[0] // SCALE, rgba.shape[1] // SCALE
    return rgba[:h * SCALE, :w * SCALE, :3].astype(np.float32).reshape(h, SCALE, w, SCALE, 3).mean((1, 3))


def _seg_dist(p, a, b):
    ab = b - a
    t = ((p - a) * ab).sum(-1) / np.maximum((ab * ab).sum(-1), 1e-6)
    t = np.clip(t, 0, 1)[..., None]
    return np.sqrt(((p - (a + t * ab)) ** 2).sum(-1))


def differs(orig_rgb, rgba):
    """h x w bool: pixels whose remastered colour is not a blend of the original colours around them (hue
    strictly, brightness loosely)."""
    o = _ycc(orig_rgb)
    d = _ycc(_down(rgba))
    h, w = o.shape[:2]
    pad = np.pad(o, ((1, 1), (1, 1), (0, 0)), mode='edge')
    nb = [pad[dy:dy + h, dx:dx + w] for dy in range(3) for dx in range(3)]
    best = np.full((h, w), 1e9, np.float32)
    for i in range(9):
        best = np.minimum(best, np.sqrt(((d - nb[i]) ** 2).sum(-1)))
        for j in range(i + 1, 9):
            best = np.minimum(best, _seg_dist(d, nb[i], nb[j]))
    return best > FAR_CHROMA


def _dilate(m):
    p = np.pad(m, 1)
    return np.any([p[dy:dy + m.shape[0], dx:dx + m.shape[1]] for dy in range(3) for dx in range(3)], 0)


def outlines(orig_rgb, rgba):
    """(original outline pixels, those of them near an outline of the remaster), h x w bool each."""
    o = orig_rgb.astype(np.int32)
    eo = np.zeros(o.shape[:2], bool)
    big = lambda a, b: np.abs(a - b).sum(-1) > 120
    eo[:, 1:] |= big(o[:, 1:], o[:, :-1])
    eo[1:, :] |= big(o[1:, :], o[:-1, :])
    g = _down(rgba) @ np.array([0.299, 0.587, 0.114], np.float32)
    gr = np.zeros_like(g)
    gr[:, 1:] = np.maximum(gr[:, 1:], np.abs(g[:, 1:] - g[:, :-1]))
    gr[1:, :] = np.maximum(gr[1:, :], np.abs(g[1:, :] - g[:-1, :]))
    er = gr > 25
    return eo, eo & _dilate(_dilate(er))


def check(orig_rgb, opaque, rgba):
    """orig_rgb: h x w x 3 original colours (as shown); opaque: h x w where the picture draws; rgba: the
    cleaned-up art. Returns (ok, reasons)."""
    bad = differs(orig_rgb, rgba) & opaque
    eo, hit = outlines(orig_rgb, rgba)
    eo &= opaque
    hit &= opaque
    h, w = bad.shape
    reasons = []
    for ty in range(0, h, TILE):
        for tx in range(0, w, TILE):
            where = 'pixels %d-%d x %d-%d' % (tx, min(tx + TILE, w) - 1, ty, min(ty + TILE, h) - 1)
            t = (slice(ty, ty + TILE), slice(tx, tx + TILE))
            n, b = int(opaque[t].sum()), int(bad[t].sum())
            if n and b >= TILE_MIN and b >= TILE_BAD * n:
                reasons.append('%s: %d of %d differ in colour' % (where, b, n))
                continue
            ne, nh = int(eo[t].sum()), int(hit[t].sum())
            if ne >= EDGE_MIN and nh < EDGE_RECALL * ne:
                reasons.append('%s: %d of %d outline pixels missing' % (where, ne - nh, ne))
    return not reasons, reasons
