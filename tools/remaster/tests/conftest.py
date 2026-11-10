import json, os, sys
import numpy as np
from PIL import Image

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..'))

from rmlib.images import EGA_PALETTE


def write_dump(d, pictures):
    """pictures: list of (sha1, id, idx array, masked) -> a dump folder like --dump-pictures writes."""
    os.makedirs(d, exist_ok=True)
    with open(os.path.join(d, 'pictures.jsonl'), 'w') as f:
        for sha1, pid, idx, masked in pictures:
            h, w = idx.shape
            im = Image.fromarray(idx.astype(np.uint8), 'P')
            pal = []
            for c in EGA_PALETTE:
                pal += [c >> 16, c >> 8 & 255, c & 255]
            im.putpalette(pal + [0] * (768 - len(pal)))
            im.save(os.path.join(d, sha1 + '.bmp'))
            if masked:
                Image.fromarray(np.where(idx == 0, 255, 0).astype(np.uint8), 'L').save(os.path.join(d, sha1 + '.mask.bmp'))
            f.write(json.dumps({'sha1': sha1, 'lib': 2, 'index': pid - 1000, 'id': pid, 'w': w, 'h': h,
                                'masked': masked}, separators=(',', ':')) + '\n')
