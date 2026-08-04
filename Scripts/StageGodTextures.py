# Stages Skylanders rip textures for the god roster.
#
# The rip stores textures under mangled Unity names like
#   textpngColorMap,textures@!actors!Skylanders!Cynder!Cynder_C`tga,111.png
# and many are palettized PNGs. This finds the right ones per character,
# converts them to straight RGBA, and writes Staging/<Name>/<Name>_C.png (+_N).
#
# Plain python (no editor):  python Scripts/StageGodTextures.py

import os
import sys

from PIL import Image

REPO = r"C:/Users/ajoin/Downloads/Skylanders-Models"
TEX = REPO + "/Assets/Skylanders/Textures"
STAGING = REPO + "/Staging"

# (staged name, color-texture substring, normal-texture substring)
CHARACTERS = [
    ("Spyro",       "Spyro!SpyroNew_C",                          "Spyro!SpyroNew_N"),
    ("StealthElf",  "DriverStealthElf!DriverStealthElfBody_C",   "DriverStealthElf!DriverStealthElfBody_N"),
    ("KaosSensei",  "S627_Sensei_Kaos!KaosSensei_C",             "S627_Sensei_Kaos!KaosSensei_N"),
    ("ChompyMage",  "ChompyMage!chompymageToy_C_C",              "ChompyMage!chompymageToy_C_N"),
    ("Cynder",      "Cynder!Cynder_C",                           "Cynder!Cynder_N"),
]


def find(folder, needle):
    """First non-.meta file in folder whose name contains needle (case-insensitive)."""
    if not os.path.isdir(folder):
        return None
    low = needle.lower()
    for f in sorted(os.listdir(folder)):
        if f.lower().endswith('.meta'):
            continue
        if low in f.lower():
            return os.path.join(folder, f)
    return None


def convert(src, dst):
    """Palettized/odd-mode rip PNG -> straight RGBA PNG."""
    with Image.open(src) as im:
        im.convert('RGBA').save(dst)
    return os.path.getsize(dst)


def main():
    made, missing = 0, []
    for name, color_pat, normal_pat in CHARACTERS:
        out_dir = os.path.join(STAGING, name)
        os.makedirs(out_dir, exist_ok=True)

        c_src = find(os.path.join(TEX, 'Color'), color_pat)
        if c_src:
            size = convert(c_src, os.path.join(out_dir, name + '_C.png'))
            print('{:<12} C  <- {}  ({} bytes)'.format(name, os.path.basename(c_src)[:60], size))
            made += 1
        else:
            missing.append('{} color ({})'.format(name, color_pat))

        n_src = find(os.path.join(TEX, 'Normals'), normal_pat)
        if n_src:
            convert(n_src, os.path.join(out_dir, name + '_N.png'))
            print('{:<12} N  <- {}'.format(name, os.path.basename(n_src)[:60]))
        else:
            print('{:<12} N  -- none (fine, material just skips the normal)'.format(name))

    print('\nStaged colour maps: {}/{}'.format(made, len(CHARACTERS)))
    if missing:
        print('MISSING:')
        for m in missing:
            print('  ' + m)
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
