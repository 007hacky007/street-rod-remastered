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


def test_splice_background_outside_the_figure():
    from rmlib.repair import splice, find_offset
    bg_idx = np.full((6, 8), 3, np.uint8)
    bg_idx[0, :] = 5                                  # a stripe and a post the frame must line up with
    bg_idx[1:, 2] = 7
    fr = np.full((4, 4), 3, np.uint8)                 # frame at (2, 0) of the background
    fr[0, :] = 5
    fr[1:, 0] = 7
    fr[1:4, 1:4] = 9                                  # the figure, a ring touching the frame's edge
    fr[2, 2] = 3                                      # background-coloured inside the figure: kept
    assert find_offset(fr, bg_idx) == (2, 0)
    bg_rgba = np.zeros((24, 32, 4), np.uint8)
    bg_rgba[..., 0] = 200
    bg_rgba[..., 3] = 255
    bg_ix = np.repeat(np.repeat(bg_idx, 4, 0), 4, 1)
    rgba = np.zeros((16, 16, 4), np.uint8)
    rgba[..., 1] = 100
    rgba[..., 3] = 255
    ix = np.repeat(np.repeat(fr, 4, 0), 4, 1)
    out, oix, n = splice(rgba, ix, fr, bg_rgba, bg_ix, bg_idx, (2, 0))
    assert out[0, 0, 0] == 200 and out[0, 0, 1] == 0          # border background from the scene
    assert out[5, 5, 1] == 100                                # figure kept
    assert out[9, 9, 1] == 100                                # enclosed background colour kept
    assert out[13, 1, 0] == 200                               # (0, 3): outside, spliced
    assert n == 7


def test_splice_only_where_allowed():
    from rmlib.repair import splice
    bg_idx = np.full((2, 2), 3, np.uint8)
    rgba = np.zeros((8, 8, 4), np.uint8)
    bg_rgba = np.full((8, 8, 4), 200, np.uint8)
    ix = np.full((8, 8), 3, np.uint8)
    allowed = np.array([[True, True], [True, False]])
    out, _, n = splice(rgba, ix, bg_idx.copy(), bg_rgba, ix, bg_idx, (0, 0), allowed)
    assert n == 3 and out[0, 0, 0] == 200 and out[7, 7, 0] == 0


def test_splice_skips_where_the_drawings_disagree():
    from rmlib.repair import splice
    bg_idx = np.full((1, 2), 3, np.uint8)
    rgba = np.full((4, 8, 4), 100, np.uint8)
    bg_rgba = np.full((4, 8, 4), 110, np.uint8)
    bg_rgba[:, 4:, :3] = (255, 120, 0)            # the scene's art drew a chair leg over pixel 1
    ix = np.full((4, 8), 3, np.uint8)
    out, _, n = splice(rgba, ix, bg_idx.copy(), bg_rgba, ix, bg_idx, (0, 0), max_diff=40)
    assert n == 1 and out[0, 0, 0] == 110 and out[0, 5, 0] == 100
