# Bakes a top-down mask of the LANE corridor for the ground material.
#
# The map's ground is one flat plane, so "the lane is a clear path" has to come
# from the material rather than geometry. Computing distance-to-lane in HLSL for
# a 6-segment polyline is fiddly and hard to art-direct; baking it to a texture
# is simpler and lets the edge wobble.
#
# White = trodden lane path, black = off-lane ground. The material lerps its two
# surface textures with this and darkens the rim so the path reads crisply.
#
# Plain CPython (no numpy here) -- all raster work is PIL ops, no pixel loops:
#   python Scripts/GenerateLaneMask.py
#
# !! Keep LANE_PATH / MAP_* in sync with ASkylandersMapBuilder::BuildMap.
#    They are duplicated deliberately: this runs outside the engine.

import os

from PIL import Image, ImageChops, ImageDraw, ImageFilter

OUT_DIR = os.environ.get('SKY_LANEMASK_DIR', os.path.join(
    os.path.dirname(os.path.abspath(__file__)), '..', 'Saved', 'LaneMask'))
OUT = os.path.join(OUT_DIR, 'LaneMask.png')

# Ground plane is a 100uu cube scaled (270, 105) => 27000 x 10500, centred.
MAP_W, MAP_H = 27000.0, 10500.0
RES_X, RES_Y = 2048, 800

# Mirror of LanePath in BuildMap (blue -X -> red +X).
LANE_PATH = [(-8300, 0), (-6100, 1000), (-3400, -650), (0, 0),
             (3400, -650), (6100, 1000), (8300, 0)]
BASE_MOUTHS = [(-10000, 0), (10000, 0)]

# The walkable corridor is 820 half-width; the *visible* path is deliberately
# narrower so there is off-path shoulder either side -- that margin is what makes
# it read as a path rather than as the edge of the playfield.
PATH_HALF_WIDTH = 620.0
BASE_RADIUS = 1500.0

EDGE_SOFTNESS = 26.0   # px of blur before thresholding
# Two octaves of value noise added to the mask before the ramp. Generated small
# and upscaled rather than blurred down from full res: blurring high-frequency
# noise flattens its amplitude to nothing, which is how the first attempt ended
# up with a suspiciously clean ribbon.
WOBBLE_OCTAVES = ((26, 1.0), (13, 0.4))   # (downscale factor, weight)
EDGE_WOBBLE = 190.0                       # peak edge displacement, in mask levels


def to_px(x, y):
    """World XY -> pixel. +Y world is up the image, so V is flipped."""
    return ((x + MAP_W * 0.5) / MAP_W * RES_X,
            (MAP_H * 0.5 - y) / MAP_H * RES_Y)


def main():
    if not os.path.isdir(OUT_DIR):
        os.makedirs(OUT_DIR)

    # Non-uniform world->px scale; use the X scale for widths (the map is drawn
    # with the same units on both axes, X is the dominant direction of travel).
    px_per_uu = RES_X / MAP_W

    hard = Image.new('L', (RES_X, RES_Y), 0)
    draw = ImageDraw.Draw(hard)

    pts = [to_px(x, y) for x, y in LANE_PATH]
    width_px = max(1, int(round(PATH_HALF_WIDTH * 2 * px_per_uu)))
    draw.line(pts, fill=255, width=width_px, joint='curve')
    # joint='curve' rounds interior joints but not the ends; cap them manually
    # so the lane does not finish in a blunt rectangle.
    r = width_px / 2.0
    for cx, cy in (pts[0], pts[-1]):
        draw.ellipse([cx - r, cy - r, cx + r, cy + r], fill=255)

    for wx, wy in BASE_MOUTHS:
        cx, cy = to_px(wx, wy)
        rb = BASE_RADIUS * px_per_uu
        draw.ellipse([cx - rb, cy - rb, cx + rb, cy + rb], fill=255)

    soft = hard.filter(ImageFilter.GaussianBlur(EDGE_SOFTNESS))

    # Low-frequency noise displaces the 50% contour so the edge meanders.
    # It is weighted by 4s(1-s), which peaks in the blurred transition band and
    # falls to zero in the solid interior and the far field -- without that
    # weighting the noise also fires deep in the black and speckles the whole
    # plane with false path pixels.
    s = soft.point(lambda v: int(255.0 * 4.0 * (v / 255.0) * (1.0 - v / 255.0)))

    combined = soft
    total_weight = sum(w for _, w in WOBBLE_OCTAVES)
    for factor, weight in WOBBLE_OCTAVES:
        small = Image.effect_noise(
            (max(2, RES_X // factor), max(2, RES_Y // factor)), 90)
        octave = small.resize((RES_X, RES_Y), Image.BICUBIC)
        amp = (EDGE_WOBBLE / 255.0) * (weight / total_weight)
        pos = octave.point(lambda v, a=amp: int(max(0, v - 128) * a))
        neg = octave.point(lambda v, a=amp: int(max(0, 128 - v) * a))
        combined = ImageChops.add(combined, ImageChops.multiply(pos, s))
        combined = ImageChops.subtract(combined, ImageChops.multiply(neg, s))

    # Ramp the soft mask into a path with a short gradient at its edge.
    lo, hi = 96, 168
    combined = combined.point(
        lambda v: 0 if v <= lo else (255 if v >= hi else
                                     int(255 * (v - lo) / float(hi - lo))))
    combined = combined.filter(ImageFilter.GaussianBlur(2))

    combined.save(OUT)
    print('lane mask -> {} ({}x{}, path half-width {:.0f}uu)'.format(
        OUT, RES_X, RES_Y, PATH_HALF_WIDTH))


if __name__ == '__main__':
    main()
