"""Synthetic pictures for the pipeline tests: a 40 x 30 scene with a few objects in EGA colours."""
import numpy as np
from PIL import Image

from rmlib.images import palette_rgb


def scene(shift=(0, 0), recolour=None, extra_blob=False):
    idx = np.full((30, 40), 8, np.uint8)             # dark grey wall
    idx[20:, :] = 7                                   # light grey floor
    y, x = 6 + shift[1], 8 + shift[0]
    idx[y:y + 8, x:x + 6] = recolour if recolour is not None else 4    # a red box
    idx[5:9, 25:35] = 14                              # a yellow shelf item
    idx[22:27, 15:30] = 1                             # a blue car body
    if extra_blob:
        idx[10:16, 28:34] = 12                        # something added
    return idx


def as_rgb(idx):
    return palette_rgb()[idx]


def chatgpt_like(idx, out_w=600, out_h=450, inset=(30, 24), size=(540, 405)):
    """The picture drawn smoothly into a bigger canvas at an offset, as a generator returns it."""
    rgb = Image.fromarray(as_rgb(idx)).resize(size, Image.BICUBIC)
    canvas = Image.new('RGB', (out_w, out_h), (200, 200, 200))
    canvas.paste(rgb, inset)
    return np.asarray(canvas)
