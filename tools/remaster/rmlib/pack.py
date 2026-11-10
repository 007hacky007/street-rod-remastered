"""The remaster art pack: pics/<sha1>.png (RGBA, 4x), pics/<sha1>.idx.png (original index per texel),
pack.json ({"format": 1, "pictures": {sha1: {"ref": [16 x "RRGGBB"]}}, "shapes": {}})."""
import hashlib, json, os, shutil
import numpy as np
from PIL import Image

from .images import EGA_PALETTE, palette_rgb, unique_pictures

SCALE = 4


def write_entry(out, sha1, rgba, idx, ref):
    os.makedirs(os.path.join(out, 'pics'), exist_ok=True)
    Image.fromarray(rgba.astype(np.uint8), 'RGBA').save(os.path.join(out, 'pics', sha1 + '.png'))
    Image.fromarray(idx.astype(np.uint8), 'L').save(os.path.join(out, 'pics', sha1 + '.idx.png'))


def write_meta(out, refs, shapes=None):
    meta = {'format': 1,
            'pictures': {s: {'ref': ['%06X' % c for c in ref]} for s, ref in sorted(refs.items())},
            'shapes': shapes or {}}
    with open(os.path.join(out, 'pack.json'), 'w') as f:
        json.dump(meta, f, indent=1)


def upscale(a):
    return np.repeat(np.repeat(a, SCALE, 0), SCALE, 1)


def identity_art(p):
    """The original picture at 4x, nearest neighbour: (rgba, idx)."""
    idx = upscale(p.idx)
    rgb = palette_rgb()[idx]
    alpha = np.where(upscale(p.transparent), 0, 255).astype(np.uint8)
    return np.dstack([rgb, alpha]), idx


def identity_pack(dumps, out):
    refs = {}
    for sha1, p in unique_pictures(dumps).items():
        rgba, idx = identity_art(p)
        write_entry(out, sha1, rgba, idx, EGA_PALETTE)
        refs[sha1] = EGA_PALETTE
    write_meta(out, refs)
    return len(refs)


def marker_pack(dumps, out):
    """Each picture tinted with a colour from its sha1 and a 1-texel checker: shows coverage and seams."""
    refs = {}
    for sha1, p in unique_pictures(dumps).items():
        rgba, idx = identity_art(p)
        h = hashlib.sha1(sha1.encode()).digest()
        tint = np.array([h[0], h[1], h[2]], np.float32)
        rgb = rgba[..., :3].astype(np.float32) * 0.5 + tint * 0.5
        yy, xx = np.mgrid[0:rgb.shape[0], 0:rgb.shape[1]]
        rgb[(xx + yy) % 2 == 1] *= 0.8
        rgba = np.dstack([rgb.clip(0, 255).astype(np.uint8), rgba[..., 3]])
        write_entry(out, sha1, rgba, idx, EGA_PALETTE)
        refs[sha1] = EGA_PALETTE
    write_meta(out, refs)
    return len(refs)


def install(pack, dest):
    if os.path.isdir(dest):
        shutil.rmtree(dest)
    shutil.copytree(pack, dest)
