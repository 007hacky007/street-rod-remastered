from rmlib.progress import Progress


def test_progress_next_and_tries(tmp_path):
    p = Progress(str(tmp_path / 'progress.json'))
    p.add('garage', ['a' * 40])
    p.add('icons-1', ['b' * 40, 'c' * 40])
    assert p.next() == 'garage'
    p.result('garage', ok=False, reasons=['tile (3, 4)'])
    assert p.next() == 'garage' and p.jobs['garage']['tries'] == 1
    p.result('garage', ok=False, reasons=['x'])
    p.result('garage', ok=False, reasons=['y'])
    assert p.jobs['garage']['status'] == 'attention'
    assert p.next() == 'icons-1'
    p.result('icons-1', ok=True)
    p2 = Progress(str(tmp_path / 'progress.json'))
    assert p2.jobs['icons-1']['status'] == 'done' and p2.next() is None


def test_settle_keeps_tries_and_retry_gives_one_more(tmp_path):
    p = Progress(str(tmp_path / 'progress.json'))
    p.add('garage', ['a' * 40])
    p.result('garage', ok=False, reasons=['x'])
    p.settle('garage', ok=False, reasons=['still'])   # a repair that did not help: not a try
    assert p.jobs['garage']['status'] == 'failed' and p.jobs['garage']['tries'] == 1
    p.result('garage', ok=False); p.result('garage', ok=False)
    assert p.jobs['garage']['status'] == 'attention'
    p.settle('garage', ok=False)
    assert p.jobs['garage']['status'] == 'attention'
    p.retry('garage')
    assert p.next() == 'garage' and p.jobs['garage']['status'] == 'failed'
    p.result('garage', ok=False)
    assert p.jobs['garage']['status'] == 'attention'
    p.settle('garage', ok=True)
    assert p.jobs['garage']['status'] == 'done'
