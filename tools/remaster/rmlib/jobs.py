"""A generation job: input.png (the original in its in-game colours, enlarged with hard pixels on a canvas
ChatGPT can produce), prompt.txt and job.json (where each picture sits on the canvas)."""
import collections, json, os
import numpy as np
from PIL import Image

from .images import EGA_PALETTE, unique_pictures
from .sheet import layout


def group(cat, batch):
    """Jobs of a batch: name -> sha1s, one per sheet name, else one per picture; pictures kept original skipped."""
    out = collections.OrderedDict()
    for s, e in cat.items():
        if e.get('batch') == batch and e.get('status') != 'original':
            out.setdefault(e.get('sheet') or s, []).append(s)
    return out

KEY = (255, 0, 128)          # hot pink: far from every EGA colour
MARGIN = (128, 128, 128)
CANVASES = [(1536, 1024), (1024, 1024), (1024, 1536)]
HERE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def style():
    return open(os.path.join(HERE, 'style.md')).read().strip()


def canvas_for(w, h):
    a = w / h
    return min(CANVASES, key=lambda c: abs(np.log((c[0] / c[1]) / a)))


def palette_of(sha1, palettes):
    return palettes.get(sha1) or palettes.get('default') or EGA_PALETTE


def picture_rgb(p, pal):
    P = np.array([[c >> 16, c >> 8 & 255, c & 255] for c in pal], np.uint8)
    return P[p.idx]


def target_rgb(p, pal):
    """The picture as the job's input shows it: transparent pixels in the key colour."""
    rgb = picture_rgb(p, pal)
    if p.masked:
        rgb = rgb.copy()
        rgb[p.transparent] = KEY
    return rgb


def build(name, sha1s, catalog, dumps, palettes, out):
    pics = unique_pictures(dumps)
    d = os.path.join(out, name)
    os.makedirs(d, exist_ok=True)
    masked = any(catalog[s].get('masked') for s in sha1s)
    if len(sha1s) == 1:
        s = sha1s[0]
        w, h = catalog[s]['w'], catalog[s]['h']
        cw, ch = canvas_for(w, h)
        k = max(1, int(min(cw * 0.88 / w, ch * 0.88 / h)))
        rects = [((cw - w * k) // 2, (ch - h * k) // 2, w * k, h * k)]
    else:
        (cw, ch), rects = layout([(catalog[s]['w'], catalog[s]['h']) for s in sha1s])
    canvas = Image.new('RGB', (cw, ch), KEY if masked else MARGIN)
    cells = []
    for s, (x, y, w, h) in zip(sha1s, rects):
        p = pics[s]
        rgb = picture_rgb(p, palette_of(s, palettes))
        im = Image.fromarray(rgb).resize((w, h), Image.NEAREST)
        if p.masked:
            m = Image.fromarray((~p.transparent * 255).astype(np.uint8)).resize((w, h), Image.NEAREST)
            canvas.paste(im, (x, y), m)
        else:
            canvas.paste(im, (x, y))
        cells.append({'sha1': s, 'rect': [x, y, w, h]})
    canvas.save(os.path.join(d, 'input.png'))
    json.dump({'name': name, 'canvas': [cw, ch], 'cells': cells}, open(os.path.join(d, 'job.json'), 'w'), indent=1)
    open(os.path.join(d, 'prompt.txt'), 'w').write(prompt(sha1s, catalog, masked))
    return d


def prompt(sha1s, catalog, masked):
    parts = ['Create an image from the attached picture.', style()]
    if len(sha1s) == 1:
        e = catalog[sha1s[0]]
        if e.get('desc'):
            parts.append('The picture shows: %s.' % e['desc'].rstrip('.'))
        if e.get('keep'):
            parts.append('Must stay exactly as it is: %s.' % e['keep'].rstrip('.'))
    else:
        parts.append('The attached image holds %d separate small pictures. Redraw each one where it is, at the same '
                     'position and size; do not merge, move or connect them.' % len(sha1s))
        said = {(catalog[s].get('desc'), catalog[s].get('keep')) for s in sha1s}
        if len(said) == 1 and catalog[sha1s[0]].get('desc'):
            e = catalog[sha1s[0]]
            parts.append('Each picture: %s%s.' % (e['desc'].rstrip('.'),
                                                  ('; keep ' + e['keep'].rstrip('.')) if e.get('keep') else ''))
        for i, s in enumerate(sha1s if len(said) > 1 else [], 1):
            e = catalog[s]
            if e.get('desc'):
                parts.append('Picture %d: %s%s.' % (i, e['desc'].rstrip('.'),
                                                    ('; keep ' + e['keep'].rstrip('.')) if e.get('keep') else ''))
    if masked:
        parts.append('Everything stands on a flat hot pink background (#FF0080). Keep that background flat hot pink, '
                     'with no shadows, gradients or anything else on it: it is cut away afterwards.')
    parts.append('Same framing as the attached image, landscape or portrait as it is. No borders, no extra objects.')
    return '\n\n'.join(parts) + '\n'
