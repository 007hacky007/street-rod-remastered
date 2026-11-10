import json
import numpy as np

from conftest import write_dump
from rmlib import catalog


def test_catalog_merge_keeps_human_fields(tmp_path):
    a = np.full((2, 2), 1)
    write_dump(tmp_path / 'main', [('c' * 40, 1003, a, 1)])
    write_dump(tmp_path / 'dd', [('c' * 40, 1007, a, 1), ('d' * 40, 1008, a, 0)])
    path = tmp_path / 'catalog.json'
    catalog.merge(str(path), [('main', str(tmp_path / 'main'))])
    cat = json.load(open(path))
    cat['c' * 40]['desc'] = 'a wrench'
    cat['c' * 40]['batch'] = 1
    json.dump(cat, open(path, 'w'))
    catalog.merge(str(path), [('main', str(tmp_path / 'main')), ('datadisk', str(tmp_path / 'dd'))])
    cat = json.load(open(path))
    assert cat['c' * 40]['desc'] == 'a wrench' and cat['c' * 40]['batch'] == 1
    assert cat['c' * 40]['ids'] == {'main': [1003], 'datadisk': [1007]}
    assert cat['d' * 40]['games'] == ['datadisk'] and cat['d' * 40]['masked'] == 0
    assert set(cat['d' * 40]) >= {'w', 'h', 'group', 'batch', 'desc', 'keep', 'sheet', 'status'}
