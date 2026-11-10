import json, os
import numpy as np
from PIL import Image

from conftest import write_dump
from rmlib import pack
from rmlib.images import EGA_PALETTE


def test_identity_pack_is_4x_nearest(tmp_path):
    idx = np.array([[0, 1, 2], [15, 4, 0]])
    write_dump(tmp_path / 'dump', [('a' * 40, 1000, idx, 1)])
    pack.identity_pack([str(tmp_path / 'dump')], str(tmp_path / 'pack'))
    rgba = np.asarray(Image.open(tmp_path / 'pack' / 'pics' / ('a' * 40 + '.png')).convert('RGBA'))
    got_idx = np.asarray(Image.open(tmp_path / 'pack' / 'pics' / ('a' * 40 + '.idx.png')))
    assert rgba.shape == (8, 12, 4)
    for y in range(2):
        for x in range(3):
            block = rgba[y * 4:y * 4 + 4, x * 4:x * 4 + 4]
            c = EGA_PALETTE[idx[y, x]]
            assert (block[..., 0] == c >> 16).all() and (block[..., 2] == c & 255).all()
            assert (block[..., 3] == (0 if idx[y, x] == 0 else 255)).all()
            assert (got_idx[y * 4:y * 4 + 4, x * 4:x * 4 + 4] == idx[y, x]).all()
    meta = json.load(open(tmp_path / 'pack' / 'pack.json'))
    assert meta['format'] == 1
    assert meta['pictures']['a' * 40]['ref'][1] == '0000AA'


def test_marker_pack_differs_from_identity(tmp_path):
    idx = np.full((2, 2), 7)
    write_dump(tmp_path / 'dump', [('b' * 40, 1001, idx, 0)])
    pack.marker_pack([str(tmp_path / 'dump')], str(tmp_path / 'pack'))
    rgba = np.asarray(Image.open(tmp_path / 'pack' / 'pics' / ('b' * 40 + '.png')).convert('RGBA'))
    assert rgba.shape == (8, 8, 4) and (rgba[..., 3] == 255).all()
    assert not (rgba[..., :3] == [0xAA, 0xAA, 0xAA]).all(-1).all()
