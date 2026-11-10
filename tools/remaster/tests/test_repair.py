import numpy as np

from rmlib.repair import hue_lock, tiles_of


def test_hue_lock_restores_hue_keeps_shading():
    rgba = np.zeros((8, 8, 4), np.uint8)
    rgba[..., 3] = 255
    rgba[:, :4, :3] = (200, 120, 40)            # orange, should be yellow
    rgba[:, 4:, :3] = (120, 70, 20)             # darker orange
    idx = np.full((8, 8), 14, np.uint8)
    ref = [0] * 16
    ref[14] = 0xFFFF55
    out, _ = hue_lock(rgba, idx, ref, [(0, 1, 0, 1)])
    a, b = out[0, 0, :3].astype(int), out[0, 6, :3].astype(int)
    assert a[0] > 100 and abs(a[0] - a[1]) < 10 and a[2] < a[0]   # yellow
    assert b.sum() < a.sum()                                      # still the darker one


def test_tiles_of_parses_reasons():
    r = ['47ceed57: pixels 64-79 x 96-111: 52 of 256 differ in colour', 'x: pixels 1-2 x 3-4: 5 of 6 outline pixels missing']
    assert tiles_of(r) == [(64, 79, 96, 111)]


def test_hue_lock_uses_original_indices():
    rgba = np.zeros((8, 8, 4), np.uint8)
    rgba[..., 3] = 255
    rgba[..., :3] = (170, 85, 0)                 # drifted to brown
    idx = np.full((8, 8), 6, np.uint8)           # and so indexed as brown
    ref = [0] * 16
    ref[6], ref[14] = 0xAA5500, 0xFFFF55
    orig = np.full((2, 2), 14, np.uint8)         # the original is yellow
    out, ix = hue_lock(rgba, idx, ref, [(0, 1, 0, 1)], orig)
    assert (ix == 14).all()
    r, g, b = out[0, 0, :3].astype(int)
    assert abs(r - g) < 10 and b < r
