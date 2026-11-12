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
