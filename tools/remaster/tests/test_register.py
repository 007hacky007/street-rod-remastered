import numpy as np

from synth import as_rgb, chatgpt_like, scene
from rmlib.register import register


def test_register_recovers_rect():
    idx = scene()
    res = chatgpt_like(idx, inset=(30, 24), size=(540, 405))
    x, y, w, h, score = register(as_rgb(idx), res, guess=(0, 0, 600, 450))
    assert abs(x - 30) <= 6 and abs(y - 24) <= 6
    assert abs(w - 540) <= 10 and abs(h - 405) <= 10
    assert score > 0.5


def test_register_finds_masked_picture_on_key():
    """A masked picture that also draws its transparent colour (black) is returned on hot pink:
    the registration target must show the transparent pixels as the key colour too."""
    from types import SimpleNamespace
    from PIL import Image
    from rmlib import jobs
    idx = np.zeros((30, 40), np.uint8)               # index 0 (black) is the transparent colour
    idx[4:26, 6:34] = 1                               # a dark blue piece
    idx[4:26:2, 6:34:2] = 0                           # shaded with a black checker
    idx[10:20, 15:25] = 8
    tr = np.zeros((30, 40), bool)
    tr[:4], tr[26:], tr[:, :6], tr[:, 34:] = True, True, True, True
    p = SimpleNamespace(idx=idx, transparent=tr, masked=1)
    pal = [int('%02X%02X%02X' % tuple(c), 16) for c in as_rgb(np.arange(16))]
    shown = jobs.picture_rgb(p, pal).copy()
    shown[tr] = jobs.KEY
    big = Image.fromarray(shown).resize((540, 405), Image.BICUBIC)
    canvas = Image.new('RGB', (600, 450), jobs.KEY)
    canvas.paste(big, (30, 24))
    res = np.asarray(canvas)
    t = jobs.target_rgb(p, pal)
    assert tuple(t[0, 0]) == jobs.KEY and tuple(t[5, 7]) == (0, 0, 0xAA)
    *_, score = register(t, res, guess=(30, 24, 540, 405))
    assert score > 0.5
