from rmlib.palettes import merge


def test_dark_palettes_are_skipped_and_replaced():
    black = ['000000'] * 16
    garage = ['000000', '0000AA'] + ['AAAAAA'] * 14
    out = merge({'a': black, 'b': garage}, [{'sha1': 'a', 'pal': black}, {'sha1': 'a', 'pal': garage},
                                             {'sha1': 'c', 'pal': black}])
    assert out['a'] == garage                    # the logged fade (black) is replaced by the real one
    assert 'c' not in out                        # only ever seen black: unknown
    assert out['b'] == garage


def test_first_real_palette_wins():
    p1 = ['000000'] + ['555555'] * 15
    p2 = ['000000'] + ['FFFFFF'] * 15
    out = merge({}, [{'sha1': 'a', 'pal': p1}, {'sha1': 'a', 'pal': p2}])
    assert out['a'] == p1
