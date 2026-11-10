import json
import numpy as np
from PIL import Image

from conftest import write_dump
from rmlib import jobs
from rmlib.images import EGA_PALETTE


def test_job_single_and_sheet(tmp_path):
    a = np.full((20, 30), 4); a[5:10, 5:10] = 0
    b = np.full((10, 12), 14)
    write_dump(tmp_path / 'dump', [('a' * 40, 1000, a, 1), ('b' * 40, 1001, b, 0)])
    cat = {'a' * 40: {'w': 30, 'h': 20, 'masked': 1, 'desc': 'a red box', 'keep': 'the hole', 'sheet': None},
           'b' * 40: {'w': 12, 'h': 10, 'masked': 0, 'desc': 'a yellow tile', 'keep': '', 'sheet': None}}
    out = tmp_path / 'jobs'
    j = jobs.build('a' * 40, ['a' * 40], cat, [str(tmp_path / 'dump')], {}, str(out))
    meta = json.load(open(out / ('a' * 40) / 'job.json'))
    im = np.asarray(Image.open(out / ('a' * 40) / 'input.png').convert('RGB'))
    assert im.shape[1::-1] == tuple(meta['canvas'])
    x, y, w, h = meta['cells'][0]['rect']
    assert abs(w / h - 1.5) < 0.05
    assert tuple(im[y + h // 2, x + w // 2]) == (0xAA, 0, 0)          # the picture, in its palette
    assert tuple(im[2, 2]) == jobs.KEY                                 # masked: the key colour around it
    p = open(out / ('a' * 40) / 'prompt.txt').read()
    assert 'a red box' in p and 'the hole' in p and 'pink' in p
    s = jobs.build('icons', ['a' * 40, 'b' * 40], cat, [str(tmp_path / 'dump')], {}, str(out))
    meta = json.load(open(out / 'icons' / 'job.json'))
    assert [c['sha1'] for c in meta['cells']] == ['a' * 40, 'b' * 40]
    assert '2 separate' in open(out / 'icons' / 'prompt.txt').read()
