#!/usr/bin/env python3
"""Street Rod Enhanced remaster pipeline. See tools/remaster/README.md."""
import argparse, os, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from rmlib import catalog, pack  # noqa: E402

HERE = os.path.dirname(os.path.abspath(__file__))


def main():
    ap = argparse.ArgumentParser(prog='rm.py')
    sub = ap.add_subparsers(dest='cmd', required=True)
    c = sub.add_parser('catalog', help='merge dumps (GAME=DIR ...) into catalog.json')
    c.add_argument('dumps', nargs='+')
    for name in ('identity-pack', 'marker-pack'):
        s = sub.add_parser(name)
        s.add_argument('out')
        s.add_argument('dumps', nargs='+')
    i = sub.add_parser('install')
    i.add_argument('pack')
    i.add_argument('dest')
    a = ap.parse_args()
    if a.cmd == 'catalog':
        cat = catalog.merge(os.path.join(HERE, 'catalog.json'), [tuple(d.split('=', 1)) for d in a.dumps])
        print(len(cat), 'pictures')
    elif a.cmd == 'identity-pack':
        print(pack.identity_pack(a.dumps, a.out), 'pictures')
    elif a.cmd == 'marker-pack':
        print(pack.marker_pack(a.dumps, a.out), 'pictures')
    elif a.cmd == 'install':
        pack.install(a.pack, a.dest)


if __name__ == '__main__':
    main()
