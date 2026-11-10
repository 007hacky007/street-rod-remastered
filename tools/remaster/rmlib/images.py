"""Reading the engine's picture dump (streetrod-enhanced --dump-pictures)."""
import json, os
import numpy as np
from PIL import Image

# The engine's ega_palette (src/platform/ega.c), index order.
EGA_PALETTE = [0x000000, 0x0000AA, 0x00AA00, 0x00AAAA, 0xAA0000, 0xAA00AA, 0xAA5500, 0xAAAAAA,
               0x555555, 0x5555FF, 0x55FF55, 0x55FFFF, 0xFF5555, 0xFF55FF, 0xFFFF55, 0xFFFFFF]


class Picture:
    def __init__(self, dump, rec):
        self.dump = dump
        self.rec = rec
        self.sha1 = rec['sha1']
        self.w, self.h, self.masked = rec['w'], rec['h'], rec['masked']

    @property
    def idx(self):
        return np.asarray(Image.open(os.path.join(self.dump, self.sha1 + '.bmp')))

    @property
    def transparent(self):
        """True where the game draws nothing (masked pictures only)."""
        if not self.masked:
            return np.zeros((self.h, self.w), bool)
        return np.asarray(Image.open(os.path.join(self.dump, self.sha1 + '.mask.bmp'))) > 127


def read_dump(dump):
    """The pictures of a dump folder, then its race-view shapes (shapes.jsonl, kind "shape")."""
    out = []
    for name in ('pictures.jsonl', 'shapes.jsonl'):
        path = os.path.join(dump, name)
        if os.path.exists(path):
            with open(path) as f:
                out += [Picture(dump, json.loads(line)) for line in f if line.strip()]
    return out


def unique_pictures(dumps):
    """sha1 -> Picture (first dump wins) over several dump folders."""
    out = {}
    for d in dumps:
        for p in read_dump(d):
            out.setdefault(p.sha1, p)
    return out


def palette_rgb(pal=EGA_PALETTE):
    return np.array([[c >> 16, c >> 8 & 255, c & 255] for c in pal], np.uint8)
