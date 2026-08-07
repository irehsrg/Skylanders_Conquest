# Authors the three tiling ground textures the lane material blends between:
#
#   Grass_C      saturated green, the off-lane base
#   Dirt_C       warm trodden earth, the path shoulder
#   Cobble_C     light stone tiles, the lane core
#
# These stand in for the authentic Imaginators set (L505_Cobblestone_01__C,
# SH_Grass01_C, SH_Dirt01_C ...), which is stuck behind the NX texture swizzle -
# see docs/ASSET-EXTRACTION.md. Same material wiring either way, so swapping the
# real ones in later is a change of inputs, not of architecture.
#
# Plain CPython + PIL, no numpy:
#   python Scripts/GenerateGroundTextures.py
#
# Everything is TILEABLE: the map samples these in world space every few hundred
# units, so any seam would repeat across the whole playfield in a visible grid.

import math
import os
import random

from PIL import Image, ImageChops, ImageDraw, ImageFilter

SIZE = 512
OUT_DIR = os.environ.get('SKY_GROUNDTEX_DIR', os.path.join(
    os.path.dirname(os.path.abspath(__file__)), '..', 'Saved', 'GroundTex'))


def tileable_noise(size, grid, seed):
    """Smooth value noise that wraps exactly.

    Interpolated by hand rather than via Image.resize: PIL's resamplers clamp at
    the image border instead of wrapping, which left a faint seam every `size`
    pixels -- and at map scale that repeats into a visible grid across the whole
    playfield. Lattice lookups here are modulo `grid`, so the field is periodic
    by construction. Smoothstep keeps it from looking like stretched bilinear.
    """
    rng = random.Random(seed)
    cells = [[rng.randrange(256) for _ in range(grid)] for _ in range(grid)]

    step = size / float(grid)
    img = Image.new('L', (size, size))
    px = img.load()
    for y in range(size):
        gy = y / step
        j0 = int(gy) % grid
        j1 = (j0 + 1) % grid
        ty = gy - int(gy)
        ty = ty * ty * (3.0 - 2.0 * ty)
        row0, row1 = cells[j0], cells[j1]
        for x in range(size):
            gx = x / step
            i0 = int(gx) % grid
            i1 = (i0 + 1) % grid
            tx = gx - int(gx)
            tx = tx * tx * (3.0 - 2.0 * tx)
            a = row0[i0] + (row0[i1] - row0[i0]) * tx
            b = row1[i0] + (row1[i1] - row1[i0]) * tx
            px[x, y] = int(a + (b - a) * ty)
    return img


def octaves(size, spec, seed):
    """Weighted sum of tileable_noise layers, normalised to 0..255."""
    total = None
    wsum = sum(w for _, w in spec)
    for i, (grid, w) in enumerate(spec):
        layer = tileable_noise(size, grid, seed + i * 977)
        scaled = layer.point(lambda v, k=w / wsum: int(v * k))
        total = scaled if total is None else ImageChops.add(total, scaled)
    return total


def colorize(gray, dark, light):
    """Map a grayscale ramp onto a two-colour gradient."""
    lut = []
    for ch in range(3):
        for v in range(256):
            t = v / 255.0
            lut.append(int(dark[ch] + (light[ch] - dark[ch]) * t))
    return gray.convert('RGB').point(lut)


def wrapped(draw_fn, size):
    """Run a drawing callback 9 times on a 3x3 lattice so strokes that cross an
    edge reappear on the opposite side, keeping the result tileable."""
    for dy in (-size, 0, size):
        for dx in (-size, 0, size):
            draw_fn(dx, dy)


def make_grass():
    # Low frequencies deliberately weak: a strong grid-6 octave gives a big
    # blotchy motif that, tiled across the map, reads as an obvious repeat.
    base = octaves(SIZE, [(6, 0.35), (17, 0.75), (46, 0.45), (120, 0.22)], seed=11)
    base = ImageChops.multiply(base, base.point(lambda v: min(255, v + 90)))
    img = colorize(base, (38, 86, 30), (146, 200, 78))

    d = ImageDraw.Draw(img)
    rng = random.Random(5)
    for _ in range(2600):
        x, y = rng.randrange(SIZE), rng.randrange(SIZE)
        ln = rng.randint(4, 11)
        lean = rng.uniform(-0.55, 0.55)
        lighter = rng.random() < 0.5
        col = ((rng.randint(150, 190), rng.randint(205, 235), rng.randint(85, 120))
               if lighter else
               (rng.randint(28, 52), rng.randint(70, 104), rng.randint(24, 44)))

        def stroke(dx, dy, x=x, y=y, ln=ln, lean=lean, col=col):
            d.line([(x + dx, y + dy),
                    (x + dx + lean * ln, y + dy - ln)], fill=col, width=1)
        wrapped(stroke, SIZE)

    return img.filter(ImageFilter.GaussianBlur(0.6))


