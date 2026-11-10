"""Several small pictures on one generation canvas: one cell each, the same enlargement for all."""


def layout(sizes, canvas=(1536, 1024), gap_frac=0.04):
    """sizes: [(w, h)] originals. Returns (canvas, cells) with cells [(x, y, w, h)] in input order, the
    largest scale at which shelves of cells fit with gaps between them and around the edge."""
    cw, ch = canvas
    gap = int(min(cw, ch) * gap_frac)
    order = sorted(range(len(sizes)), key=lambda i: -sizes[i][1])

    def place(s):
        cells = [None] * len(sizes)
        x, y, row = gap, gap, 0
        for i in order:
            w, h = int(sizes[i][0] * s), int(sizes[i][1] * s)
            if x + w + gap > cw:
                x, y, row = gap, y + row + gap, 0
            if y + h + gap > ch or x + w + gap > cw:
                return None
            cells[i] = (x, y, w, h)
            x += w + gap
            row = max(row, h)
        return cells

    lo, hi = 0.5, 200.0
    best = place(lo)
    for _ in range(40):
        mid = (lo + hi) / 2
        c = place(mid)
        if c:
            lo, best = mid, c
        else:
            hi = mid
    return canvas, best
