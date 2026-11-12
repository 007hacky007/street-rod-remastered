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


def third_source(tmp_path):
    a = np.full((2, 2), 1)
    write_dump(tmp_path / 'main', [('c' * 40, 1003, a, 1)])
    write_dump(tmp_path / 'sr2', [('c' * 40, 1050, a, 1), ('e' * 40, 2000, a, 0)])
    path = tmp_path / 'catalog.json'
    catalog.merge(str(path), [('main', str(tmp_path / 'main'))])
    cat = json.load(open(path))
    cat['c' * 40]['desc'] = 'a wrench'
    cat['c' * 40]['pal_like'] = 1001
    json.dump(cat, open(path, 'w'))
    return path, catalog.merge(str(path), [('main', str(tmp_path / 'main')), ('sr2', str(tmp_path / 'sr2'))])


def test_catalog_merge_third_source(tmp_path):
    path, _ = third_source(tmp_path)
    cat = json.load(open(path))
    assert cat['c' * 40]['ids'] == {'main': [1003], 'sr2': [1050]}
    assert cat['c' * 40]['games'] == ['main', 'sr2']
    assert cat['e' * 40]['games'] == ['sr2']
    assert cat['c' * 40]['desc'] == 'a wrench' and cat['c' * 40]['pal_like'] == 1001


def test_catalog_merge_one_game_keeps_the_others(tmp_path):
    a = np.full((2, 2), 1)
    write_dump(tmp_path / 'main', [('c' * 40, 1003, a, 1)])
    write_dump(tmp_path / 'dd', [('c' * 40, 1007, a, 1), ('d' * 40, 1008, a, 0)])
    write_dump(tmp_path / 'sr2', [('c' * 40, 1050, a, 1), ('d' * 40, 1060, a, 0)])
    path = tmp_path / 'catalog.json'
    catalog.merge(str(path), [('main', str(tmp_path / 'main')), ('datadisk', str(tmp_path / 'dd'))])
    catalog.merge(str(path), [('sr2', str(tmp_path / 'sr2'))])
    cat = json.load(open(path))
    assert cat['c' * 40]['ids'] == {'main': [1003], 'datadisk': [1007], 'sr2': [1050]}
    assert cat['c' * 40]['games'] == ['main', 'datadisk', 'sr2']
    assert cat['d' * 40]['ids'] == {'datadisk': [1008], 'sr2': [1060]}
    assert cat['d' * 40]['games'] == ['datadisk', 'sr2']


def test_catalog_overlap_counts(tmp_path):
    _, cat = third_source(tmp_path)
    assert catalog.overlap(cat, 'sr2') == (2, 1, 1)
    assert catalog.overlap(cat, 'main') == (1, 1, 0)
