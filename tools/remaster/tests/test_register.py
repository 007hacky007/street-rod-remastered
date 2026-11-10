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
