import numpy as np

from synth import as_rgb, chatgpt_like, scene
from rmlib.check import check
from rmlib.cleanup import cleanup
from rmlib.images import EGA_PALETTE


def art_of(idx):
    res = chatgpt_like(idx, inset=(30, 24), size=(540, 405))
    return cleanup(scene(), np.zeros((30, 40), bool), res, (30, 24, 540, 405), EGA_PALETTE)[0]


def run(idx):
    orig = scene()
    return check(as_rgb(orig), np.ones(orig.shape, bool), art_of(idx))


def test_faithful_passes():
    ok, reasons = run(scene())
    assert ok, reasons


def test_moved_object_rejected():
    ok, reasons = run(scene(shift=(2, 2)))
    assert not ok


def test_recoloured_object_rejected():
    ok, reasons = run(scene(recolour=2))
    assert not ok


def test_added_blob_rejected():
    ok, reasons = run(scene(extra_blob=True))
    assert not ok
