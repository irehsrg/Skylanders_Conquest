# Offline top-down preview of M_Ground, so the lane can be art-directed without
# relaunching the game for every tweak.
#
# Mirrors the material built by BuildLaneGround.py:
#     col = lerp(grass, cobble, m) * (1 - 4m(1-m)*RIM_DARKEN)
# Keep these constants in sync with that script. The three-sample grass/dirt/
# cobble version this once previewed is NOT what ships -- it rendered clean here
# but produced screen-space seams in-engine, so the material was simplified.
# A preview that does not match the material is worse than no preview.
#
#   python Scripts/PreviewLaneGround.py [out.png]

import os
import sys

from PIL import Image, ImageChops

HERE = os.path.dirname(os.path.abspath(__file__))
TEXDIR = os.path.join(HERE, '..', 'Saved', 'GroundTex')
MASK = os.path.join(HERE, '..', 'Saved', 'LaneMask', 'LaneMask.png')

MAP_W, MAP_H = 27000.0, 10500.0
RES_X, RES_Y = 1600, 622

GRASS_TILE = 1400.0
DIRT_TILE = 800.0
COBBLE_TILE = 300.0

RIM_DARKEN = 0.30


def tiled(name, tile_uu):
    src = Image.open(os.path.join(TEXDIR, name)).convert('RGB')
    px = max(2, int(round(tile_uu / MAP_W * RES_X)))
    tile = src.resize((px, px), Image.LANCZOS)
    layer = Image.new('RGB', (RES_X, RES_Y))
    for y in range(0, RES_Y, px):
        for x in range(0, RES_X, px):
            layer.paste(tile, (x, y))
    return layer


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else os.path.join(
        HERE, '..', 'Saved', 'LaneMask', 'preview.png')

    mask = Image.open(MASK).convert('L').resize((RES_X, RES_Y), Image.LANCZOS)
    grass = tiled('Grass_C.png', GRASS_TILE)
    cobble = tiled('Cobble_C.png', COBBLE_TILE)

    col = Image.composite(cobble, grass, mask)

    # rim = 4m(1-m): a trodden shoulder where the stone meets the grass
    rim = mask.point(lambda v: int(255.0 * 4.0 * (v / 255.0) * (1.0 - v / 255.0)))
    keep = rim.point(lambda v: int(255 - v * RIM_DARKEN))
    col = ImageChops.multiply(col, Image.merge('RGB', (keep, keep, keep)))

    col.save(out)
    print('preview -> ' + out)


if __name__ == '__main__':
    main()
