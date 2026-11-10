import numpy as np
from PIL import Image

from rmlib.sheet import layout


def test_sheet_round_trip():
    sizes = [(24, 17), (32, 29), (8, 11), (40, 28)]
    canvas, cells = layout(sizes, canvas=(1536, 1024))
    assert len(cells) == 4
    for (w, h), (x, y, cw, ch) in zip(sizes, cells):
        assert 0 <= x and x + cw <= canvas[0] and 0 <= y and y + ch <= canvas[1]
        assert abs(cw / ch - w / h) < 0.05           # aspect kept
    for i, a in enumerate(cells):                     # no overlap
        for b in cells[i + 1:]:
            assert a[0] + a[2] <= b[0] or b[0] + b[2] <= a[0] or a[1] + a[3] <= b[1] or b[1] + b[3] <= a[1]
