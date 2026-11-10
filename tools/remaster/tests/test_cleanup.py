import numpy as np

from synth import as_rgb, chatgpt_like, scene
from rmlib.cleanup import cleanup
from rmlib.images import EGA_PALETTE


def test_cleanup_exact_size_alpha_and_idx():
    idx = scene()
    transparent = np.zeros(idx.shape, bool)
    transparent[:, :4] = True                         # a masked strip on the left
    res = chatgpt_like(idx, inset=(30, 24), size=(540, 405))
    rgba, ix = cleanup(idx, transparent, res, (30, 24, 540, 405), EGA_PALETTE)
    assert rgba.shape == (120, 160, 4) and ix.shape == (120, 160)
    assert (rgba[:, :16, 3] == 0).all()               # nothing outside the outline
    assert (rgba[40:60, 60:80, 3] == 255).all()
    assert (ix[30:50, 36:52] == 4).mean() > 0.9       # the red box keeps its index
    assert len(np.unique(rgba[..., :3].reshape(-1, 3), axis=0)) <= 64


def test_cleanup_replaces_key_bleed():
    idx = np.full((10, 10), 2, np.uint8)
    res = np.zeros((40, 40, 3), np.uint8)
    res[...] = (0, 170, 0)
    res[30:, 20:] = (255, 0, 128)                 # the pink key inside the outline
    rgba, ix = cleanup(idx, np.zeros(idx.shape, bool), res, (0, 0, 40, 40), EGA_PALETTE)
    assert tuple(rgba[35, 35, :3]) == (0, 0xAA, 0)
