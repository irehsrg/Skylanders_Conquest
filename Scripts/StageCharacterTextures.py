# Stages Skylanders rip textures for any list of characters.
#
# The rip stores textures under mangled Unity names like
#   textpngColorMap,textures@!actors!Skylanders!Cynder!Cynder_C`tga,111.png
# so this searches by character name and picks the LARGEST matching "_C" map as
# the body colour (largest is reliably the main body atlas, not eyes/props),
# then converts the palettized PNG to straight RGBA.
#
#   python Scripts/StageCharacterTextures.py Terrabite Drobit BullTrain ...
#
# Pass "name=pattern" when the texture folder is named differently from the
# model, e.g.  StealthElf=DriverStealthElfBody  KaosSensei=S627_Sensei_Kaos

import os
import sys

from PIL import Image

REPO = r"C:/Users/ajoin/Downloads/Skylanders-Models"
TEX = REPO + "/Assets/Skylanders/Textures"
STAGING = REPO + "/Staging"


def candidates(folder, pattern, suffix):
    """Non-meta files under folder matching pattern and a '<suffix>`' marker."""
    if not os.path.isdir(folder):
        return []
    pat, suf = pattern.lower(), (suffix + '`').lower()
    out = []
    for f in os.listdir(folder):
        low = f.lower()
        if low.endswith('.meta') or pat not in low or suf not in low:
            continue
        full = os.path.join(folder, f)
        try:
            out.append((os.path.getsize(full), full))
        except OSError:
            pass
    out.sort(reverse=True)  # largest first == main body atlas
    return out


def convert(src, dst):
    with Image.open(src) as im:
        im.convert('RGBA').save(dst)


def stage(name, pattern):
    out_dir = os.path.join(STAGING, name)
    os.makedirs(out_dir, exist_ok=True)

    colors = candidates(os.path.join(TEX, 'Color'), pattern, '_C')
    if not colors:
        print('{:<14} NO COLOUR TEXTURE for pattern "{}"'.format(name, pattern))
        return False
    src = colors[0][1]
    convert(src, os.path.join(out_dir, name + '_C.png'))
    print('{:<14} C <- {:<52} ({} candidates)'.format(
        name, os.path.basename(src)[:52], len(colors)))

    normals = candidates(os.path.join(TEX, 'Normals'), pattern, '_N')
    if normals:
        convert(normals[0][1], os.path.join(out_dir, name + '_N.png'))
        print('{:<14} N <- {}'.format(name, os.path.basename(normals[0][1])[:52]))
    return True


def main():
    args = sys.argv[1:]
    if not args:
        print(__doc__)
        return 2
    ok = 0
    for arg in args:
        name, _, pattern = arg.partition('=')
        if stage(name, pattern or name):
            ok += 1
    print('\nStaged {}/{}'.format(ok, len(args)))
    return 0 if ok == len(args) else 1


if __name__ == '__main__':
    sys.exit(main())
