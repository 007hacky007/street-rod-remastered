import importlib
import os

import rm


def test_art_folder_from_env(tmp_path, monkeypatch):
    monkeypatch.setenv('SRE_REMASTER_ART', str(tmp_path))
    try:
        importlib.reload(rm)
        assert rm.ART == str(tmp_path)
        assert rm.DUMPS[2] == str(tmp_path / 'dump' / 'sr2')
    finally:
        monkeypatch.delenv('SRE_REMASTER_ART')
        importlib.reload(rm)
    assert rm.ART == os.path.join(rm.REPO, 'remaster-art')


def test_raw_path_never_overwrites(tmp_path):
    """A kept result gets the next free number, even after a job's tries were reset."""
    d = tmp_path / 'raw'
    assert rm.next_raw_path(str(d), 'job') == str(d / 'job.1.png')
    d.mkdir()
    (d / 'job.1.png').write_bytes(b'x'); (d / 'job.3.png').write_bytes(b'x'); (d / 'other.7.png').write_bytes(b'x')
    assert rm.next_raw_path(str(d), 'job') == str(d / 'job.4.png')


def test_guesses_take_a_cropped_result():
    """The picture's place in the result: its cell scaled from the job's canvas, and for a one-picture job
    returned cropped to the picture (no canvas, the picture's shape) the whole result too."""
    canvas, rect = (1536, 1024), (168, 334, 1200, 356)          # a 150 x 45 picture at 8x, centred
    g = rm.place_guesses(rect, canvas, (1024, 1536), 1, (150, 45))
    assert g == [(168.0, 334.0, 1200.0, 356.0)]
    g = rm.place_guesses(rect, canvas, (724, 2172), 1, (150, 45))
    x, y, w, h = g[-1]                                          # the picture's shape, centred, as big as fits
    assert len(g) == 2 and x == 0 and w == 2172 and abs(w / h - 150 / 45) < 1e-6 and abs(y - (724 - h) / 2) < 1e-6
    assert len(rm.place_guesses(rect, canvas, (724, 2172), 1, (320, 90))) == 2   # 3.0 against 3.56: extra edges
    assert len(rm.place_guesses(rect, canvas, (724, 2172), 2, (150, 45))) == 1    # a sheet: never
    assert len(rm.place_guesses(rect, canvas, (1024, 1024), 1, (150, 45))) == 1   # another shape: never


def test_cropped_result_registers():
    from synth import as_rgb, chatgpt_like, scene
    from rmlib.register import register
    idx = scene()                                               # 40 x 30
    res = chatgpt_like(idx, out_w=800, out_h=600, inset=(0, 0), size=(800, 600))
    g = rm.place_guesses((300, 200, 400, 300), (1000, 1000), res.shape[:2], 1, (40, 30))
    best = max((register(as_rgb(idx), res, r) for r in g), key=lambda r: r[4])
    assert best[4] > 0.5 and abs(best[2] - 800) < 20
