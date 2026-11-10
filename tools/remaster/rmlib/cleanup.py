"""A generated picture onto the art pack's grid: exactly 4x the original, alpha within the original's
outline, a pixel-art palette and the original index of every texel."""
import numpy as np
from PIL import Image

SCALE = 4
MAX_COLOURS = 64
KEY = np.array([255, 0, 128])   # rmlib.jobs.KEY


def _up(a):
    return np.repeat(np.repeat(a, SCALE, 0), SCALE, 1)


def _box3(a):
    p = np.pad(a, 1, mode='edge')
    return sum(p[dy:dy + a.shape[0], dx:dx + a.shape[1]] for dy in range(3) for dx in range(3)) / 9.0


def cleanup(orig_idx, transparent, result, rect, ref):
    """orig_idx: h x w original indices; transparent: h x w (masked pictures, else all False); result: the
    generated image; rect: (x, y, w, h) of the picture in it (register); ref: 16 x 0xRRGGBB, the colours the
    art is drawn in. Returns (rgba 4h x 4w x 4, idx 4h x 4w)."""
    h, w = orig_idx.shape
    res = np.asarray(result)
    x, y, rw, rh = rect
    im = Image.fromarray(res.astype(np.uint8))
    big = np.asarray(im.resize((w * SCALE, h * SCALE), Image.LANCZOS, box=(x, y, x + rw, y + rh)))
    rgb = big[..., :3]
    q = Image.fromarray(rgb).quantize(MAX_COLOURS, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.NONE)
    rgb = np.asarray(q.convert('RGB')).copy()
    # the generation's background key (hot pink) bleeding inside the outline: the original colour there
    pal0 = np.array([[c >> 16, c >> 8 & 255, c & 255] for c in ref], np.uint8)
    bleed = (np.abs(rgb.astype(np.int32) - KEY).sum(-1) < 120) & _up(~transparent)
    rgb[bleed] = pal0[_up(orig_idx)][bleed]
    opaque = _up(~transparent).astype(np.float32)
    alpha = opaque * np.clip(_box3(opaque) * 1.5 - 0.5, 0, 1)      # soft only inside the outline
    if big.shape[2] == 4:
        alpha = np.minimum(alpha, big[..., 3] / 255.0)
    alpha = (alpha * 255 + 0.5).astype(np.uint8)
    rgb[alpha == 0] = 0
    # the original index per texel: the nearest reference colour among the 3 x 3 original neighbours
    pal = np.array([[c >> 16, c >> 8 & 255, c & 255] for c in ref], np.float32)
    pad = np.pad(orig_idx, 1, mode='edge')
    best = np.full((h * SCALE, w * SCALE), 1e9, np.float32)
    idx = _up(orig_idx).copy()
    f = rgb.astype(np.float32)
    for dy in range(3):
        for dx in range(3):
            cand = _up(pad[dy:dy + h, dx:dx + w])
            d = ((f - pal[cand]) ** 2).sum(-1)
            if dy == 1 and dx == 1:
                d -= 1.0                                   # ties go to the pixel's own index
            take = d < best
            best[take] = d[take]
            idx[take] = cand[take]
    idx[alpha == 0] = _up(orig_idx)[alpha == 0]
    return np.dstack([rgb, alpha]), idx.astype(np.uint8)
