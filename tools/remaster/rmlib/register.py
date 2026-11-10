"""Where the picture is in a generated image: the rectangle that best matches the original."""
import numpy as np
from PIL import Image


def _gray(a):
    a = a.astype(np.float32)
    return a[..., 0] * 0.299 + a[..., 1] * 0.587 + a[..., 2] * 0.114


def _edges(g):
    gx = np.zeros_like(g)
    gy = np.zeros_like(g)
    gx[:, 1:-1] = g[:, 2:] - g[:, :-2]
    gy[1:-1, :] = g[2:, :] - g[:-2, :]
    return np.hypot(gx, gy)


def _ncc(a, b):
    a = a - a.mean()
    b = b - b.mean()
    d = np.sqrt((a * a).sum() * (b * b).sum())
    return float((a * b).sum() / d) if d > 0 else 0.0


def register(orig_rgb, result, guess, max_work=96):
    """orig_rgb: h x w x 3 original colours. result: H x W x 3(4) generated image. guess: (x, y, w, h) where
    the picture should be in result. Returns (x, y, w, h, score), score = similarity in [-1, 1]."""
    h0, w0 = orig_rgb.shape[:2]
    k = max(1, int(np.ceil(max_work / max(w0, h0))))       # small originals compared at a bigger size
    k = min(k, 8)
    tw, th = w0 * k, h0 * k
    target = np.repeat(np.repeat(orig_rgb, k, 0), k, 1)
    tg = _gray(target)
    te = _edges(tg)
    res = np.asarray(result)[..., :3].astype(np.uint8)
    pad = max(res.shape[:2]) // 8                          # the picture may reach past the image's edge
    img = Image.fromarray(np.pad(res, ((pad, pad), (pad, pad), (0, 0)), mode='edge'))

    def score(r):
        x, y, w, h = r
        if w < 4 or h < 4 or x < -pad or y < -pad or x + w > res.shape[1] + pad or y + h > res.shape[0] + pad:
            return -2.0
        crop = np.asarray(img.resize((tw, th), Image.BILINEAR, box=(x + pad, y + pad, x + w + pad, y + h + pad)))
        g = _gray(crop)
        return 0.5 * _ncc(g, tg) + 0.5 * _ncc(_edges(g), te)

    best = tuple(float(v) for v in guess)
    bs = score(best)
    # coarse: uniform scale and offsets around the guess
    gx, gy, gw, gh = best
    for s in np.linspace(0.92, 1.08, 9):
        for dx in np.linspace(-0.05, 0.05, 11):
            for dy in np.linspace(-0.05, 0.05, 11):
                w, h = gw * s, gh * s
                r = (gx + (gw - w) / 2 + dx * gw, gy + (gh - h) / 2 + dy * gh, w, h)
                v = score(r)
                if v > bs:
                    best, bs = r, v
    # fine: one parameter at a time, halving the step
    step = [best[2] * 0.01, best[3] * 0.01, best[2] * 0.01, best[3] * 0.01]
    min_step = 0.25 * best[2] / w0
    while max(step) > min_step:
        improved = False
        for i in range(4):
            for sgn in (-1, 1):
                r = list(best)
                r[i] += sgn * step[i]
                v = score(tuple(r))
                if v > bs + 1e-6:
                    best, bs, improved = tuple(r), v, True
        if not improved:
            step = [s / 2 for s in step]
    x, y, w, h = best
    return x, y, w, h, bs