def make_dirt():
    base = octaves(SIZE, [(5, 1.0), (14, 0.6), (40, 0.3), (110, 0.15)], seed=23)
    img = colorize(base, (82, 56, 34), (163, 126, 86))

    d = ImageDraw.Draw(img)
    rng = random.Random(7)
    for _ in range(900):                      # small pebbles / grit
        x, y = rng.randrange(SIZE), rng.randrange(SIZE)
        r = rng.randint(1, 3)
        g = rng.randint(120, 178)
        col = (g, int(g * 0.86), int(g * 0.70))

        def dot(dx, dy, x=x, y=y, r=r, col=col):
            d.ellipse([x + dx - r, y + dy - r, x + dx + r, y + dy + r], fill=col)
        wrapped(dot, SIZE)

    return img.filter(ImageFilter.GaussianBlur(0.7))


def make_cobble():
    """Large irregular flagstones with thin, mid-tone joints.

    Deliberately few and big (4x4 over the tile): at ~700 world units per tile a
    fine cobble reads as noise from the game camera. The joints are only a little
    darker than the stone -- a dark mortar turns the whole path into a dot grid.
    """
    cols, rows = 4, 4
    cw, ch = SIZE / cols, SIZE / rows
    img = Image.new('RGB', (SIZE, SIZE), (118, 118, 110))     # joint
    d = ImageDraw.Draw(img)
    rng = random.Random(31)

    for j in range(rows):
        for i in range(cols):
            # offset alternate rows so the joints don't line up into a lattice
            cx = (i + 0.5) * cw + (cw * 0.5 if j % 2 else 0) + rng.uniform(-5, 5)
            cy = (j + 0.5) * ch + rng.uniform(-5, 5)
            # ~0.52 of the cell: stones almost touch, leaving a thin joint
            rx = cw * 0.52 + rng.uniform(-4, 4)
            ry = ch * 0.52 + rng.uniform(-4, 4)
            tone = rng.randint(-22, 18)
            fill = (203 + tone, 205 + tone, 194 + tone)

            pts = []
            steps = 9                       # few sides -> angular flagstones
            jitter = rng.uniform(0, math.pi)
            for s in range(steps):
                a = 2 * math.pi * s / steps + jitter
                rr = 1.0 + rng.uniform(-0.10, 0.10)
                pts.append((math.cos(a) * rx * rr, math.sin(a) * ry * rr))

            def stone(dx, dy, cx=cx, cy=cy, pts=pts, fill=fill):
                poly = [(cx + px + dx, cy + py + dy) for px, py in pts]
                d.polygon(poly, fill=fill)
            wrapped(stone, SIZE)

    img = img.filter(ImageFilter.GaussianBlur(0.8))
    # Break up the flat stone fills; keep it gentle so joints stay readable.
    grain = octaves(SIZE, [(14, 1.0), (44, 0.55)], seed=53).point(
        lambda v: 168 + (v - 128) // 4)
    img = ImageChops.multiply(img, grain.convert('RGB'))
    return img.point(lambda v: min(255, int(v * 1.30)))


def tile_preview(img, reps=3):
    w, h = img.size
    sheet = Image.new('RGB', (w * reps, h * reps))
    for y in range(reps):
        for x in range(reps):
            sheet.paste(img, (x * w, y * h))
    return sheet.resize((w, h), Image.LANCZOS)


def main():
    if not os.path.isdir(OUT_DIR):
        os.makedirs(OUT_DIR)

    made = [('Grass_C', make_grass()), ('Dirt_C', make_dirt()),
            ('Cobble_C', make_cobble())]

    for name, img in made:
        img.save(os.path.join(OUT_DIR, name + '.png'))
        print('{:<10} -> {}.png'.format(name, name))

    # 3x3 of each, so any seam shows up immediately as a grid line.
    sheet = Image.new('RGB', (SIZE * len(made), SIZE))
    for i, (_, img) in enumerate(made):
        sheet.paste(tile_preview(img), (i * SIZE, 0))
    sheet.save(os.path.join(OUT_DIR, '_tiling_check.png'))
    print('tiling check -> _tiling_check.png')


if __name__ == '__main__':
    main()
