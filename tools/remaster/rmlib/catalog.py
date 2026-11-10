"""tools/remaster/catalog.json: per unique picture what it is and how it is remastered."""
import json, os

from .images import read_dump

HUMAN = {'group': None, 'batch': None, 'desc': '', 'keep': '', 'sheet': None, 'status': 'new'}


def guess_group(rec):
    return 'screen' if rec['w'] >= 300 and rec['h'] >= 100 else 'sprite'


def merge(path, dumps):
    """dumps: list of (game name, dump folder). Keeps every human-edited field of existing entries."""
    cat = json.load(open(path)) if os.path.exists(path) else {}
    seen = {}
    for game, d in dumps:
        for p in read_dump(d):
            r = p.rec
            e = cat.get(p.sha1)
            if e is None:
                e = dict(HUMAN, group=guess_group(r))
                cat[p.sha1] = e
            if p.sha1 not in seen:
                seen[p.sha1] = True
                e['ids'], e['games'] = {}, []
            e.update(w=r['w'], h=r['h'], masked=r['masked'])
            e['ids'].setdefault(game, []).append(r['id'])
            if game not in e['games']:
                e['games'].append(game)
    with open(path, 'w') as f:
        json.dump(dict(sorted(cat.items())), f, indent=1)
    return cat
