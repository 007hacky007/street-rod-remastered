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


NEAR_BLACK = 40     # a texel goes to a black reference only if no channel is above this


def assign_index(rgb, alpha, orig_idx, ref):
    """The original index per texel: the nearest reference colour among the 3 x 3 original neighbours. A
    black reference takes only texels that are near black themselves: a dark shade of a colour stays on that
    colour's index, so it repaints and fades with it."""
    h, w = orig_idx.shape
    pal = np.array([[c >> 16, c >> 8 & 255, c & 255] for c in ref], np.float32)
    black = pal.max(-1) == 0
    dark = rgb.max(-1) <= NEAR_BLACK
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
            d[black[cand] & ~dark] = 1e9
            take = d < best
            best[take] = d[take]
            idx[take] = cand[take]
    idx[alpha == 0] = _up(orig_idx)[alpha == 0]
    return idx.astype(np.uint8)


SPILL_MIN_T = 0.25      # at least this far from the original colour towards the key
SPILL_RESID = 0.25      # and this close to that line (share of the distance to the key)


def despill(rgb, orig_rgb, inside):
    """The generation's background key (hot pink) bleeding inside the outline, whole or mixed into a thin
    line: a texel on the line from its original colour to the key loses the key's share."""
    c = rgb.astype(np.float32)
    o = orig_rgb.astype(np.float32)
    d = KEY.astype(np.float32) - o
    dd = (d * d).sum(-1)
    t = np.clip((((c - o) * d).sum(-1)) / np.maximum(dd, 1), 0, 1)
    resid = c - o - t[..., None] * d
    spill = inside & (t > SPILL_MIN_T) & (np.sqrt((resid * resid).sum(-1)) < SPILL_RESID * np.sqrt(dd)) \
        & (dd > 120 * 120)
    out = rgb.copy()
    out[spill] = np.clip(c - t[..., None] * d, 0, 255)[spill].astype(np.uint8)
    return out


MERGED = 48             # a texel this far from its median-cut colour was merged into another colour's box


def quantize(rgb, ref_rgb):
    """MAX_COLOURS colours by median cut, and the picture's own reference colours: a texel the cut merged
    into a far colour (a few yellow marks among brown shades) takes the nearest reference colour instead
    when that is closer."""
    q = Image.fromarray(rgb).quantize(MAX_COLOURS, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.NONE)
    out = np.asarray(q.convert('RGB')).copy()
    if len(ref_rgb) == 0:
        return out
    f = rgb.astype(np.float32)
    d_cut = np.sqrt(((f - out) ** 2).sum(-1))
    r = ref_rgb.astype(np.float32)
    d_ref = np.sqrt(((f[..., None, :] - r) ** 2).sum(-1))
    near = d_ref.argmin(-1)
    take = (d_cut > MERGED) & (d_ref.min(-1) < d_cut)
    out[take] = ref_rgb[near[take]]
    return out


def cleanup(orig_idx, transparent, result, rect, ref):
    """orig_idx: h x w original indices; transparent: h x w (masked pictures, else all False); result: the
    generated image; rect: (x, y, w, h) of the picture in it (register); ref: 16 x 0xRRGGBB, the colours the
    art is drawn in. Returns (rgba 4h x 4w x 4, idx 4h x 4w)."""
    h, w = orig_idx.shape
    res = np.asarray(result)
    x, y, rw, rh = rect
    pad = int(np.ceil(max(0.0, -x, -y, x + rw - res.shape[1], y + rh - res.shape[0])))   # past the edge
    if pad:
        res = np.pad(res, ((pad, pad), (pad, pad), (0, 0)), mode='edge')
    im = Image.fromarray(res.astype(np.uint8))
    x, y = x + pad, y + pad
    big = np.asarray(im.resize((w * SCALE, h * SCALE), Image.LANCZOS, box=(x, y, x + rw, y + rh)))
    pal0 = np.array([[c >> 16, c >> 8 & 255, c & 255] for c in ref], np.uint8)
    rgb = despill(big[..., :3], pal0[_up(orig_idx)], _up(~transparent))
    rgb = quantize(rgb, pal0[np.unique(orig_idx[~transparent])])
    opaque = _up(~transparent).astype(np.float32)
    alpha = opaque * np.clip(_box3(opaque) * 1.5 - 0.5, 0, 1)      # soft only inside the outline
    if big.shape[2] == 4:
        alpha = np.minimum(alpha, big[..., 3] / 255.0)
    alpha = (alpha * 255 + 0.5).astype(np.uint8)
    rgb[alpha == 0] = 0
    idx = assign_index(rgb, alpha, orig_idx, ref)
    return np.dstack([rgb, alpha]), idx
