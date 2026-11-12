"""tools/remaster/catalog.json: per unique picture what it is and how it is remastered."""
import json, os

from .images import read_dump

HUMAN = {'group': None, 'batch': None, 'desc': '', 'keep': '', 'sheet': None, 'status': 'new'}


def guess_group(rec):
    return 'screen' if rec['w'] >= 300 and rec['h'] >= 100 else 'sprite'


def merge(path, dumps):
    """dumps: list of (game name, dump folder). Every entry first loses the ids of the games named, then the
    dumps add theirs back: the other games' ids stay. Keeps every human-edited field of existing entries."""
    cat = json.load(open(path)) if os.path.exists(path) else {}
    named = [game for game, _ in dumps]
    for e in cat.values():
        e['ids'] = {g: v for g, v in e.get('ids', {}).items() if g not in named}
        e['games'] = [g for g in e.get('games', []) if g not in named]
    for game, d in dumps:
        for p in read_dump(d):
            r = p.rec
            e = cat.get(p.sha1)
            if e is None:
                e = dict(HUMAN, group=guess_group(r), ids={}, games=[])
                cat[p.sha1] = e
            e.update(w=r['w'], h=r['h'], masked=r['masked'])
            e['ids'].setdefault(game, []).append(r['id'])
            if game not in e['games']:
                e['games'].append(game)
    with open(path, 'w') as f:
        json.dump(dict(sorted(cat.items())), f, indent=1)
    return cat


def overlap(cat, game):
    """(entries in game, those also in another game, those only in game)."""
    mine = [e for e in cat.values() if game in e.get('games', [])]
    shared = sum(1 for e in mine if len(e['games']) > 1)
    return len(mine), shared, len(mine) - shared
